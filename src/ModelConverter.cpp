//
//  ModelConverter.cpp
//  Parses a power system XML configuration and generates a .dmodl
//  file string suitable for the dTwin DAE solver.
//
//  DFIG generators use Federico Milano's dq-frame model.
//  Standard generators use the classical swing-equation model.
//
//  Threading: convert() runs on a worker std::thread.  The caller
//  wraps ProgressCallback in gui::thread::asyncExecInMainThread
//  so that GUI updates happen on the main thread.
//
//  Created for Vjetroelektrana DFIG plugin.
//

#include "ModelConverter.h"

#include <xml/DOMParser.h>
#include <fo/FileOperations.h>
#include <td/StringUtils.h>
#include <td/MutableString.h>
#include <mu/ScopedCLocale.h>

#include <cmath>
#include <fstream>
#include <cstring>

// ═════════════════════════════════════════════════════════════════════
//  Utility: parse comma-separated generator IDs
// ═════════════════════════════════════════════════════════════════════
void ModelConverter::parseDFIGIDs(const td::String& csv,
                                  cnt::PushBackVector<td::INT4>& ids)
{
    if (csv.length() == 0)
        return;

    cnt::PushBackVector<td::String> tokens;
    tokens.reserve(16);
    td::String csvCopy(csv);
    csvCopy.split(",; ", tokens);

    for (td::UINT4 i = 0; i < tokens.size(); ++i)
    {
        const td::String& tok = tokens[i];
        if (tok.length() == 0)
            continue;
        td::INT4 id = std::atoi(tok.c_str());
        if (id > 0)
            ids.push_back(id);
    }
}

// ═════════════════════════════════════════════════════════════════════
//  Stage 2: Assign DFIG flags to selected generators
// ═════════════════════════════════════════════════════════════════════
void ModelConverter::assignDFIGGenerators(PowerSystem& ps,
                                          const td::String& dfigIDs,
                                          const DFIGParams& dfigDefaults,
                                          const StdGenParams& stdDefaults)
{
    cnt::PushBackVector<td::INT4> ids;
    ids.reserve(16);
    parseDFIGIDs(dfigIDs, ids);

    for (td::UINT4 g = 0; g < ps.generators.size(); ++g)
    {
        GeneratorData& gen = ps.generators[g];
        gen.isDFIG = false;

        for (td::UINT4 k = 0; k < ids.size(); ++k)
        {
            if (gen.number == ids[k])
            {
                gen.isDFIG = true;
                // Apply default DFIG params if not overridden in XML
                if (gen.dfig.Htotal == 0.0)
                    gen.dfig = dfigDefaults;
                break;
            }
        }

        if (!gen.isDFIG)
        {
            // Apply default standard gen params
            if (gen.std.H == 0.0)
                gen.std = stdDefaults;
        }
    }
}

// ═════════════════════════════════════════════════════════════════════
//  Stage 10: Build natID matrices (sparse Y-bus, dense gen params)
//  Uses sparse::ICmplxMatrix for Y-bus and dense::DblMatrix for
//  generator parameters and bus voltages as required by spec.
// ═════════════════════════════════════════════════════════════════════
void ModelConverter::buildMatrices(PowerSystem& ps,
                                   const ProgressCallback& onProgress)
{
    onProgress(0.17, "Building Y-bus admittance matrix (sparse)...");

    td::INT4 nBus = (td::INT4)ps.buses.size();
    td::INT4 nBranch = (td::INT4)ps.branches.size();

    // Build bus number → index mapping
    cnt::PushBackVector<td::INT4> busMap;
    busMap.reserve(nBus);
    td::INT4 maxBusNum = 0;
    for (td::UINT4 i = 0; i < ps.buses.size(); ++i)
    {
        if (ps.buses[i].number > maxBusNum)
            maxBusNum = ps.buses[i].number;
    }
    // Simple mapping: index = busNumber - 1 (assuming sequential)
    // For non-sequential, we use a lookup array
    cnt::PushBackVector<td::INT4> busNumToIdx;
    busNumToIdx.reserve(maxBusNum + 1);
    for (td::INT4 i = 0; i <= maxBusNum; ++i)
        busNumToIdx.push_back(-1);
    for (td::UINT4 i = 0; i < ps.buses.size(); ++i)
        busNumToIdx[ps.buses[i].number] = (td::INT4)i;

    // Estimate non-zeros: diagonal (N) + 2 per branch (off-diag)
    int nNZ = nBus + 2 * nBranch;
    ps.pYbus = sparse::createCmplxMatrix(nBus, nBus, nNZ,
                                          sparse::Symmetry::NonSymmetric);
    ps.ybusReleaser = ps.pYbus;

    // Add branch admittances
    for (td::UINT4 k = 0; k < ps.branches.size(); ++k)
    {
        const BranchData& br = ps.branches[k];
        td::INT4 i = busNumToIdx[br.fromBus];
        td::INT4 j = busNumToIdx[br.toBus];
        if (i < 0 || j < 0) continue;

        double rr = br.r;
        double xx = br.x;
        double denom = rr * rr + xx * xx;
        if (denom < 1e-12) denom = 1e-12; // avoid division by zero

        // Series admittance y_ij = 1/(r + jx)
        std::complex<double> yij(rr / denom, -xx / denom);

        // Line charging susceptance (half on each side)
        std::complex<double> yshunt(0.0, br.b / 2.0);

        // Tap ratio handling
        double tapRatio = (br.tap > 0.0) ? br.tap : 1.0;

        // Y-bus elements (pi-model with tap)
        // Y[i][i] += yij / tap^2 + yshunt
        std::complex<double> yii_add = yij / (tapRatio * tapRatio) + yshunt;
        ps.pYbus->addTriple1(i + 1, i + 1, yii_add);

        // Y[j][j] += yij + yshunt
        std::complex<double> yjj_add = yij + yshunt;
        ps.pYbus->addTriple1(j + 1, j + 1, yjj_add);

        // Y[i][j] -= yij / tap
        std::complex<double> yij_off = -yij / tapRatio;
        ps.pYbus->addTriple1(i + 1, j + 1, yij_off);

        // Y[j][i] -= yij / tap
        ps.pYbus->addTriple1(j + 1, i + 1, yij_off);
    }

    // Add bus shunt admittances (Gs + jBs) to diagonal
    for (td::UINT4 i = 0; i < ps.buses.size(); ++i)
    {
        const BusData& bus = ps.buses[i];
        if (bus.Gs != 0.0 || bus.Bs != 0.0)
        {
            std::complex<double> yshuntBus(bus.Gs / ps.baseMVA,
                                           bus.Bs / ps.baseMVA);
            ps.pYbus->addTriple1((td::INT4)i + 1, (td::INT4)i + 1, yshuntBus);
        }
    }

    onProgress(0.18, "Building bus voltage matrix (dense)...");

    // ── Dense bus voltage matrix [N x 2]: col0=Vm, col1=Va ──────────
    ps.busVoltages.reserve((td::UINT4)nBus, 2, nullptr, true);
    {
        auto mio = ps.busVoltages.getManipulator();
        for (td::UINT4 i = 0; i < ps.buses.size(); ++i)
        {
            mio((td::UINT4)i, 0) = ps.buses[i].Vm;
            mio((td::UINT4)i, 1) = ps.buses[i].Va;
        }
    }

    onProgress(0.19, "Building generator parameter matrix (dense)...");

    // ── Dense gen params [nGen x 8] ─────────────────────────────────
    // Columns: Pg, Qg, Vs, mBase, H, xPrime, isDFIG, busIdx
    td::UINT4 nGen = (td::UINT4)ps.generators.size();
    ps.genParams.reserve(nGen, 8, nullptr, true);
    {
        auto mio = ps.genParams.getManipulator();
        for (td::UINT4 i = 0; i < nGen; ++i)
        {
            const auto& g = ps.generators[i];
            mio(i, 0) = g.Pg / ps.baseMVA;          // Pg [p.u.]
            mio(i, 1) = g.Qg / ps.baseMVA;          // Qg [p.u.]
            mio(i, 2) = g.Vs;                        // Vs [p.u.]
            mio(i, 3) = g.mBase;                     // Machine base [MVA]
            mio(i, 4) = g.isDFIG ? g.dfig.Htotal : g.std.H;   // Inertia
            mio(i, 5) = g.isDFIG ? g.dfig.xs : g.std.xd_prime;// Reactance
            mio(i, 6) = g.isDFIG ? 1.0 : 0.0;        // isDFIG flag
            td::INT4 bIdx = busNumToIdx[g.bus];
            mio(i, 7) = (double)bIdx;                 // Bus index
        }
    }

    onProgress(0.20, "Matrix construction complete.");
}

// ═════════════════════════════════════════════════════════════════════
//  Stage 1: Parse XML configuration into PowerSystem struct
// ═════════════════════════════════════════════════════════════════════
bool ModelConverter::parseXML(const td::String& xmlPath,
                              PowerSystem& ps,
                              const ProgressCallback& onProgress)
{
    onProgress(0.02, "Opening XML file...");

    xml::FileParser parser;
    if (!parser.parseFile(xmlPath))
    {
        onProgress(0.05, "ERROR: Failed to parse XML file!");
        return false;
    }

    auto root = parser.getRootNode();
    if (root.end())
    {
        onProgress(0.05, "ERROR: Empty XML document!");
        return false;
    }

    // ── Root: <PowerSystem name="..." baseMVA="..."> ────────────────
    {
        td::String name;
        if (root.getAttribValue("name", name))
            ps.name = name;
        root.getAttribValue("baseMVA", ps.baseMVA);
    }

    onProgress(0.05, "Parsing bus data...");

    // ── <Buses> ─────────────────────────────────────────────────────
    auto busesNode = root.getChildNode("Buses");
    if (busesNode.isOk())
    {
        auto busIt = busesNode.getChildNode("Bus");
        while (busIt.isOk())
        {
            BusData& b = ps.buses.push_back();
            busIt.getAttribValue("number", b.number);
            busIt.getAttribValue("type",   b.type);
            busIt.getAttribValue("Pd",     b.Pd);
            busIt.getAttribValue("Qd",     b.Qd);
            busIt.getAttribValue("Gs",     b.Gs);
            busIt.getAttribValue("Bs",     b.Bs);
            busIt.getAttribValue("baseKV", b.baseKV);
            busIt.getAttribValue("Vm",     b.Vm);
            busIt.getAttribValue("Va",     b.Va);
            ++busIt;
        }
    }

    onProgress(0.08, "Parsing branch data...");

    // ── <Branches> ──────────────────────────────────────────────────
    auto branchesNode = root.getChildNode("Branches");
    if (branchesNode.isOk())
    {
        auto brIt = branchesNode.getChildNode("Branch");
        while (brIt.isOk())
        {
            BranchData& br = ps.branches.push_back();
            brIt.getAttribValue("from",  br.fromBus);
            brIt.getAttribValue("to",    br.toBus);
            brIt.getAttribValue("r",     br.r);
            brIt.getAttribValue("x",     br.x);
            brIt.getAttribValue("b",     br.b);
            brIt.getAttribValue("rateA", br.rateA);
            brIt.getAttribValue("tap",   br.tap);
            brIt.getAttribValue("shift", br.shift);
            ++brIt;
        }
    }

    onProgress(0.10, "Parsing generator data...");

    // ── <Generators> ────────────────────────────────────────────────
    auto gensNode = root.getChildNode("Generators");
    if (gensNode.isOk())
    {
        auto genIt = gensNode.getChildNode("Gen");
        while (genIt.isOk())
        {
            GeneratorData& g = ps.generators.push_back();
            genIt.getAttribValue("bus",    g.bus);
            genIt.getAttribValue("number", g.number);
            genIt.getAttribValue("Pg",     g.Pg);
            genIt.getAttribValue("Qg",     g.Qg);
            genIt.getAttribValue("Qmax",   g.Qmax);
            genIt.getAttribValue("Qmin",   g.Qmin);
            genIt.getAttribValue("Vs",     g.Vs);
            genIt.getAttribValue("mBase",  g.mBase);

            // Per-generator DFIG override (optional in XML)
            auto dfigChild = genIt.getChildNode("DFIG");
            if (dfigChild.isOk())
            {
                dfigChild.getAttribValue("xs",      g.dfig.xs);
                dfigChild.getAttribValue("xmu",     g.dfig.xmu);
                dfigChild.getAttribValue("Htotal",  g.dfig.Htotal);
                dfigChild.getAttribValue("Teps",    g.dfig.Teps);
                dfigChild.getAttribValue("KV",      g.dfig.KV);
                dfigChild.getAttribValue("vref",    g.dfig.vref);
                dfigChild.getAttribValue("Popt",    g.dfig.Popt);
                dfigChild.getAttribValue("OmegaB",  g.dfig.OmegaB);
                dfigChild.getAttribValue("omega_m0",g.dfig.omega_m0);
                dfigChild.getAttribValue("i_rq0",   g.dfig.i_rq0);
                dfigChild.getAttribValue("i_rd0",   g.dfig.i_rd0);
            }

            // Per-generator standard override (optional)
            auto stdChild = genIt.getChildNode("StdGen");
            if (stdChild.isOk())
            {
                stdChild.getAttribValue("xd_prime", g.std.xd_prime);
                stdChild.getAttribValue("H",        g.std.H);
                stdChild.getAttribValue("D",        g.std.D);
                stdChild.getAttribValue("OmegaB",   g.std.OmegaB);
                stdChild.getAttribValue("E_prime",  g.std.E_prime);
                stdChild.getAttribValue("delta0",   g.std.delta0);
                stdChild.getAttribValue("omega0",   g.std.omega0);
            }
            ++genIt;
        }
    }

    onProgress(0.12, "Parsing default DFIG parameters...");

    // ── <DFIGDefaults> ──────────────────────────────────────────────
    auto dfigDef = root.getChildNode("DFIGDefaults");
    if (dfigDef.isOk())
    {
        dfigDef.getAttribValue("xs",       ps.defaultDFIG.xs);
        dfigDef.getAttribValue("xmu",      ps.defaultDFIG.xmu);
        dfigDef.getAttribValue("Htotal",   ps.defaultDFIG.Htotal);
        dfigDef.getAttribValue("Teps",     ps.defaultDFIG.Teps);
        dfigDef.getAttribValue("KV",       ps.defaultDFIG.KV);
        dfigDef.getAttribValue("vref",     ps.defaultDFIG.vref);
        dfigDef.getAttribValue("Popt",     ps.defaultDFIG.Popt);
        dfigDef.getAttribValue("OmegaB",   ps.defaultDFIG.OmegaB);
        dfigDef.getAttribValue("omega_m0", ps.defaultDFIG.omega_m0);
        dfigDef.getAttribValue("i_rq0",    ps.defaultDFIG.i_rq0);
        dfigDef.getAttribValue("i_rd0",    ps.defaultDFIG.i_rd0);
    }

    // ── <StdGenDefaults> ────────────────────────────────────────────
    auto stdDef = root.getChildNode("StdGenDefaults");
    if (stdDef.isOk())
    {
        stdDef.getAttribValue("xd_prime", ps.defaultStdGen.xd_prime);
        stdDef.getAttribValue("H",        ps.defaultStdGen.H);
        stdDef.getAttribValue("D",        ps.defaultStdGen.D);
        stdDef.getAttribValue("OmegaB",   ps.defaultStdGen.OmegaB);
        stdDef.getAttribValue("E_prime",  ps.defaultStdGen.E_prime);
        stdDef.getAttribValue("delta0",   ps.defaultStdGen.delta0);
        stdDef.getAttribValue("omega0",   ps.defaultStdGen.omega0);
    }

    onProgress(0.15, "XML parsing complete.");
    return true;
}

// ═════════════════════════════════════════════════════════════════════
//  Stage 3: Write model header
// ═════════════════════════════════════════════════════════════════════
void ModelConverter::writeHeader(arch::MemoryOut& out,
                                 const ConvertOptions& opts,
                                 td::MutableString& buf)
{
    buf.reserve(2048);
    buf.appendFormat(
        "Header:\n"
        "\tmaxIter = %d\n"
        "\treport = Solved\n"
        "\tmaxReps = -1\n"
        "\toutToTxt = false\n"
        "\ttxtFile = \"\"\n"
        "\tstartTime = 0\n"
        "\tdTime = %.6f\n"
        "\tendTime = %.3f\n"
        "end\n"
        "//Model generated by Vjetroelektrana DFIG plugin converter\n\n",
        opts.maxIter, opts.dTime, opts.endTime);
    out.put(buf.c_str(), buf.length());
    buf.reset();
}

// ═════════════════════════════════════════════════════════════════════
//  Stage 4: Write variable declarations
//
//  For each standard gen g:  delta_g, omega_g
//  For each DFIG gen g:      omega_m_g, i_rq_g, i_rd_g
//  Plus algebraic:           tau_e_g (DFIG), P_e_g (StdGen)
//  Plus network:             v_h_g (bus voltage at gen bus)
// ═════════════════════════════════════════════════════════════════════
void ModelConverter::writeVariables(arch::MemoryOut& out,
                                    const PowerSystem& ps,
                                    td::MutableString& buf)
{
    td::String modelName("Vjetroelektrana DFIG Power System");
    buf.reserve(4096);
    buf.appendFormat("Model [type=DAE domain=real method=RK2 name=\"%s\"]:\n",
                     modelName.c_str());
    buf.append("Vars [out=true]:\n\t");
    out.put(buf.c_str(), buf.length());
    buf.reset();

    // State + algebraic variables for each generator
    for (td::UINT4 i = 0; i < ps.generators.size(); ++i)
    {
        const auto& g = ps.generators[i];
        int id = g.number;

        if (g.isDFIG)
        {
            buf.appendFormat("omega_m_%d; i_rq_%d; i_rd_%d; tau_e_%d; ",
                             id, id, id, id);
            buf.appendFormat("P_h_%d; Q_h_%d; ", id, id);
        }
        else
        {
            buf.appendFormat("delta_%d; omega_%d; P_e_%d; ", id, id, id);
        }
    }
    buf.append("\n");
    out.put(buf.c_str(), buf.length());
    buf.reset();
}

// ═════════════════════════════════════════════════════════════════════
//  Stage 5: Write parameter values
// ═════════════════════════════════════════════════════════════════════
void ModelConverter::writeParameters(arch::MemoryOut& out,
                                     const PowerSystem& ps,
                                     td::MutableString& buf)
{
    buf.reserve(8192);
    buf.append("Params:\n");

    // ── Global ──────────────────────────────────────────────────────
    buf.appendFormat("\tbaseMVA = %.1f\n", ps.baseMVA);

    // ── Per-generator parameters ────────────────────────────────────
    for (td::UINT4 i = 0; i < ps.generators.size(); ++i)
    {
        const auto& g = ps.generators[i];
        int id = g.number;

        buf.appendFormat("\t// Generator %d (bus %d) — %s\n",
                         id, g.bus, g.isDFIG ? "DFIG" : "Standard");

        buf.appendFormat("\tPg_%d = %.6f; Vs_%d = %.6f; mBase_%d = %.1f\n",
                         id, g.Pg / ps.baseMVA, id, g.Vs, id, g.mBase);

        if (g.isDFIG)
        {
            const auto& d = g.dfig;
            buf.appendFormat("\txs_%d = %.6f; xmu_%d = %.6f\n",
                             id, d.xs, id, d.xmu);
            buf.appendFormat("\tH_total_%d = %.6f; T_eps_%d = %.6f\n",
                             id, d.Htotal, id, d.Teps);
            buf.appendFormat("\tK_V_%d = %.6f; v_ref_%d = %.6f\n",
                             id, d.KV, id, d.vref);
            buf.appendFormat("\tP_opt_%d = %.6f; Omega_b_%d = %.6f\n",
                             id, d.Popt, id, d.OmegaB);
            // Initial conditions
            buf.appendFormat("\tomega_m_%d = %.6f\t[init=true]\n", id, d.omega_m0);
            buf.appendFormat("\ti_rq_%d = %.6f\t[init=true]\n",   id, d.i_rq0);
            buf.appendFormat("\ti_rd_%d = %.6f\t[init=true]\n",   id, d.i_rd0);
            // Bus voltage magnitude at generator bus (from flat start)
            double vh = g.Vs;
            buf.appendFormat("\tv_h_%d = %.6f\n", id, vh);
        }
        else
        {
            const auto& s = g.std;
            buf.appendFormat("\txd_prime_%d = %.6f; H_%d = %.6f; D_%d = %.6f\n",
                             id, s.xd_prime, id, s.H, id, s.D);
            buf.appendFormat("\tOmega_b_%d = %.6f; E_prime_%d = %.6f\n",
                             id, s.OmegaB, id, s.E_prime);
            // P_m equals initial P in p.u.
            buf.appendFormat("\tP_m_%d = %.6f\n", id, g.Pg / ps.baseMVA);
            // Bus voltage at generator bus
            double vm = g.Vs;
            buf.appendFormat("\tVm_%d = %.6f\n", id, vm);
            // Initial angle from power flow: delta0 ≈ asin(P_e * xd' / (E' * V))
            double sinD = (g.Pg / ps.baseMVA) * s.xd_prime / (s.E_prime * vm);
            if (sinD > 1.0) sinD = 1.0;
            if (sinD < -1.0) sinD = -1.0;
            double d0 = std::asin(sinD);
            buf.appendFormat("\tdelta_%d = %.6f\t[init=true]\n", id, d0);
            buf.appendFormat("\tomega_%d = %.6f\t[init=true]\n", id, s.omega0);
        }
        buf.append("\n");
    }

    out.put(buf.c_str(), buf.length());
    buf.reset();
}

// ═════════════════════════════════════════════════════════════════════
//  Stage 6: Standard generator ODEs (classical swing equation)
//
//    d(delta_g)/dt = Omega_b * (omega_g - 1)
//    d(omega_g)/dt = (P_m_g - P_e_g - D_g*(omega_g - 1)) / (2*H_g)
//    P_e_g = (E'_g * Vm_g / xd'_g) * sin(delta_g)
// ═════════════════════════════════════════════════════════════════════
void ModelConverter::writeStdGenEquations(arch::MemoryOut& out,
                                          const PowerSystem& ps,
                                          td::MutableString& buf)
{
    bool hasAny = false;
    for (td::UINT4 i = 0; i < ps.generators.size(); ++i)
    {
        if (!ps.generators[i].isDFIG)
        {
            hasAny = true;
            break;
        }
    }
    if (!hasAny)
        return;

    buf.reserve(4096);
    buf.append("\t// ── Standard synchronous generator equations ──\n");

    for (td::UINT4 i = 0; i < ps.generators.size(); ++i)
    {
        const auto& g = ps.generators[i];
        if (g.isDFIG)
            continue;

        int id = g.number;

        // Algebraic: electrical power
        buf.appendFormat(
            "\tP_e_%d = (E_prime_%d * Vm_%d / xd_prime_%d) * sin(delta_%d)\n",
            id, id, id, id, id);

        // ODE: rotor angle
        buf.appendFormat(
            "\tdelta_%d' = Omega_b_%d * (omega_%d - 1)\n",
            id, id, id);

        // ODE: rotor speed
        buf.appendFormat(
            "\tomega_%d' = (P_m_%d - P_e_%d - D_%d * (omega_%d - 1)) / (2 * H_%d)\n",
            id, id, id, id, id, id);

        buf.append("\n");
    }

    out.put(buf.c_str(), buf.length());
    buf.reset();
}

// ═════════════════════════════════════════════════════════════════════
//  Stage 7: DFIG generator ODEs (Federico Milano's model)
//
//  State: omega_m, i_rq, i_rd
//  ODEs:
//    d(omega_m)/dt = (tau_t - tau_e) / (2*H_total)
//    d(i_rq)/dt    = (1/T_eps) * (-((xs+xmu)/(xmu*v_h)) *
//                                   (P_opt/omega_m) - i_rq)
//    d(i_rd)/dt    = K_V*(v_h - v_ref) - (v_h/xmu) - i_rd
//
//  Algebraic:
//    tau_e = -(xmu * v_h * i_rq) / (Omega_b * (xs + xmu))
//
//  Network injections:
//    P_h = -(xmu * v_h * i_rq) * Omega_b / (Omega_b * (xs + xmu))
//        (simplified to use tau_e * Omega_b for consistency)
//    Q_h = -((xmu * v_h * i_rd) / (xs + xmu)) - (v_h^2 / xmu)
// ═════════════════════════════════════════════════════════════════════
void ModelConverter::writeDFIGEquations(arch::MemoryOut& out,
                                        const PowerSystem& ps,
                                        td::MutableString& buf)
{
    bool hasAny = false;
    for (td::UINT4 i = 0; i < ps.generators.size(); ++i)
    {
        if (ps.generators[i].isDFIG)
        {
            hasAny = true;
            break;
        }
    }
    if (!hasAny)
        return;

    buf.reserve(8192);
    buf.append("\t// ── DFIG generator equations (Milano model) ──\n");

    for (td::UINT4 i = 0; i < ps.generators.size(); ++i)
    {
        const auto& g = ps.generators[i];
        if (!g.isDFIG)
            continue;

        int id = g.number;

        buf.appendFormat("\t// DFIG Generator %d at bus %d\n", id, g.bus);

        // ── Algebraic: electromagnetic torque ────────────────────────
        // tau_e = -(xmu * v_h * i_rq) / (Omega_b * (xs + xmu))
        buf.appendFormat(
            "\ttau_e_%d = -(xmu_%d * v_h_%d * i_rq_%d) / "
            "(Omega_b_%d * (xs_%d + xmu_%d))\n",
            id, id, id, id, id, id, id);

        // ── ODE 1: rotor speed ──────────────────────────────────────
        // d(omega_m)/dt = (tau_t - tau_e) / (2 * H_total)
        // tau_t = P_opt / omega_m  (optimal power tracking)
        buf.appendFormat(
            "\tomega_m_%d' = (P_opt_%d / omega_m_%d - tau_e_%d) / "
            "(2 * H_total_%d)\n",
            id, id, id, id, id);

        // ── ODE 2: active rotor current (i_rq) ─────────────────────
        // d(i_rq)/dt = (1/T_eps) * (-((xs+xmu)/(xmu*v_h)) *
        //              (P_opt/omega_m) - i_rq)
        buf.appendFormat(
            "\ti_rq_%d' = (1 / T_eps_%d) * "
            "(-((xs_%d + xmu_%d) / (xmu_%d * v_h_%d)) * "
            "(P_opt_%d / omega_m_%d) - i_rq_%d)\n",
            id, id, id, id, id, id, id, id, id);

        // ── ODE 3: reactive rotor current (i_rd) ────────────────────
        // d(i_rd)/dt = K_V * (v_h - v_ref) - (v_h / xmu) - i_rd
        buf.appendFormat(
            "\ti_rd_%d' = K_V_%d * (v_h_%d - v_ref_%d) - "
            "(v_h_%d / xmu_%d) - i_rd_%d\n",
            id, id, id, id, id, id, id);

        // ── Network injection: active power ─────────────────────────
        // P_h = tau_e * Omega_b  (total active power injected)
        // Using expanded form: P_h = -(xmu * v_h * i_rq) / (xs + xmu)
        buf.appendFormat(
            "\tP_h_%d = -(xmu_%d * v_h_%d * i_rq_%d) / "
            "(xs_%d + xmu_%d)\n",
            id, id, id, id, id, id);

        // ── Network injection: reactive power ───────────────────────
        // Q_h = -((xmu * v_h * i_rd) / (xs + xmu)) - (v_h^2 / xmu)
        buf.appendFormat(
            "\tQ_h_%d = -((xmu_%d * v_h_%d * i_rd_%d) / "
            "(xs_%d + xmu_%d)) - (v_h_%d * v_h_%d / xmu_%d)\n",
            id, id, id, id, id, id, id, id, id);

        buf.append("\n");
    }

    out.put(buf.c_str(), buf.length());
    buf.reset();
}

// ═════════════════════════════════════════════════════════════════════
//  Stage 8: PostProc section
// ═════════════════════════════════════════════════════════════════════
void ModelConverter::writePostProc(arch::MemoryOut& out,
                                   const PowerSystem& ps,
                                   td::MutableString& buf)
{
    buf.reserve(2048);
    buf.append("PostProc:\n");
    buf.append("\t// Post-processing calculations\n");

    // For DFIG gens: output active/reactive power in MW/MVAr
    for (td::UINT4 i = 0; i < ps.generators.size(); ++i)
    {
        const auto& g = ps.generators[i];
        int id = g.number;

        if (g.isDFIG)
        {
            buf.appendFormat(
                "\t// DFIG Gen %d: P_h, Q_h are in p.u. on machine base\n", id);
        }
        else
        {
            buf.appendFormat(
                "\t// Std Gen %d: P_e is in p.u. on system base\n", id);
        }
    }

    buf.append("end\n");
    out.put(buf.c_str(), buf.length());
    buf.reset();
}

// ═════════════════════════════════════════════════════════════════════
//  Stage 9: Write visual model (.vmodl)
// ═════════════════════════════════════════════════════════════════════
void ModelConverter::writeVisualModel(arch::MemoryOut& out,
                                      const PowerSystem& ps)
{
    out.put("Header:\n\tnewTab = false\n\tdrawPlots = true\nend\n");
    out.put("Model [name=\"Vjetroelektrana DFIG — Visualization\"]:\n");
    out.put("Plots [backColor=auto]:\n");

    // ── Plot 1: DFIG rotor speed ────────────────────────────────────
    bool hasDFIG = false;
    for (td::UINT4 i = 0; i < ps.generators.size(); ++i)
    {
        if (ps.generators[i].isDFIG)
        {
            hasDFIG = true;
            break;
        }
    }

    if (hasDFIG)
    {
        td::MutableString buf;
        buf.reserve(4096);

        // Rotor speed plot
        buf.append("\tlinePlot [xLabel=\"Time [s]\" yLabel=\"omega_m [p.u.]\" "
                   "name=\"DFIG Rotor Speed\" anchor=TR legend=true "
                   "nCols=1 anchorX=140 anchorY=35]:\n");
        buf.append("\t\t@x << t\n");

        static const char* colors[] = {
            "red", "green", "cyan", "magenta", "lightBlue",
            "darkYellow", "darkGreen", "darkBlue"
        };
        int ci = 0;

        for (td::UINT4 i = 0; i < ps.generators.size(); ++i)
        {
            const auto& g = ps.generators[i];
            if (!g.isDFIG) continue;
            buf.appendFormat(
                "\t\t@y << omega_m_%d [colorD=%s width=2 name=\"omega_m_%d\"]\n",
                g.number, colors[ci % 8], g.number);
            ++ci;
        }
        buf.append("\tend\n");

        // Active power injection plot
        buf.append("\tlinePlot [xLabel=\"Time [s]\" yLabel=\"P_h [p.u.]\" "
                   "name=\"DFIG Active Power\" anchor=TR legend=true "
                   "nCols=1 anchorX=140 anchorY=35]:\n");
        buf.append("\t\t@x << t\n");
        ci = 0;
        for (td::UINT4 i = 0; i < ps.generators.size(); ++i)
        {
            const auto& g = ps.generators[i];
            if (!g.isDFIG) continue;
            buf.appendFormat(
                "\t\t@y << P_h_%d [colorD=%s width=2 name=\"P_h_%d\"]\n",
                g.number, colors[ci % 8], g.number);
            ++ci;
        }
        buf.append("\tend\n");

        // Reactive power injection plot
        buf.append("\tlinePlot [xLabel=\"Time [s]\" yLabel=\"Q_h [p.u.]\" "
                   "name=\"DFIG Reactive Power\" anchor=TR legend=true "
                   "nCols=1 anchorX=140 anchorY=35]:\n");
        buf.append("\t\t@x << t\n");
        ci = 0;
        for (td::UINT4 i = 0; i < ps.generators.size(); ++i)
        {
            const auto& g = ps.generators[i];
            if (!g.isDFIG) continue;
            buf.appendFormat(
                "\t\t@y << Q_h_%d [colorD=%s width=2 name=\"Q_h_%d\"]\n",
                g.number, colors[ci % 8], g.number);
            ++ci;
        }
        buf.append("\tend\n");

        // Rotor currents plot
        buf.append("\tlinePlot [xLabel=\"Time [s]\" yLabel=\"I_rotor [p.u.]\" "
                   "name=\"DFIG Rotor Currents\" anchor=TR legend=true "
                   "nCols=1 anchorX=160 anchorY=35]:\n");
        buf.append("\t\t@x << t\n");
        ci = 0;
        for (td::UINT4 i = 0; i < ps.generators.size(); ++i)
        {
            const auto& g = ps.generators[i];
            if (!g.isDFIG) continue;
            buf.appendFormat(
                "\t\t@y << i_rq_%d [colorD=%s width=2 name=\"i_rq_%d\"]\n",
                g.number, colors[ci % 8], g.number);
            buf.appendFormat(
                "\t\t@y << i_rd_%d [colorD=%s width=1 name=\"i_rd_%d\"]\n",
                g.number, colors[(ci + 4) % 8], g.number);
            ++ci;
        }
        buf.append("\tend\n");

        out.put(buf.c_str(), buf.length());
    }

    // ── Plot for standard generators ────────────────────────────────
    bool hasStd = false;
    for (td::UINT4 i = 0; i < ps.generators.size(); ++i)
    {
        if (!ps.generators[i].isDFIG)
        {
            hasStd = true;
            break;
        }
    }

    if (hasStd)
    {
        td::MutableString buf;
        buf.reserve(2048);

        // Rotor angle plot
        buf.append("\tlinePlot [xLabel=\"Time [s]\" yLabel=\"delta [rad]\" "
                   "name=\"Std Gen Rotor Angle\" anchor=TR legend=true "
                   "nCols=1 anchorX=140 anchorY=35]:\n");
        buf.append("\t\t@x << t\n");

        static const char* colors2[] = {
            "red", "green", "cyan", "magenta", "lightBlue",
            "darkYellow", "darkGreen", "darkBlue"
        };
        int ci = 0;
        for (td::UINT4 i = 0; i < ps.generators.size(); ++i)
        {
            const auto& g = ps.generators[i];
            if (g.isDFIG) continue;
            buf.appendFormat(
                "\t\t@y << delta_%d [colorD=%s width=2 name=\"delta_%d\"]\n",
                g.number, colors2[ci % 8], g.number);
            ++ci;
        }
        buf.append("\tend\n");

        // Rotor speed plot
        buf.append("\tlinePlot [xLabel=\"Time [s]\" yLabel=\"omega [p.u.]\" "
                   "name=\"Std Gen Rotor Speed\" anchor=TR legend=true "
                   "nCols=1 anchorX=140 anchorY=35]:\n");
        buf.append("\t\t@x << t\n");
        ci = 0;
        for (td::UINT4 i = 0; i < ps.generators.size(); ++i)
        {
            const auto& g = ps.generators[i];
            if (g.isDFIG) continue;
            buf.appendFormat(
                "\t\t@y << omega_%d [colorD=%s width=2 name=\"omega_%d\"]\n",
                g.number, colors2[ci % 8], g.number);
            ++ci;
        }
        buf.append("\tend\n");

        out.put(buf.c_str(), buf.length());
    }

    out.put("end //end of the visual model\n");
}

// ═════════════════════════════════════════════════════════════════════
//  Main entry point: convert()
//
//  Called from a worker std::thread.  The caller wraps onProgress
//  in gui::thread::asyncExecInMainThread for safe GUI updates.
// ═════════════════════════════════════════════════════════════════════
bool ModelConverter::convert(const td::String&      inputXMLPath,
                             const td::String&      outDmodlPath,
                             const ConvertOptions&  options,
                             arch::MemoryOut&       memDigitalOut,
                             arch::MemoryOut*       memVisualOut,
                             const ProgressCallback& onProgress)
{
    // Force "C" locale for correct floating-point formatting
    mu::ScopedCLocale scopedLocale;

    // ── Stage 1: Parse XML ──────────────────────────────────────────
    PowerSystem ps;
    ps.defaultDFIG   = options.dfigDefaults;
    ps.defaultStdGen = options.stdGenDefaults;

    if (!parseXML(inputXMLPath, ps, onProgress))
        return false;

    onProgress(0.15, "Assigning DFIG generators...");

    // ── Stage 2: Assign DFIG generators ─────────────────────────────
    assignDFIGGenerators(ps, options.dfigGeneratorIDs,
                         options.dfigDefaults, options.stdGenDefaults);

    // ── Stage 10: Build natID matrices (Y-bus sparse, gen dense) ────
    buildMatrices(ps, onProgress);

    // Count generators for progress reporting
    int nDFIG = 0, nStd = 0;
    for (td::UINT4 i = 0; i < ps.generators.size(); ++i)
    {
        if (ps.generators[i].isDFIG) ++nDFIG;
        else                          ++nStd;
    }

    td::MutableString statusBuf;
    statusBuf.reserve(256);
    statusBuf.appendFormat("Found %d buses, %d branches, %d generators "
                           "(%d DFIG, %d Standard). Y-bus: %dx%d sparse.",
                           (int)ps.buses.size(), (int)ps.branches.size(),
                           (int)ps.generators.size(), nDFIG, nStd,
                           (int)ps.buses.size(), (int)ps.buses.size());
    onProgress(0.22, statusBuf.c_str());
    statusBuf.reset();

    // ── Stage 3: Write Header ───────────────────────────────────────
    onProgress(0.25, "Writing model header...");
    td::MutableString buf;
    writeHeader(memDigitalOut, options, buf);

    // ── Stage 4: Write Variables ────────────────────────────────────
    onProgress(0.35, "Writing variable declarations...");
    writeVariables(memDigitalOut, ps, buf);

    // ── Stage 5: Write Parameters ───────────────────────────────────
    onProgress(0.45, "Writing parameter values...");
    writeParameters(memDigitalOut, ps, buf);

    // ── Stage 6 & 7: Write Equations ────────────────────────────────
    memDigitalOut.put("ODEs:\n");

    onProgress(0.55, "Writing standard generator equations...");
    writeStdGenEquations(memDigitalOut, ps, buf);

    onProgress(0.65, "Writing DFIG generator equations...");
    writeDFIGEquations(memDigitalOut, ps, buf);

    // ── Stage 8: PostProc ───────────────────────────────────────────
    onProgress(0.75, "Writing post-processing section...");
    writePostProc(memDigitalOut, ps, buf);

    // ── Write .dmodl to disk ────────────────────────────────────────
    onProgress(0.80, "Saving digital model file...");
    {
        std::ofstream fDigital;
        if (!fo::createTextFile(fDigital, outDmodlPath))
        {
            onProgress(0.80, "ERROR: Cannot create output .dmodl file!");
            return false;
        }
        memDigitalOut.writeToFile(fDigital);
        fDigital.close();
    }

    // ── Stage 9: Visual model (.vmodl) ──────────────────────────────
    if (memVisualOut)
    {
        onProgress(0.85, "Generating visual model...");
        writeVisualModel(*memVisualOut, ps);

        td::String vmodlPath = fo::replaceFileExtension<false>(outDmodlPath, ".vmodl");
        std::ofstream fVisual;
        if (!fo::createTextFile(fVisual, vmodlPath))
        {
            onProgress(0.90, "WARNING: Cannot create .vmodl file.");
            // Non-fatal: continue
        }
        else
        {
            memVisualOut->writeToFile(fVisual);
            fVisual.close();
        }
    }

    onProgress(1.0, "Conversion complete!");
    return true;
}
