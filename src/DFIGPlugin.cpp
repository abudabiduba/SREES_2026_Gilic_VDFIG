//
//  DFIGPlugin.cpp
//  sc::IPlugin implementation + extern "C" DLL entry point.
//  The conversion runs on a worker thread; progress is relayed
//  to the GUI via gui::thread::asyncExecInMainThread.
//
//  Created for Vjetroelektrana DFIG plugin.
//

#include "DFIGPlugin.h"
#include <sc/IPlugin.h>
#include "WindowPlugin.h"
#include <gui/Thread.h>
#include <fo/FileOperations.h>
#include <mu/ScopedCLocale.h>
#include <thread>

// ═════════════════════════════════════════════════════════════════════
//  Plugin singleton implementing sc::IPlugin
// ═════════════════════════════════════════════════════════════════════
class Plugin : public sc::IPlugin
{
    MemoryArchiveContainer _outArchives;
    WindowPlugin* _pWnd = nullptr;
public:
    Plugin()
    {
        for (size_t i = 0; i < size_t(ArchType::NA); ++i)
            _outArchives[i] = nullptr;
    }

    void show(gui::Window* parentWnd,
              MemoryArchiveContainer& archives,
              td::UINT4 wndID,
              const sc::IPlugin::Cleaner& cleaner,
              const sc::IPlugin::CallBack& onComplete) override final
    {
        for (size_t i = 0; i < size_t(ArchType::NA); ++i)
            _outArchives[i] = archives[i];

        if (_pWnd)
            _pWnd->setFocus();
        else
        {
            _pWnd = new WindowPlugin(parentWnd, this, onComplete, cleaner, wndID);
            _pWnd->open();
        }
    }

    td::String getMenuName() const override final
    {
        return "Vjetroelektrana DFIG Converter";
    }

    arch::MemoryOut* getArchive(sc::IPlugin::ArchType type) override final
    {
        auto iType = size_t(type);
        if (iType >= getMaxSupportedArchiveParts())
            return nullptr;
        return _outArchives[size_t(type)];
    }

    MemoryArchiveContainer& getArchives() override final
    {
        return _outArchives;
    }

    td::String getOutFileName() const override final
    {
        assert(_pWnd);
        return _pWnd->getOutFileName();
    }

    size_t getMaxSupportedArchiveParts() const override final
    {
        return size_t(ArchType::NA);
    }

    ModelType getModelType() const override final
    {
        return ModelType::DAE;
    }

    void onClosedPluginWindow()
    {
        _pWnd = nullptr;
    }
};

static Plugin s_plugin;

void onClosedPluginWindow()
{
    s_plugin.onClosedPluginWindow();
}

// ── DLL entry point ────────────────────────────────────────────────
extern "C"
{
    PLUGIN_API sc::IPlugin* getPluginInterface()
    {
        return &s_plugin;
    }
}

// ═════════════════════════════════════════════════════════════════════
//  startConversion — called from the GUI Convert button.
//  Launches a worker thread and uses asyncExecInMainThread to update
//  the progress indicator and status bar on the GUI thread.
// ═════════════════════════════════════════════════════════════════════
bool startConversion(const td::String& inputFileName,
                     const td::String& outFileName,
                     sc::IPlugin* pIPlugin,
                     const ConvertOptions& options,
                     gui::LineEdit& status)
{
    if (!fo::fs::exists(inputFileName.c_str()))
    {
        status = "ERROR: Input XML file not found!";
        return false;
    }

    // Get the digital and visual model archives from the plugin
    auto pDigitModel  = pIPlugin->getArchive(sc::IPlugin::ArchType::DigitalModel);
    auto pVisualModel = pIPlugin->getArchive(sc::IPlugin::ArchType::VisualModel);

    if (!pDigitModel)
    {
        status = "ERROR: Digital model archive is null!";
        return false;
    }

    status = "Starting conversion...";

    // Capture copies for the worker thread
    td::String inFile(inputFileName);
    td::String outFile(outFileName);
    ConvertOptions opts(options);

    // ── Worker thread ───────────────────────────────────────────────
    std::thread worker([inFile, outFile, opts,
                        pDigitModel, pVisualModel, &status]()
    {
        // Build the progress callback that dispatches to GUI thread
        ProgressCallback onProgress = [&status](double progress,
                                                const td::String& msg)
        {
            td::String msgCopy(msg);
            double pVal = progress;
            gui::thread::asyncExecInMainThread([&status, pVal, msgCopy]()
            {
                td::MutableString s;
                s.reserve(256);
                s.appendFormat("[%3.0f%%] %s", pVal * 100.0, msgCopy.c_str());
                status = s.c_str();
            });
        };

        bool ok = ModelConverter::convert(inFile, outFile, opts,
                                          *pDigitModel, pVisualModel,
                                          onProgress);

        gui::thread::asyncExecInMainThread([&status, ok]()
        {
            if (ok)
                status = "Conversion completed successfully!";
            else
                status = "ERROR: Conversion failed — see log.";
        });
    });

    worker.detach();
    return true;
}
