//
//  ModelConverter.h
//  Converts a power system XML configuration into a .dmodl model file
//  for the dTwin solver.  DFIG generators use Federico Milano's model;
//  remaining generators use the classical synchronous machine model.
//
//  Threading: convert() is designed to run on a worker std::thread.
//  It reports progress back to the GUI via a ProgressCallback that the
//  caller wraps with gui::thread::asyncExecInMainThread.
//
//  Created for Vjetroelektrana DFIG plugin.
//

#pragma once
#include <td/Types.h>
#include <td/String.h>
#include <td/MutableString.h>
#include <arch/MemoryOut.h>
#include <functional>
#include "PowerSystemModel.h"

// Forward declarations (GUI types only needed in the .cpp)
namespace gui { class ProgressIndicator; class LineEdit; }

// ─────────────────────────────────────────────────────────────────────
//  Converter options (populated from the GUI ViewOptions tab)
// ─────────────────────────────────────────────────────────────────────
struct ConvertOptions
{
    td::String modelName;
    td::String dfigGeneratorIDs;   // Comma-separated gen numbers, e.g. "2,5,7"
    td::INT4   maxIter  = 20;
    float      dTime    = 0.001f;
    float      endTime  = 30.0f;

    // Default DFIG parameters (apply to all DFIG gens unless overridden)
    DFIGParams   dfigDefaults;
    StdGenParams stdGenDefaults;
};

// ─────────────────────────────────────────────────────────────────────
//  Progress callback signature
//    progress : [0.0 … 1.0]
//    message  : human-readable status string
// ─────────────────────────────────────────────────────────────────────
using ProgressCallback = std::function<void(double progress, const td::String& message)>;

// ─────────────────────────────────────────────────────────────────────
//  ModelConverter — stateless converter callable from any thread
// ─────────────────────────────────────────────────────────────────────
class ModelConverter
{
public:
    // ── Main entry point ────────────────────────────────────────────
    //  inputXMLPath  : path to the power system XML config file
    //  outDmodlPath  : path to the .dmodl output file
    //  options       : solver & DFIG settings from the GUI
    //  memDigitalOut : archive buffer for the digital model text
    //  memVisualOut  : archive buffer for the visual model text
    //  onProgress    : called periodically from the worker thread;
    //                  the caller must dispatch to the main thread.
    //  Returns true on success.
    static bool convert(const td::String&      inputXMLPath,
                        const td::String&      outDmodlPath,
                        const ConvertOptions&  options,
                        arch::MemoryOut&       memDigitalOut,
                        arch::MemoryOut*       memVisualOut,
                        const ProgressCallback& onProgress);

private:
    // ── Internal pipeline stages ────────────────────────────────────

    // 1. Parse XML → PowerSystem struct
    static bool parseXML(const td::String& xmlPath,
                         PowerSystem& ps,
                         const ProgressCallback& onProgress);

    // 2. Mark selected generators as DFIG based on the comma list
    static void assignDFIGGenerators(PowerSystem& ps,
                                     const td::String& dfigIDs,
                                     const DFIGParams& dfigDefaults,
                                     const StdGenParams& stdDefaults);

    // 3. Emit the model header
    static void writeHeader(arch::MemoryOut& out,
                            const ConvertOptions& opts,
                            td::MutableString& buf);

    // 4. Emit variable declarations
    static void writeVariables(arch::MemoryOut& out,
                               const PowerSystem& ps,
                               td::MutableString& buf);

    // 5. Emit parameter values
    static void writeParameters(arch::MemoryOut& out,
                                const PowerSystem& ps,
                                td::MutableString& buf);

    // 6. Emit ODE / algebraic equations for standard generators
    static void writeStdGenEquations(arch::MemoryOut& out,
                                     const PowerSystem& ps,
                                     td::MutableString& buf);

    // 7. Emit ODE / algebraic equations for DFIG generators
    static void writeDFIGEquations(arch::MemoryOut& out,
                                   const PowerSystem& ps,
                                   td::MutableString& buf);

    // 8. Emit PostProc section (power injections, network calcs)
    static void writePostProc(arch::MemoryOut& out,
                              const PowerSystem& ps,
                              td::MutableString& buf);

    // 9. Generate visual model (.vmodl)
    static void writeVisualModel(arch::MemoryOut& out,
                                 const PowerSystem& ps);

    // 10. Build natID matrices (sparse Y-bus, dense gen params)
    static void buildMatrices(PowerSystem& ps,
                              const ProgressCallback& onProgress);

    // ── Helpers ─────────────────────────────────────────────────────
    static void parseDFIGIDs(const td::String& csv,
                             cnt::PushBackVector<td::INT4>& ids);
};
