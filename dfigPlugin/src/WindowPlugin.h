//
//  WindowPlugin.h
//  Plugin window shown by dTwin when the plugin is activated.
//
//  Created for Vjetroelektrana DFIG plugin.
//

#pragma once
#include <gui/Window.h>
#include "TabView.h"

class WindowPlugin : public gui::Window
{
protected:
    TabView _tabView;
    sc::IPlugin::Cleaner _cleanPlugin;

    void onClose() override final
    {
        _cleanPlugin();
        onClosedPluginWindow();
    }

public:
    WindowPlugin(gui::Window* parentWnd,
                 sc::IPlugin* pIPlugin,
                 const sc::IPlugin::CallBack& onComplete,
                 const sc::IPlugin::Cleaner& cleaner,
                 td::UINT4 wndID = 0)
    : gui::Window(gui::Size(900, 650), parentWnd, wndID)
    , _tabView(pIPlugin, onComplete)
    , _cleanPlugin(cleaner)
    {
        setTitle("Vjetroelektrana DFIG — Converter Plugin");
        setCentralView(&_tabView);
    }

    td::String getOutFileName() const
    {
        return _tabView.getOutFileName();
    }
};
