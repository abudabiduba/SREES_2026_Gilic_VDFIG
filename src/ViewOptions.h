//
//  ViewOptions.h
//  Options tab: DFIG params, generator selection, solver settings.
//
//  Created for Vjetroelektrana DFIG plugin.
//

#pragma once
#include <gui/View.h>
#include <gui/Label.h>
#include <gui/LineEdit.h>
#include <gui/NumericEdit.h>
#include <gui/GridLayout.h>
#include <gui/GridComposer.h>
#include "DFIGPlugin.h"

class ViewOptions : public gui::View
{
protected:
    // ── Solver settings ─────────────────────────────────────────────
    gui::Label       _lblName;
    gui::LineEdit    _editName;
    gui::Label       _lblDFIGIDs;
    gui::LineEdit    _editDFIGIDs;
    gui::Label       _lblMaxIter;
    gui::NumericEdit _neMaxIter;
    gui::Label       _lbldT;
    gui::NumericEdit _neDeltaTime;
    gui::Label       _lblEndT;
    gui::NumericEdit _neEndTime;

    // ── DFIG default parameters ─────────────────────────────────────
    gui::Label       _lblDFIGSection;
    gui::Label       _lblXs;
    gui::NumericEdit _neXs;
    gui::Label       _lblXmu;
    gui::NumericEdit _neXmu;
    gui::Label       _lblHtotal;
    gui::NumericEdit _neHtotal;
    gui::Label       _lblTeps;
    gui::NumericEdit _neTeps;
    gui::Label       _lblKV;
    gui::NumericEdit _neKV;
    gui::Label       _lblVref;
    gui::NumericEdit _neVref;
    gui::Label       _lblPopt;
    gui::NumericEdit _nePopt;
    gui::Label       _lblOmegaB;
    gui::NumericEdit _neOmegaB;

    gui::GridLayout  _gl;
    ConvertOptions   _options;

public:
    ViewOptions()
    : _lblName("Model name:")
    , _lblDFIGIDs("DFIG gen IDs (comma sep.):")
    , _lblMaxIter("Max iterations:")
    , _lbldT("Time step dT:")
    , _lblEndT("End time:")
    , _lblDFIGSection("── Default DFIG Parameters ──")
    , _lblXs("xs (stator reactance):")
    , _lblXmu("xmu (magnetizing):")
    , _lblHtotal("H_total (inertia):")
    , _lblTeps("T_eps (rotor current τ):")
    , _lblKV("K_V (voltage gain):")
    , _lblVref("v_ref (voltage ref.):")
    , _lblPopt("P_opt (power tracking):")
    , _lblOmegaB("Omega_b (base freq.):")
    , _neMaxIter(td::int4)
    , _neDeltaTime(td::real4, gui::LineEdit::Messages::DoNotSend, false, "dT", 6)
    , _neEndTime(td::real4, gui::LineEdit::Messages::DoNotSend, true, "End", 3)
    , _neXs(td::real8,  gui::LineEdit::Messages::DoNotSend, false, "xs", 6)
    , _neXmu(td::real8, gui::LineEdit::Messages::DoNotSend, false, "xmu", 4)
    , _neHtotal(td::real8, gui::LineEdit::Messages::DoNotSend, false, "H", 4)
    , _neTeps(td::real8, gui::LineEdit::Messages::DoNotSend, false, "Teps", 6)
    , _neKV(td::real8,   gui::LineEdit::Messages::DoNotSend, false, "KV", 4)
    , _neVref(td::real8, gui::LineEdit::Messages::DoNotSend, false, "vref", 4)
    , _nePopt(td::real8, gui::LineEdit::Messages::DoNotSend, false, "Popt", 4)
    , _neOmegaB(td::real8, gui::LineEdit::Messages::DoNotSend, false, "OmegaB", 3)
    , _gl(12, 4)
    {
        // Defaults
        _editName     = "Vjetroelektrana DFIG model";
        _editDFIGIDs  = "2";
        _neMaxIter.setValue(td::INT4(20));
        _neDeltaTime.setValue(0.001f);
        _neEndTime.setValue(30.0f);

        _neXs.setValue(0.01);
        _neXmu.setValue(3.5);
        _neHtotal.setValue(3.5);
        _neTeps.setValue(0.01);
        _neKV.setValue(10.0);
        _neVref.setValue(1.0);
        _nePopt.setValue(0.5);
        _neOmegaB.setValue(314.159);

        gui::GridComposer gc(_gl);
        gc.appendRow(_lblName);       gc.appendCol(_editName, 0);
        gc.appendRow(_lblDFIGIDs);    gc.appendCol(_editDFIGIDs, 0);
        gc.appendRow(_lblMaxIter)  << _neMaxIter;
        gc.appendRow(_lbldT)       << _neDeltaTime << _lblEndT << _neEndTime;
        gc.appendRow(_lblDFIGSection, 0);
        gc.appendRow(_lblXs)       << _neXs     << _lblXmu    << _neXmu;
        gc.appendRow(_lblHtotal)   << _neHtotal  << _lblTeps   << _neTeps;
        gc.appendRow(_lblKV)       << _neKV      << _lblVref   << _neVref;
        gc.appendRow(_lblPopt)     << _nePopt    << _lblOmegaB << _neOmegaB;

        setLayout(&_gl);
    }

    const ConvertOptions& getOptions()
    {
        _options.modelName       = _editName.getText();
        _options.dfigGeneratorIDs = _editDFIGIDs.getText();
        _options.maxIter         = _neMaxIter.getValue().i4Val();
        _options.dTime           = _neDeltaTime.getValue().r4Val();
        _options.endTime         = _neEndTime.getValue().r4Val();

        // DFIG defaults from the GUI
        _options.dfigDefaults.xs      = _neXs.getValue().r8Val();
        _options.dfigDefaults.xmu     = _neXmu.getValue().r8Val();
        _options.dfigDefaults.Htotal  = _neHtotal.getValue().r8Val();
        _options.dfigDefaults.Teps    = _neTeps.getValue().r8Val();
        _options.dfigDefaults.KV      = _neKV.getValue().r8Val();
        _options.dfigDefaults.vref    = _neVref.getValue().r8Val();
        _options.dfigDefaults.Popt    = _nePopt.getValue().r8Val();
        _options.dfigDefaults.OmegaB  = _neOmegaB.getValue().r8Val();

        return _options;
    }
};
