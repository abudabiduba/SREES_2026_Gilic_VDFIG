#include "Application.h"
#include <td/StringConverter.h>
#include <gui/WinMain.h>
#include <iostream>
#include <filesystem>
#include <syst/LibraryLoader.h>
#include <fo/FileOperations.h>
#include <mu/Application.h>
#include <sparse/ISolver.h>

// Function type for the exported convertHeadless
using HeadlessConvertFunc = bool(*)(const char*, const char*);

int runBatchMode(int argc, const char * argv[])
{
    // Initialize the headless natID framework application (crucial for loading solver configurations)
    mu::Application app(argc, argv);

    std::cout << "==================================================\n";
    std::cout << " DFIG Converter - Automated Headless Batch Testing\n";
    std::cout << "==================================================\n";

    // Locate the plugin directory and file
    fo::fs::path pluginPath = PLUGIN_DIR;
#if defined(_WIN32)
    pluginPath /= "dfigPlugin.dll";
#elif defined(__APPLE__)
    pluginPath /= "libdfigPlugin.dylib";
#else
    pluginPath /= "libdfigPlugin.so";
#endif

    std::cout << "Loading plugin from: " << pluginPath.string() << "\n";

    syst::LibraryLoader loader;
    if (!loader.load(pluginPath.string().c_str()))
    {
        std::cerr << "ERROR: Failed to load plugin library!\n";
        return 1;
    }

    HeadlessConvertFunc convertFunc = loader.getFunctionPtr<HeadlessConvertFunc>("convertHeadless");
    if (!convertFunc)
    {
        std::cerr << "ERROR: Plugin does not export 'convertHeadless' symbol!\n";
        return 1;
    }

    // Locate testData directory (inside SREES/src/test_runner/src/testData)
    fo::fs::path testDataDir = "/home/abu/SREES/src/test_runner/src/testData";
    fo::fs::path outputDir = "/home/abu/SREES/src/test_runner/src/testOutput";

    if (!fo::fs::exists(testDataDir))
    {
        std::cerr << "ERROR: testData directory not found at " << testDataDir.string() << "\n";
        return 1;
    }

    std::filesystem::create_directories(outputDir);
    std::cout << "Reading XML cases from: " << testDataDir.string() << "\n";
    std::cout << "Output DMODL directory: " << outputDir.string() << "\n\n";

    // Loop through all XML files in testDataDir
    int successCount = 0;
    int totalCount = 0;

    for (const auto& entry : std::filesystem::directory_iterator(testDataDir))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".xml")
        {
            totalCount++;
            std::string xmlPath = entry.path().string();
            std::string stemName = entry.path().stem().string();
            fo::fs::path outDmodl = outputDir / (stemName + ".dmodl");

            std::cout << "--------------------------------------------------\n";
            std::cout << "[" << totalCount << "] Testing case: " << stemName << "\n";
            std::cout << "Source XML: " << xmlPath << "\n";
            std::cout << "Output: " << outDmodl.string() << "\n";

            bool success = convertFunc(xmlPath.c_str(), outDmodl.string().c_str());
            if (success)
            {
                std::cout << "Result: SUCCESS\n";
                successCount++;
            }
            else
            {
                std::cout << "Result: FAILED\n";
            }
        }
    }

    std::cout << "==================================================\n";
    std::cout << "Batch run finished: " << successCount << " / " << totalCount << " cases succeeded.\n";
    std::cout << "==================================================\n";

    return (successCount == totalCount) ? 0 : 1;
}

int main(int argc, const char * argv[])
{
    // Check if the user requested headless batch mode
    if (argc > 1 && (std::string(argv[1]) == "--batch" || std::string(argv[1]) == "-b"))
    {
        // Headless batch execution
        return runBatchMode(argc, argv);
    }

    // GUI Application execution
    Application app(argc, argv);
    app.init("BA"); // Change to app.init("EN") for English if desired
    return app.run();
}
