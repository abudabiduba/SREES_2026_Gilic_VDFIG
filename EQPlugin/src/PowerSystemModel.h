//
//  PowerSystemModel.h
//  Data structures for IEEE power system test cases and generator models.
//  DFIG parameters follow Federico Milano's model.
//
//  Uses natID dense::Matrix and sparse::IMatrix for all matrix data
//  as required by the project specification.
//
//  Created for Vjetroelektrana DFIG plugin.
//

#pragma once
#include <td/Types.h>
#include <td/String.h>
#include <cnt/PushBackVector.h>
#include <dense/Matrix.h>
#include <sparse/IMatrix.h>
#include <complex>

// ─────────────────────────────────────────────────────────────────────
//  Bus data (IEEE common data format)
// ─────────────────────────────────────────────────────────────────────
struct BusData
{
    td::INT4 number = 0;
    td::INT4 type   = 1;     // 1=PQ, 2=PV, 3=Slack
    double Pd       = 0.0;   // Active load [MW]
    double Qd       = 0.0;   // Reactive load [MVAr]
    double Gs       = 0.0;   // Shunt conductance [MW at V=1.0]
    double Bs       = 0.0;   // Shunt susceptance [MVAr at V=1.0]
    double baseKV   = 0.0;   // Base voltage [kV]
    double Vm       = 1.0;   // Voltage magnitude [p.u.]
    double Va       = 0.0;   // Voltage angle [deg]
};

// ─────────────────────────────────────────────────────────────────────
//  Branch data (pi-model)
// ─────────────────────────────────────────────────────────────────────
struct BranchData
{
    td::INT4 fromBus = 0;
    td::INT4 toBus   = 0;
    double r         = 0.0;  // Resistance [p.u.]
    double x         = 0.01; // Reactance  [p.u.]
    double b         = 0.0;  // Total line charging susceptance [p.u.]
    double rateA     = 0.0;  // MVA rating A (long-term)
    double tap       = 1.0;  // Transformer off-nominal tap ratio
    double shift     = 0.0;  // Transformer phase shift [deg]
};

// ─────────────────────────────────────────────────────────────────────
//  DFIG-specific parameters (Federico Milano's model)
//
//  State variables: omega_m, i_rq, i_rd
//  ODEs:
//    d(omega_m)/dt = (tau_t - tau_e) / (2*H_total)
//    d(i_rq)/dt    = (1/T_eps)*(-((x_s+x_mu)/(x_mu*v_h))*(P_opt/omega_m) - i_rq)
//    d(i_rd)/dt    = K_V*(v_h - v_ref) - (v_h/x_mu) - i_rd
//  Algebraic:
//    tau_e         = -(x_mu*v_h*i_rq) / (Omega_b*(x_s+x_mu))
// ─────────────────────────────────────────────────────────────────────
struct DFIGParams
{
    double xs      = 0.01;    // Stator reactance [p.u.]
    double xmu     = 3.5;     // Mutual (magnetizing) reactance [p.u.]
    double Htotal  = 3.5;     // Total inertia constant [s]
    double Teps    = 0.01;    // Rotor current time constant [s]
    double KV      = 10.0;    // Voltage controller gain
    double vref    = 1.0;     // Reference voltage [p.u.]
    double Popt    = 0.5;     // Optimal power tracking coefficient
    double OmegaB  = 314.159; // Base angular frequency = 2*pi*f [rad/s]

    // Initial conditions
    double omega_m0 = 1.0;    // Initial rotor speed [p.u.]
    double i_rq0    = 0.0;    // Initial active rotor current [p.u.]
    double i_rd0    = 0.0;    // Initial reactive rotor current [p.u.]
};

// ─────────────────────────────────────────────────────────────────────
//  Standard synchronous generator parameters (classical model)
//
//  State variables: delta, omega
//  ODEs:
//    d(delta)/dt = Omega_b * (omega - 1)
//    d(omega)/dt = (P_m - P_e - D*(omega - 1)) / (2*H)
//  Algebraic:
//    P_e = (E_prime * Vm / xd_prime) * sin(delta - Va)
// ─────────────────────────────────────────────────────────────────────
struct StdGenParams
{
    double xd_prime = 0.1;    // Transient reactance [p.u.]
    double H        = 5.0;    // Inertia constant [s]
    double D        = 0.0;    // Damping coefficient [p.u.]
    double OmegaB   = 314.159;// Base angular frequency [rad/s]
    double E_prime  = 1.05;   // Voltage behind transient reactance [p.u.]

    // Initial conditions
    double delta0   = 0.0;    // Initial rotor angle [rad]
    double omega0   = 1.0;    // Initial rotor speed [p.u.]
};

// ─────────────────────────────────────────────────────────────────────
//  Generator data (unified — carries either DFIG or Standard params)
// ─────────────────────────────────────────────────────────────────────
struct GeneratorData
{
    td::INT4 bus    = 0;
    td::INT4 number = 0;      // Generator ID within the network
    double Pg       = 0.0;    // Active power output [MW]
    double Qg       = 0.0;    // Reactive power output [MVAr]
    double Qmax     = 999.0;
    double Qmin     = -999.0;
    double Vs       = 1.0;    // Voltage setpoint [p.u.]
    double mBase    = 100.0;  // Machine base [MVA]

    bool isDFIG     = false;  // Set at runtime based on user config
    DFIGParams dfig;          // Populated when isDFIG == true
    StdGenParams std;         // Populated when isDFIG == false
};

// ─────────────────────────────────────────────────────────────────────
//  Aggregate power system data container
//
//  Matrices use natID dense::Matrix and sparse::IMatrix types
//  as mandated by the project specification.
// ─────────────────────────────────────────────────────────────────────
struct PowerSystem
{
    td::String name;
    double baseMVA = 100.0;

    cnt::PushBackVector<BusData>       buses;
    cnt::PushBackVector<BranchData>    branches;
    cnt::PushBackVector<GeneratorData> generators;

    // Default DFIG and standard params (applied to generators that
    // don't have per-generator overrides in the XML).
    DFIGParams   defaultDFIG;
    StdGenParams defaultStdGen;

    // ── natID matrices (built after XML parsing) ────────────────────

    // Y-bus admittance matrix (sparse, complex-valued)
    // Stored as natID sparse::ICmplxMatrix via factory function.
    // Ownership managed via sparse::CmplxMatrixReleaser.
    sparse::ICmplxMatrix*      pYbus = nullptr;
    sparse::CmplxMatrixReleaser ybusReleaser;

    // Generator parameter matrix (dense, real-valued)
    // Rows = generators, Cols = [Pg, Qg, Vs, mBase, H/Htotal, xd'/xs, ...]
    dense::DblMatrix genParams;

    // Bus voltage vector (dense, real-valued, Nx2: [Vm, Va])
    dense::DblMatrix busVoltages;

    void releaseMatrices()
    {
        // PointerReleaser is RAII — assignment triggers _ptr->release()
        if (pYbus)
        {
            ybusReleaser = static_cast<sparse::ICmplxMatrix*>(nullptr);
            pYbus = nullptr;
        }
        genParams.clean();
        busVoltages.clean();
    }

    ~PowerSystem()
    {
        releaseMatrices();
    }
};
