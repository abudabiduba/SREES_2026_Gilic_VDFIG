//
//  DFIGPlugin.h
//  Plugin API header — exports the sc::IPlugin interface.
//
//  Created for Vjetroelektrana DFIG plugin.
//

#pragma once
#include <compiler/Definitions.h>
#include <sc/IPlugin.h>
#include <gui/LineEdit.h>
#include "ModelConverter.h"

// ── DLL export/import macros ────────────────────────────────────────
#ifdef MU_WINDOWS
    #ifdef PLUGIN_EXPORTS
        #define PLUGIN_API __declspec(dllexport)
    #else
        #define PLUGIN_API __declspec(dllimport)
    #endif
#else
    #ifdef PLUGIN_EXPORTS
        #define PLUGIN_API __attribute__((visibility("default")))
    #else
        #define PLUGIN_API
    #endif
#endif

void onClosedPluginWindow();

bool startConversion(const td::String& inputFileName,
                     const td::String& outFileName,
                     sc::IPlugin* pIPlugin,
                     const ConvertOptions& options,
                     gui::LineEdit& status);
