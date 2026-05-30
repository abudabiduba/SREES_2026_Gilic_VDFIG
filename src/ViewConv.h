//
//  ViewConv.h
//  Converter tab: input/output file selection, info preview, convert.
//  The Convert button launches conversion on a worker thread.
//
//  Created for Vjetroelektrana DFIG plugin.
//

#pragma once
#include <gui/View.h>
#include <gui/Label.h>
#include <gui/Button.h>
#include <gui/LineEdit.h>
#include <gui/TextEdit.h>
#include <gui/ProgressIndicator.h>
#include <gui/HorizontalLayout.h>
#include <gui/GridLayout.h>
#include <gui/GridComposer.h>
#include <gui/FileDialog.h>
#include <fo/FileOperations.h>
#include <fstream>
#include "ViewOptions.h"
#include "DFIGPlugin.h"

class ViewConv : public gui::View
{
protected:
    sc::IPlugin*          _pIPlugin;
    sc::IPlugin::CallBack _onComplete;
    ViewOptions*          _pViewOptions = nullptr;

    gui::Label            _lblFnIn;
    gui::LineEdit         _editFnIn;
    gui::Label            _lblFnOut;
    gui::LineEdit         _editFnOut;
    gui::Label            _lblStatus;
    gui::LineEdit         _editStatus;
    gui::Label            _lblProgress;
    gui::ProgressIndicator _progressBar;
    gui::Button           _btnSelectInFn;
    gui::Button           _btnSelectOutFn;
    gui::TextEdit         _te;
    gui::Button           _btnInfo;
    gui::Button           _btnConvert;

    gui::HorizontalLayout _hlButtons;
    gui::GridLayout        _gl;

    td::UINT4             _wndID;

protected:
    void handleUserActions()
    {
        // ── Info button ─────────────────────────────────────────────
        _btnInfo.onClick([this] {
            td::String fileName = _editFnIn.getText();
            if (!fo::fs::exists(fileName.c_str()))
            {
                _editStatus = "ERROR: File doesn't exist!";
                return;
            }
            std::ifstream ifs;
            if (!fo::openFile(ifs, fileName))
            {
                _editStatus = "ERROR: Cannot load file content!";
                return;
            }
            std::string fileContent((std::istreambuf_iterator<char>(ifs)),
                                     std::istreambuf_iterator<char>());
            _editStatus = "INFO: Content loaded.";
            _te.setText(fileContent.c_str());
        });

        // ── Select input file ───────────────────────────────────────
        _btnSelectInFn.onClick([this] {
            gui::OpenFileDialog::show(this, "Open Power System XML",
                "*.xml", _wndID + 1000,
                [this](gui::FileDialog* pDlg) {
                    if (pDlg->getStatus() == gui::FileDialog::Status::OK)
                    {
                        td::String fn = pDlg->getFileName();
                        if (!fn.isEmpty())
                        {
                            _editFnIn = fn;
                            _editFnIn.setFocus();
                        }
                    }
                });
        });

        // ── Select output file ──────────────────────────────────────
        _btnSelectOutFn.onClick([this] {
            gui::SaveFileDialog::show(this, "Save .dmodl Model",
                "*.dmodl", _wndID + 2000,
                [this](gui::FileDialog* pDlg) {
                    if (pDlg->getStatus() == gui::FileDialog::Status::OK)
                    {
                        td::String fn = pDlg->getFileName();
                        if (!fn.isEmpty())
                        {
                            _editFnOut = fn;
                            _editFnOut.setFocus();
                        }
                    }
                });
        });

        // ── Convert button ──────────────────────────────────────────
        _btnConvert.onClick([this] {
            td::String inputFileName = _editFnIn.getText();
            if (inputFileName.isEmpty())
            {
                _editStatus = "ERROR: Empty input file name!";
                return;
            }
            if (!fo::fs::exists(inputFileName.c_str()))
            {
                _editStatus = "ERROR: Input file doesn't exist!";
                return;
            }
            td::String outFileName = _editFnOut.getText();
            if (outFileName.isEmpty())
            {
                _editStatus = "ERROR: Empty output file name!";
                return;
            }

            const auto& options = _pViewOptions->getOptions();
            if (!startConversion(inputFileName, outFileName,
                                 _pIPlugin, options, _editStatus))
            {
                return;
            }

            // Note: conversion runs async on worker thread.
            // The onComplete callback will be called once
            // the user closes the plugin window or conversion finishes.
        });
    }

    ViewConv() = delete;

public:
    ViewConv(sc::IPlugin* pIPlugin, const sc::IPlugin::CallBack& onComplete)
    : _pIPlugin(pIPlugin)
    , _onComplete(onComplete)
    , _lblFnIn("Input XML:")
    , _lblFnOut("Output .dmodl:")
    , _lblStatus("Status:")
    , _lblProgress("Progress:")
    , _progressBar(gui::DataCtrl::Orientation::Horizontal, true)
    , _btnSelectInFn("…")
    , _btnSelectOutFn("…")
    , _btnInfo("Info")
    , _btnConvert("Convert")
    , _hlButtons(3)
    , _gl(6, 3)
    {
        assert(_pIPlugin);
        _editStatus.setAsReadOnly();
        _te.setAsReadOnly();
        _editFnIn.setToolTip("Path to power system XML configuration");
        _editFnOut.setToolTip("Output .dmodl digital model file");

        gui::GridComposer gc(_gl);
        gc.appendRow(_lblFnIn)     << _editFnIn     << _btnSelectInFn;
        gc.appendRow(_lblFnOut)    << _editFnOut     << _btnSelectOutFn;
        gc.appendRow(_lblStatus);    gc.appendCol(_editStatus, 0);
        gc.appendRow(_lblProgress);  gc.appendCol(_progressBar, 0);
        gc.appendRow(_te, 0);
        _hlButtons.appendSpacer() << _btnInfo << _btnConvert;
        gc.appendRow(_hlButtons, 0);

        setLayout(&_gl);
        handleUserActions();
    }

    void setOptions(ViewOptions* pViewOptions)
    {
        _pViewOptions = pViewOptions;
    }

    td::String getOutFileName() const
    {
        td::String strOutFn = _editFnOut.getText();
        return strOutFn;
    }
};
