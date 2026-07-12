//
//  TabView.h
//  Standard tabbed container: Converter + Options.
//
//  Created for Vjetroelektrana DFIG plugin.
//

#pragma once
#include <gui/StandardTabView.h>
#include "ViewConv.h"
#include "ViewOptions.h"

class TabView : public gui::StandardTabView
{
protected:
    ViewConv    _v1;
    ViewOptions _v2;

    TabView() = delete;
public:
    TabView(sc::IPlugin* pIPlugin, const sc::IPlugin::CallBack& onComplete)
    : _v1(pIPlugin, onComplete)
    {
        _v1.setOptions(&_v2);

        addView(&_v1, "Converter");
        addView(&_v2, "Options");
    }

    td::String getOutFileName() const
    {
        return _v1.getOutFileName();
    }
};
