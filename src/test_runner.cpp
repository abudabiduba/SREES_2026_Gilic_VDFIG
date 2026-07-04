//
//  test_runner.cpp
//  Headless command-line test runner for ModelConverter.
//  Converts case9, case30, case118, case300 XML configs to .dmodl / .vmodl.
//

#include <iostream>
#include "ModelConverter.h"
#include <arch/MemoryOut.h>
#include <fo/FileOperations.h>
#include <mu/Application.h>
#include <sparse/ISolver.h>

void runTestCase(const td::String& xmlPath, const td::String& dmodlPath)
{
    std::cout << "========================================\n";
    std::cout << "Testing: " << xmlPath.c_str() << "\n";
    std::cout << "========================================\n";

    ConvertOptions opts;
    fo::fs::path p(xmlPath.c_str());
    opts.modelName = p.stem().string().c_str();
    opts.dfigGeneratorIDs = "2"; // Generator 2 is DFIG in our test cases
    opts.maxIter = 50;
    opts.dTime = 0.001f;
    opts.endTime = 30.0f;

    // Allocate memory archives on the stack
    arch::MemoryOut digitModel;
    arch::MemoryOut visualModel;

    ProgressCallback onProgress = [](double progress, const td::String& message) {
        std::cout << "[" << int(progress * 100.0) << "%] " << message.c_str() << "\n";
    };

    bool ok = ModelConverter::convert(xmlPath, dmodlPath, opts, digitModel, &visualModel, onProgress);

    if (ok)
    {
        std::cout << "SUCCESS: Model converted successfully.\n";
        std::cout << "Output written to:\n  " << dmodlPath.c_str() << "\n";
        td::String vmodlPath = fo::replaceFileExtension<false>(dmodlPath, ".vmodl");
        std::cout << "  " << vmodlPath.c_str() << "\n";
    }
    else
    {
        std::cerr << "FAILED: Conversion failed.\n";
    }

    std::cout << "\n";
}

int main(int argc, const char* argv[])
{
    // Initialize the natID framework application (crucial for solver libraries initialization)
    mu::Application app(argc, argv);

    td::String baseDir = "/home/abu/SREES/src";
    if (argc > 1)
    {
        baseDir = argv[1];
    }

    td::String testDataDir(baseDir);
    testDataDir.append("/testData");

    td::String outputDir(baseDir);
    outputDir.append("/build/testOutput");

    // Ensure output dir exists using std::filesystem
    std::filesystem::create_directories(outputDir.c_str());

    td::String case9(testDataDir);
    case9.append("/case9.xml");
    td::String case9Out(outputDir);
    case9Out.append("/case9.dmodl");
    runTestCase(case9, case9Out);

    td::String case30(testDataDir);
    case30.append("/case30.xml");
    td::String case30Out(outputDir);
    case30Out.append("/case30.dmodl");
    runTestCase(case30, case30Out);

    td::String case118(testDataDir);
    case118.append("/case118.xml");
    td::String case118Out(outputDir);
    case118Out.append("/case118.dmodl");
    runTestCase(case118, case118Out);

    td::String case300(testDataDir);
    case300.append("/case300.xml");
    td::String case300Out(outputDir);
    case300Out.append("/case300.dmodl");
    runTestCase(case300, case300Out);

    std::cout << "All test runs completed.\n";
    
    // Explicitly release solver libraries before exit
    sparse::releaseSolverLibraries();
    return 0;
}
