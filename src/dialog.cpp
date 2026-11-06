
/***************************************************************************
 *   Copyright (C) 2012 by Jonathan Duddington                             *
 *   email: jonsd@users.sourceforge.net                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see:                                 *
 *               <http://www.gnu.org/licenses/>.                           *
 ***************************************************************************/


#include <wx/cmndata.h>
#include <wx/colordlg.h>
#include <wx/choice.h>
#include <wx/filename.h>
#include "auroramon.h"


extern void MakeInverterNames(int control);
extern wxArrayString InverterNames;


/************************************************************
                Setup Dialog
*************************************************************/

BEGIN_EVENT_TABLE(DlgSetup, wxDialog)
	EVT_BUTTON(dlgOK, DlgSetup::OnButton)
	EVT_BUTTON(dlgCancel, DlgSetup::OnButton)
END_EVENT_TABLE()

#define N_DATE_FORMATS  4
wxString date_formats[N_DATE_FORMATS] = {_T("2012-12-31"), _T("Local"), _T("31 Dec 2012"), _T("31.12.2012")};

DlgSetup::DlgSetup(wxWindow *parent)
    : wxDialog(parent, -1, _T("Setup"))
{//=================================
    wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);
    wxFlexGridSizer *grid = new wxFlexGridSizer(2, DLG_GAP, 2*DLG_GAP);
    wxBoxSizer *addresses = new wxBoxSizer(wxHORIZONTAL);

    t_serialport = new wxTextCtrl(this, dlgSerialPort, serial_port, wxDefaultPosition, wxSize(320, -1));
    t_inverter1 = new wxTextCtrl(this, dlgInverter1, wxString::Format(_T("%d"), inverter_address[0]), wxDefaultPosition, wxSize(60, -1));
    t_inverter2 = new wxTextCtrl(this, dlgInverter2, wxString::Format(_T("%d"), inverter_address[1]), wxDefaultPosition, wxSize(60, -1));
    t_data_dir = new wxTextCtrl(this, dlgDataDir, data_dir, wxDefaultPosition, wxSize(320, -1));
    t_date_format = new wxChoice(this, -1, wxDefaultPosition, wxDefaultSize, N_DATE_FORMATS, date_formats);

    addresses->Add(t_inverter1);
    addresses->Add(t_inverter2, 0, wxLEFT, DLG_GAP);

    grid->AddGrowableCol(1);
    AddGridLabel(this, grid, _T("Serial port"));
    grid->Add(t_serialport, 0, wxEXPAND);
    AddGridLabel(this, grid, _T("Inverter addresses"));
    grid->Add(addresses);
    AddGridLabel(this, grid, _T("Data directory"));
    grid->Add(t_data_dir, 0, wxEXPAND);
    AddGridLabel(this, grid, _T("Date format"));
    grid->Add(t_date_format);

    button_OK = new wxButton(this, dlgOK, _T("OK"));
    button_OK->SetDefault();
    button_Cancel = new wxButton(this, dlgCancel, _T("Cancel"));

    top->Add(grid, 1, wxEXPAND | wxALL, DLG_BORDER);
    top->Add(MakeButtonRow(button_OK, button_Cancel), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, DLG_BORDER);
    SetSizerAndFit(top);
}

void DlgSetup::ShowDlg()
{//=====================
    t_serialport->ChangeValue(serial_port);

    if(inverter_address[0] > 0)
        t_inverter1->ChangeValue(wxString::Format(_T("%d"), inverter_address[0]));
    else
        t_inverter1->ChangeValue(_T(""));

    if(inverter_address[1] > 0)
        t_inverter2->ChangeValue(wxString::Format(_T("%d"), inverter_address[1]));
    else
        t_inverter2->ChangeValue(_T(""));

    t_data_dir->ChangeValue(data_dir);
    t_date_format->SetSelection(option_date_format);
    ShowModal();
}


void DlgSetup::OnButton(wxCommandEvent &event)
{//===========================================
    int id;
    long int value;
    wxString dir;
    wxString s;

    id = event.GetId();
    if(id == dlgOK)
    {
        dir = t_data_dir->GetValue();
        if(!wxDirExists(dir))
        {
            // ask before anything is changed, the dialog stays open if the answer is No
            if(wxMessageBox(_T("Create directory '") + dir + _T("' ?"), _T("Setup"), wxYES_NO, this) != wxYES)
            {
                return;
            }
            wxMkdir(dir);
        }

        {
            wxMutexLocker lock(comms_mutex);   // the inverter thread reads these
            serial_port = t_serialport->GetValue();
            value = 0;
            t_inverter1->GetValue().ToLong(&value);
            inverter_address[0] = value;
            value = 0;   // a blank field means no inverter
            t_inverter2->GetValue().ToLong(&value);
            inverter_address[1] = value;
            data_dir = dir;
        }

        option_date_format = t_date_format->GetSelection();

        mainframe->SetupStatusPanel(1);
        status_panel->Refresh();

//        CalcSun(graph_date_ymd);  // done in OpenLogFiles()
//        graph_panel->Refresh();  // done in OpenLogFiles()
        OpenLogFiles(-1, 0);  // because the inverter address may have changed
        ConfigSave(1);
        mainframe->MakeInverterMenu(1);  // remake the inverter menu

    }
    Show(false);
}


//=======================================================================================================================


/************************************************************
                Location Dialog
*************************************************************/

BEGIN_EVENT_TABLE(DlgLocation, wxDialog)
	EVT_BUTTON(dlgOK, DlgLocation::OnButton)
	EVT_BUTTON(dlgCancel, DlgLocation::OnButton)
END_EVENT_TABLE()


wxString degrees_to_string(double degrees)
{//=======================================
    int mins;
    int secs;
    int deg;

    deg = (int)degrees;
    if(degrees < 0) degrees = -degrees;
    mins = (int)(degrees*60) % 60;
    secs = (int)(degrees*3600) % 60;
    return(wxString::Format(_T("%2d:%.2d:%.2d"), deg, mins, secs));
}

double string_to_degrees(wxString string)
{//======================================
    int deg = 0;
    int mins = 0;
    float secs = 0;
    double degrees = 0;
    int sign;
    char buf[200];

    strncpy(buf, string.mb_str(wxConvLocal), sizeof(buf));
    buf[sizeof(buf)-1] = 0;

    if(strchr(buf,':') == NULL)
    {
        string.ToDouble(&degrees);
    }
    else
    {
        sscanf(buf, "%d:%d:%f", &deg, &mins, &secs);
        sign = 1;
        if(deg < 0)
        {
            sign = -1;
            deg = -deg;
        }
        degrees = sign * (deg + (double)mins / 60 + secs/3600);
    }
    return(degrees);
}

DlgLocation::DlgLocation(wxWindow *parent)
    : wxDialog(parent, -1, _T("Location"))
{//======================================
    int ix;
    wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);
    wxFlexGridSizer *grid = new wxFlexGridSizer(2, DLG_GAP, 2*DLG_GAP);
    wxFlexGridSizer *panels = new wxFlexGridSizer(1 + N_PANEL_GROUPS, DLG_GAP, 2*DLG_GAP);
    wxBoxSizer *timezone = new wxBoxSizer(wxHORIZONTAL);

    t_longitude = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(110, -1));
    t_latitude = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(110, -1));
    t_timezone = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(110, -1));
    t_tz_auto = new wxCheckBox(this, -1, _T("auto"));

    timezone->Add(t_timezone);
    timezone->Add(t_tz_auto, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, DLG_GAP);

    AddGridLabel(this, grid, _T("Longitude"));
    grid->Add(t_longitude);
    AddGridLabel(this, grid, _T("Latitude"));
    grid->Add(t_latitude);
    AddGridLabel(this, grid, _T("Time Zone (hrs E)"));
    grid->Add(timezone);

    AddGridLabel(this, panels, _T("Panel Group"));
    for(ix=0; ix<N_PANEL_GROUPS; ix++)
    {
        t_tilt[ix] = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(70, -1), wxTE_CENTRE);
        t_facing[ix] = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(70, -1), wxTE_CENTRE);
        panels->Add(new wxStaticText(this, -1, wxString(wxChar('A' + ix))), 0, wxALIGN_CENTER);
    }
    AddGridLabel(this, panels, _T("Tilt"));
    for(ix=0; ix<N_PANEL_GROUPS; ix++)
        panels->Add(t_tilt[ix]);
    AddGridLabel(this, panels, _T("Facing CW from N"));
    for(ix=0; ix<N_PANEL_GROUPS; ix++)
        panels->Add(t_facing[ix]);

    button_OK = new wxButton(this, dlgOK, _T("OK"));
    button_OK->SetDefault();
    button_Cancel = new wxButton(this, dlgCancel, _T("Cancel"));

    top->Add(grid, 0, wxALL, DLG_BORDER);
    top->Add(panels, 1, wxLEFT | wxRIGHT | wxBOTTOM, DLG_BORDER);
    top->Add(MakeButtonRow(button_OK, button_Cancel), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, DLG_BORDER);
    SetSizerAndFit(top);
}

void DlgLocation::ShowDlg()
{//=====================
    int ix;

    t_longitude->ChangeValue(degrees_to_string(Longitude));
    t_latitude->ChangeValue(degrees_to_string(Latitude));
    t_timezone->ChangeValue(wxString::Format(_T("%.2f"), Timezone));
    if(TimeZone2 > 24)
        t_tz_auto->SetValue(true);
    else
        t_tz_auto->SetValue(false);

    for(ix=0; ix<N_PANEL_GROUPS; ix++)
    {
        t_tilt[ix]->ChangeValue(wxString::Format(_T("%.1f"), panel_groups[ix].tilt));
        t_facing[ix]->ChangeValue(wxString::Format(_T("%.1f"), panel_groups[ix].facing));
    }
    ShowModal();
}


void DlgLocation::OnButton(wxCommandEvent &event)
{//==============================================
    int id;
    int ix;
    wxString dir;
    wxString s;

    id = event.GetId();
    if(id == dlgOK)
    {
        s = t_longitude->GetValue();
        if(s.Find(':') == wxNOT_FOUND)
        {
            Longitude = 0;
            s.ToDouble(&Longitude);
        }
        else
        {
            Longitude = string_to_degrees(s);
        }

        s = t_latitude->GetValue();
        if(s.Find(':') == wxNOT_FOUND)
        {
            Latitude = 0;
            s.ToDouble(&Latitude);
        }
        else
        {
            Latitude = string_to_degrees(s);
        }

        t_timezone->GetValue().ToDouble(&Timezone);
        if(t_tz_auto->GetValue() == true)
            TimeZone2 = 99;
        else
            TimeZone2 = Timezone;

        for(ix=0; ix<N_PANEL_GROUPS; ix++)
        {
            t_tilt[ix]->GetValue().ToDouble(&panel_groups[ix].tilt);
            t_facing[ix]->GetValue().ToDouble(&panel_groups[ix].facing);
        }
        ConfigSave(1);
        if(CalcSun(graph_date_ymd) < 0)
            return;
        graph_panel->Refresh();
    }
    Show(false);
}


//=======================================================================================================================


/************************************************************
                Inverter Dialog
*************************************************************/

BEGIN_EVENT_TABLE(DlgInverter, wxDialog)
	EVT_BUTTON(dlgOK, DlgInverter::OnButton)
	EVT_BUTTON(dlgCancel, DlgInverter::OnButton)
END_EVENT_TABLE()


#define N_SYNC_CHOICES  5
static wxString sync_choices[N_SYNC_CHOICES] = {
    _T("Never"), _T("Every hour"), _T("Every 6 hours"), _T("Every day"), _T("Every week") };
static const int sync_seconds[N_SYNC_CHOICES] = {0, 3600, 6*3600, 24*3600, 7*24*3600};


DlgInverter::DlgInverter(wxWindow *parent)
    : wxDialog(parent, -1, _T("Inverter Settings"))
{//=================================
    wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

    t_retrieve = new wxCheckBox(this, -1, _T("Auto retrieve 5 minute energy data"));
    t_set_time = new wxChoice(this, -1, wxDefaultPosition, wxDefaultSize, N_SYNC_CHOICES, sync_choices);

    wxBoxSizer *sync_row = new wxBoxSizer(wxHORIZONTAL);
    sync_row->Add(new wxStaticText(this, -1, _T("Sync inverter clock to computer time")), 0, wxALIGN_CENTER_VERTICAL);
    sync_row->Add(t_set_time, 0, wxLEFT, DLG_GAP);
    button_OK = new wxButton(this, dlgOK, _T("OK"));
    button_OK->SetDefault();
    button_Cancel = new wxButton(this, dlgCancel, _T("Cancel"));

    top->Add(t_retrieve, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, DLG_BORDER);
    top->Add(sync_row, 0, wxLEFT | wxRIGHT | wxTOP, DLG_BORDER);
    top->Add(new wxStaticText(this, -1, _T("It also syncs when daylight saving starts or ends, unless this is set to Never.")),
             1, wxEXPAND | wxALL, DLG_BORDER);
    top->Add(MakeButtonRow(button_OK, button_Cancel), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, DLG_BORDER);
    SetSizerAndFit(top);
}

void DlgInverter::ShowDlg(int inv)
{//===============================
    inverter = inv;
    t_retrieve->SetValue(inverters[inv].auto_retrieve);
    // the nearest choice, the config file may have another value
    int sync = 0;
    for(int ix=0; ix<N_SYNC_CHOICES; ix++)
    {
        if(inverters[inv].auto_set_time >= sync_seconds[ix])
            sync = ix;
    }
    t_set_time->SetSelection(sync);
    ShowModal();
}


void DlgInverter::OnButton(wxCommandEvent &event)
{//==============================================
    int id;

    id = event.GetId();
    if(id == dlgOK)
    {
        inverters[inverter].auto_retrieve = t_retrieve->GetValue();
        int sync_interval = sync_seconds[t_set_time->GetSelection()];
        if(sync_interval != inverters[inverter].auto_set_time)
        {
            // check the clock as soon as the option is changed
            inverters[inverter].last_clock_check = 0;
        }
        inverters[inverter].auto_set_time = sync_interval;
        ConfigSave(1);
    }
    Show(false);
}


//=======================================================================================================================


/************************************************************
                Histogram Dialog
*************************************************************/

BEGIN_EVENT_TABLE(DlgHistogram, wxDialog)
	EVT_BUTTON(dlgOK, DlgHistogram::OnButton)
	EVT_BUTTON(dlgCancel, DlgHistogram::OnButton)
END_EVENT_TABLE()


extern wxString BackColours[N_BACK_COLOURS];
double histogram_max;
int histogram_flags;

DlgHistogram::DlgHistogram(wxWindow *parent)
    : wxDialog(parent, -1, _T("Energy Histogram"))
{//=================================
    wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);
    wxFlexGridSizer *grid = new wxFlexGridSizer(2, DLG_GAP, 2*DLG_GAP);
    wxBoxSizer *daily = new wxBoxSizer(wxHORIZONTAL);

    t_dailymax = new wxTextCtrl(this, -1, data_dir, wxDefaultPosition, wxSize(80, -1));
    t_back_colour = new wxChoice(this, idBackColour, wxDefaultPosition, wxDefaultSize, N_BACK_COLOURS, BackColours);

    daily->Add(t_dailymax);
    daily->Add(new wxStaticText(this, -1, _T("kW")), 0, wxLEFT | wxALIGN_CENTER_VERTICAL, DLG_GAP);

    AddGridLabel(this, grid, _T("Maximum daily energy"));
    grid->Add(daily);
    AddGridLabel(this, grid, _T("Background"));
    grid->Add(t_back_colour);

    button_OK = new wxButton(this, dlgOK, _T("OK"));
    button_OK->SetDefault();
    button_Cancel = new wxButton(this, dlgCancel, _T("Cancel"));

    top->Add(grid, 1, wxEXPAND | wxALL, DLG_BORDER);
    top->Add(MakeButtonRow(button_OK, button_Cancel), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, DLG_BORDER);
    SetSizerAndFit(top);
}

void DlgHistogram::ShowDlg()
{//=========================

     t_dailymax->ChangeValue(wxString::Format(_T("%4.1f"), histogram_max));
     t_back_colour->SetSelection(histogram_flags);
     ShowModal();
}


void DlgHistogram::OnButton(wxCommandEvent &event)
{//=============================================
    int id;

    id = event.GetId();
    if(id == dlgOK)
    {
        t_dailymax->GetValue().ToDouble(&histogram_max);
        histogram_flags = t_back_colour->GetSelection();
        ConfigSave(1);
        graph_panel->SetMode(-1);
        graph_panel->Refresh();
    }
    Show(false);
}



//=======================================================================================================================


/************************************************************
                Retrieve Daily Energy Dialog
*************************************************************/

BEGIN_EVENT_TABLE(DlgRetrieveEnergy, wxDialog)
	EVT_BUTTON(dlgOK, DlgRetrieveEnergy::OnButton)
	EVT_BUTTON(dlgCancel, DlgRetrieveEnergy::OnButton)
END_EVENT_TABLE()

wxProgressDialog *progress_retrieval = NULL;


DlgRetrieveEnergy::DlgRetrieveEnergy(wxWindow *parent)
    : wxDialog(parent, -1, wxEmptyString)
{//=================================
    wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);

    MakeInverterNames(1);

    t_title = new wxStaticText(this, -1, _T("Retrieve 10 second energy data"));
    t_path = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(460, -1));
    t_option1 = new wxCheckBox(this, -1, _T("5 minute data for PVOutput.org "));

    button_OK = new wxButton(this, dlgOK, _T("OK"));
    button_OK->SetDefault();
    button_Cancel = new wxButton(this, dlgCancel, _T("Cancel"));

    top->Add(t_title, 0, wxALL, DLG_BORDER);
    top->Add(new wxStaticText(this, -1, _T("Write to")), 0, wxLEFT | wxRIGHT, DLG_BORDER);
    top->Add(t_path, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, DLG_BORDER/2);
    top->Add(t_option1, 1, wxALL, DLG_BORDER);
    top->Add(MakeButtonRow(button_OK, button_Cancel), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, DLG_BORDER);
    SetSizerAndFit(top);

    option_verbose = 0;
    option_pvoutput = 0;
}


void DlgRetrieveEnergy::ShowDlg(int inv, int type)
{//===============================================
    wxString fname;
    wxString path;
    int addr;

    retrieve_type = type;
    retrieve_inverter = inv;
    addr = inverter_address[inv];

    SetTitle(wxString::Format(_T("Retrieve Data from Inverter #%d"), inverter_address[inv]));
    if(retrieving_data != 0)
    {
        wxLogError(_T("Data retrieval is already in progress"));
        return;
    }

    if(type == cmdInverterDailyEnergy)
    {
        t_option1->SetLabel(_T("Verbose"));
        t_option1->SetValue(option_verbose);
        t_title->SetLabel(_T("Retrieve daily energy data"));
#ifdef __WXMSW__
        fname = wxString::Format(_T("\\daily%d_"), addr);
#else
        fname = wxString::Format(_T("/daily%d_"), addr);
#endif
    }
    else
    if(type == cmdInverter10SecEnergy)
    {
        t_option1->SetLabel(_T("5 minute data for PVOutput.org"));
        t_option1->SetValue(option_pvoutput);
        t_title->SetLabel(_T("Retrieve 10 second energy data"));
#ifdef __WXMSW__
        fname = wxString::Format(_T("\\r%d_"), addr);
#else
        fname = wxString::Format(_T("/r%d_"), addr);
#endif
    }

    if(path_retrieve == wxEmptyString)
        path = data_dir;
    else
        path = path_retrieve;

    t_path->ChangeValue(path + fname + date_ymd_string + _T(".txt"));

    Layout();   // the labels may have changed
    ShowModal();
}


void DlgRetrieveEnergy::OnButton(wxCommandEvent &event)
{//=============================================
    wxString path;

    if(event.GetId() == dlgOK)
    {
        path = wxFileName(t_path->GetValue()).GetPath();
        if(wxDirExists(path) == false)
        {
            wxLogError(_T("Directory not found:\n%s"), path);
            return;
        }
        path_retrieve = path;
        ConfigSave(1);

        retrieving_progress = 0;
        retrieving_finished = 0;
        retrieving_fname = t_path->GetValue();

        if(retrieve_type == cmdInverterDailyEnergy)
        {
            retrieve_message = _T("daily energy data");
            option_verbose = t_option1->GetValue();
        }
        else
        if(retrieve_type == cmdInverter10SecEnergy)
        {
            retrieve_message = _T("10 second energy data");
            option_pvoutput = t_option1->GetValue();
            retrieving_pvoutput = option_pvoutput;
        }

        if(QueueCommand(retrieve_inverter, retrieve_type) == 0)
        {
            progress_retrieval = new wxProgressDialog(_T("Aurora Monitor"), _T("Retrieving ") + retrieve_message, 100, mainframe, wxPD_CAN_ABORT | wxSTAY_ON_TOP);
        }
    }
    Show(false);
}




//=======================================================================================================================


/************************************************************
                Pvoutput Dialog
*************************************************************/

BEGIN_EVENT_TABLE(DlgPvoutput, wxDialog)
	EVT_BUTTON(dlgOK, DlgPvoutput::OnButton)
	EVT_BUTTON(dlgCancel, DlgPvoutput::OnButton)
	EVT_BUTTON(dlgTest, DlgPvoutput::OnButton)
END_EVENT_TABLE()


PVOUTPUT pvoutput;

wxString pvoutput_choices[4] = {_T("Off"), _T("5 minutes"), _T("10 minutes"), _T("15 minutes")};

DlgPvoutput::DlgPvoutput(wxWindow *parent)
    : wxDialog(parent, -1, _T("PVOutput.org"))
{//=================================
    wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);
    wxFlexGridSizer *grid = new wxFlexGridSizer(2, DLG_GAP, 2*DLG_GAP);
    wxBoxSizer *options = new wxBoxSizer(wxVERTICAL);

    t_url = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(380, -1));
    t_system_id = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(110, -1));
    t_api_key = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(380, -1));
    t_live_updates = new wxChoice(this, -1, wxDefaultPosition, wxDefaultSize, 4, pvoutput_choices);
    t_instantaneous = new wxCheckBox(this, -1, _T("Include Instantaneous Power"));
    t_grid_voltage = new wxCheckBox(this, -1, _T("Include Grid Voltage"));
    t_temperature = new wxCheckBox(this, -1, _T("Include Inverter Temperature"));
    button_test = new wxButton(this, dlgTest, _T("Test connection"));
    // room for a few lines of result, which can't then change the size of the dialog
    t_test_result = new wxStaticText(this, -1, wxEmptyString, wxDefaultPosition, wxSize(380, 3*GetCharHeight()), wxST_NO_AUTORESIZE);

    options->Add(t_instantaneous);
    options->Add(t_grid_voltage, 0, wxTOP, DLG_GAP/2);
    options->Add(t_temperature, 0, wxTOP, DLG_GAP/2);

    grid->AddGrowableCol(1);
    AddGridLabel(this, grid, _T("URL"));
    grid->Add(t_url, 0, wxEXPAND);
    AddGridLabel(this, grid, _T("System Id"));
    grid->Add(t_system_id);
    AddGridLabel(this, grid, _T("API Key"));
    grid->Add(t_api_key, 0, wxEXPAND);
    AddGridLabel(this, grid, _T("Live updates"));
    grid->Add(t_live_updates);
    grid->AddSpacer(1);
    grid->Add(options);
    grid->AddSpacer(1);
    grid->Add(button_test);
    grid->AddSpacer(1);
    grid->Add(t_test_result, 0, wxEXPAND);

    button_OK = new wxButton(this, dlgOK, _T("OK"));
    button_OK->SetDefault();
    button_Cancel = new wxButton(this, dlgCancel, _T("Cancel"));

    top->Add(grid, 1, wxEXPAND | wxALL, DLG_BORDER);
    top->Add(MakeButtonRow(button_OK, button_Cancel), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, DLG_BORDER);
    SetSizerAndFit(top);
}


void DlgPvoutput::ShowDlg()
{//========================
    t_url->ChangeValue(pvoutput.url);
    t_system_id->ChangeValue(pvoutput.sid);
    t_api_key->ChangeValue(pvoutput.key);
    t_live_updates->SetSelection(pvoutput.live_updates & PVO_PERIOD);
    t_instantaneous->SetValue(pvoutput.live_updates & PVO_INSTANTANEOUS);
    t_grid_voltage->SetValue(pvoutput.live_updates & PVO_GRID_VOLTAGE);
    t_temperature->SetValue(pvoutput.live_updates & PVO_TEMPERATURE);
    t_test_result->SetLabelText(wxEmptyString);
    button_test->Enable();
    ShowModal();
    PvoTestCancel();   // however the dialog was closed
}


void DlgPvoutput::OnButton(wxCommandEvent &event)
{//==============================================

    if(event.GetId() == dlgTest)
    {
        // test what's in the dialog, which may not have been saved
        PvoTestStart(t_url->GetValue(), t_system_id->GetValue(), t_api_key->GetValue(), t_test_result, button_test);
        return;
    }

    if(event.GetId() == dlgOK)
    {
        pvoutput.url = t_url->GetValue();
        if(pvoutput.url.Right(1) != _T("/"))
            pvoutput.url += _T("/");
        pvoutput.sid = t_system_id->GetValue();
        pvoutput.key = t_api_key->GetValue();
        pvoutput.live_updates = t_live_updates->GetSelection();
        if(t_instantaneous->GetValue())
            pvoutput.live_updates |= PVO_INSTANTANEOUS;
        if(t_grid_voltage->GetValue())
            pvoutput.live_updates |= PVO_GRID_VOLTAGE;
        if(t_temperature->GetValue())
            pvoutput.live_updates |= PVO_TEMPERATURE;
        pvoutput.failed = 0;
        ConfigSave(1);
        PvoResetStatus();
    }
    Show(false);
}



//=======================================================================================================================


/************************************************************
                MQTT Dialog
*************************************************************/

BEGIN_EVENT_TABLE(DlgMqtt, wxDialog)
	EVT_BUTTON(dlgOK, DlgMqtt::OnButton)
	EVT_BUTTON(dlgCancel, DlgMqtt::OnButton)
	EVT_BUTTON(dlgTest, DlgMqtt::OnButton)
END_EVENT_TABLE()


#define N_MQTT_INTERVALS  4
static const int mqtt_intervals[N_MQTT_INTERVALS] = {5, 10, 30, 60};
static wxString mqtt_interval_choices[N_MQTT_INTERVALS] = {_T("5 seconds"), _T("10 seconds"), _T("30 seconds"), _T("1 minute")};

DlgMqtt::DlgMqtt(wxWindow *parent)
    : wxDialog(parent, -1, _T("MQTT (Home Assistant)"))
{//=================================
    wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);
    wxFlexGridSizer *grid = new wxFlexGridSizer(2, DLG_GAP, 2*DLG_GAP);

    t_enabled = new wxCheckBox(this, -1, _T("Publish the readings to an MQTT broker"));
    t_host = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(280, -1));
    t_port = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(80, -1));
    t_username = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(200, -1));
    t_password = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(200, -1), wxTE_PASSWORD);
    t_prefix = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(200, -1));
    t_discovery = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(200, -1));
    t_interval = new wxChoice(this, -1, wxDefaultPosition, wxDefaultSize, N_MQTT_INTERVALS, mqtt_interval_choices);
    button_test = new wxButton(this, dlgTest, _T("Test connection"));
    // room for a few lines of result, which can't then change the size of the dialog
    t_test_result = new wxStaticText(this, -1, wxEmptyString, wxDefaultPosition, wxSize(380, 3*GetCharHeight()), wxST_NO_AUTORESIZE);

    grid->AddGrowableCol(1);
    AddGridLabel(this, grid, _T("Broker address"));
    grid->Add(t_host, 0, wxEXPAND);
    AddGridLabel(this, grid, _T("Port"));
    grid->Add(t_port);
    AddGridLabel(this, grid, _T("User name"));
    grid->Add(t_username);
    AddGridLabel(this, grid, _T("Password"));
    grid->Add(t_password);
    AddGridLabel(this, grid, _T("Topic prefix"));
    grid->Add(t_prefix);
    AddGridLabel(this, grid, _T("Discovery prefix"));
    grid->Add(t_discovery);
    AddGridLabel(this, grid, _T("Publish every"));
    grid->Add(t_interval);
    grid->AddSpacer(1);
    grid->Add(new wxStaticText(this, -1, _T("The connection isn't encrypted, so use a broker on your own network.")));
    grid->AddSpacer(1);
    grid->Add(button_test);
    grid->AddSpacer(1);
    grid->Add(t_test_result, 0, wxEXPAND);

    button_OK = new wxButton(this, dlgOK, _T("OK"));
    button_OK->SetDefault();
    button_Cancel = new wxButton(this, dlgCancel, _T("Cancel"));

    top->Add(t_enabled, 0, wxLEFT | wxRIGHT | wxTOP, DLG_BORDER);
    top->Add(grid, 1, wxEXPAND | wxALL, DLG_BORDER);
    top->Add(MakeButtonRow(button_OK, button_Cancel), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, DLG_BORDER);
    SetSizerAndFit(top);
}


void DlgMqtt::ShowDlg()
{//====================
    int ix;

    t_enabled->SetValue(mqtt.enabled != 0);
    t_host->ChangeValue(mqtt.host);
    t_port->ChangeValue(wxString::Format(_T("%d"), mqtt.port));
    t_username->ChangeValue(mqtt.username);
    t_password->ChangeValue(mqtt.password);
    t_prefix->ChangeValue(mqtt.prefix);
    t_discovery->ChangeValue(mqtt.discovery);
    t_interval->SetSelection(1);
    for(ix = 0; ix < N_MQTT_INTERVALS; ix++)
    {
        if(mqtt_intervals[ix] == mqtt.interval)
            t_interval->SetSelection(ix);
    }
    t_test_result->SetLabelText(wxEmptyString);
    button_test->Enable();
    ShowModal();
    MqttTestCancel();   // however the dialog was closed
}


// Returns an error message, or an empty string if the settings are OK.  The port is 0 if it isn't valid.
wxString DlgMqtt::GetSettings(MQTT_SETTINGS &settings)
{//===================================================
    long port;

    settings.port = 0;
    if(!t_port->GetValue().Trim().Trim(false).ToLong(&port) || (port < 1) || (port > 65535))
        return(_T("The port must be a number from 1 to 65535, normally 1883."));

    settings.enabled = t_enabled->GetValue() ? 1 : 0;
    settings.host = t_host->GetValue().Trim().Trim(false);
    settings.port = (int)port;
    settings.username = t_username->GetValue();
    settings.password = t_password->GetValue();
    settings.prefix = t_prefix->GetValue().Trim().Trim(false);
    settings.discovery = t_discovery->GetValue().Trim().Trim(false);
    settings.interval = mqtt_intervals[t_interval->GetSelection()];

    if(settings.enabled && settings.host.empty())
        return(_T("Enter the broker's address, or untick the box to stop publishing."));
    return(wxEmptyString);
}


void DlgMqtt::OnButton(wxCommandEvent &event)
{//==========================================
    MQTT_SETTINGS settings;
    wxString error;

    if(event.GetId() == dlgTest)
    {
        // test what's in the dialog, which may not have been saved
        // only the port needs to be valid, the test reports a missing address itself
        error = GetSettings(settings);
        if(settings.port == 0)
        {
            t_test_result->SetLabelText(error);
            t_test_result->Wrap(t_test_result->GetSize().x);
            return;
        }
        MqttTestStart(settings, t_test_result, button_test);
        return;
    }

    if(event.GetId() == dlgOK)
    {
        error = GetSettings(settings);
        if(!error.empty())
        {
            t_test_result->SetLabelText(error);
            t_test_result->Wrap(t_test_result->GetSize().x);
            return;    // leave the dialog open
        }
        mqtt = settings;
        ConfigSave(1);
        MqttRestart();
    }
    Show(false);
}



//=======================================================================================================================


/************************************************************
                SetTime Dialog
*************************************************************/

BEGIN_EVENT_TABLE(DlgSetTime, wxDialog)
	EVT_BUTTON(dlgOK, DlgSetTime::OnButton)
	EVT_BUTTON(dlgCancel, DlgSetTime::OnButton)
    EVT_TIMER(idSetTimeTimer, DlgSetTime::OnTimer)
END_EVENT_TABLE()


DlgSetTime::DlgSetTime(wxWindow *parent)
    : wxDialog(parent, -1, _T("Set Time"))
{//=================================
    static wxString radio_names[2] = {_T("Set to specified time "), _T("Set to computer time ")};

    wxBoxSizer *top = new wxBoxSizer(wxVERTICAL);
    wxFlexGridSizer *times = new wxFlexGridSizer(3, DLG_GAP, 2*DLG_GAP);
    wxFlexGridSizer *new_time = new wxFlexGridSizer(2, DLG_GAP, 2*DLG_GAP);
    const wxSize date_size(GetTextExtent(_T("No response ")).GetWidth() + 8, -1);

    t_computer_date = new wxStaticText(this, -1, wxEmptyString, wxDefaultPosition, date_size);
    t_computer_time = new wxStaticText(this, -1, wxEmptyString, wxDefaultPosition, date_size);
    t_inverter_date = new wxStaticText(this, -1, wxEmptyString, wxDefaultPosition, date_size);
    t_inverter_time = new wxStaticText(this, -1, wxEmptyString, wxDefaultPosition, date_size);

    AddGridLabel(this, times, _T("Computer time"));
    times->Add(t_computer_date);
    times->Add(t_computer_time);
    AddGridLabel(this, times, _T("Inverter time"));
    times->Add(t_inverter_date);
    times->Add(t_inverter_time);

    t_radiobox = new wxRadioBox(this, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, 2, radio_names, 1, wxRA_SPECIFY_COLS);
    t_new_date = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(130, -1));
    t_new_time = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(130, -1));

    AddGridLabel(this, new_time, _T("Specified date"));
    new_time->Add(t_new_date);
    AddGridLabel(this, new_time, _T("Specified time"));
    new_time->Add(t_new_time);

    button_OK = new wxButton(this, dlgOK, _T("OK"));
    button_OK->SetDefault();
    button_Cancel = new wxButton(this, dlgCancel, _T("Cancel"));

    top->Add(times, 0, wxALL, DLG_BORDER);
    top->Add(t_radiobox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, DLG_BORDER);
    top->Add(new_time, 1, wxLEFT | wxRIGHT | wxBOTTOM, DLG_BORDER);
    top->Add(MakeButtonRow(button_OK, button_Cancel), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, DLG_BORDER);
    SetSizerAndFit(top);

    settime_timer.SetOwner(this->GetEventHandler(), idSetTimeTimer);
}



void DlgSetTime::OnTimer(wxTimerEvent& WXUNUSED(event))
{//====================================================
    wxDateTime datetime;
    time_t computer_time;
    time_t inverter_time;
    const int need_time_offset = inverters[inverter].shown_need_time_offset;
    const int time_offset = inverters[inverter].shown_time_offset;

    if(this->IsShown() == false)
    {
        settime_timer.Stop();
    }

    computer_time = time(NULL);
    datetime.Set(computer_time);
    t_computer_date->SetLabel(datetime.FormatISODate());
    t_computer_time->SetLabel(datetime.FormatISOTime());
    if(t_new_date->GetValue() == wxEmptyString)
        t_new_date->ChangeValue(datetime.FormatISODate());

    if(need_time_offset != 0)
    {
        t_inverter_time->SetLabel(wxEmptyString);
        if(need_time_offset < 0)
            t_inverter_date->SetLabel(_T("No response"));
        else
            t_inverter_date->SetLabel(_T("Waiting..."));
    }
    else
    {
        inverter_time = computer_time + time_offset;
        datetime.Set(inverter_time);
        t_inverter_date->SetLabel(datetime.FormatISODate());
        t_inverter_time->SetLabel(datetime.FormatISOTime());
    }
}


void DlgSetTime::ShowDlg(int inv)
{//==============================
    wxTimerEvent null_event;

    inverter = inv;
    inverters[inv].shown_need_time_offset = 2;
    OnTimer(null_event);
    settime_timer.Start(250, wxTIMER_CONTINUOUS);
    QueueCommand(inv, cmdGetInvTime);
    {
        wxMutexLocker lock(comms_mutex);
        inverters[inv].need_time_offset = 2;  // pending
    }
    inverters[inv].shown_need_time_offset = 2;
    ShowModal();
}


void DlgSetTime::OnButton(wxCommandEvent &event)
{//=============================================
    int action;
    int n1, n2;
    int year, month;
    struct tm btime;
    time_t new_time;
    char buf[80];

    if(event.GetId() == dlgOK)
    {
        action = t_radiobox->GetSelection();
        if(action == 0)
        {
            memset(&btime, 0, sizeof(btime));
            btime.tm_isdst = -1;   // mktime() finds whether daylight saving applies on this date
            strncpy0(buf, t_new_date->GetValue().mb_str(wxConvLocal), sizeof(buf));
            n1 = sscanf(buf, "%d-%d-%d", &year, &month, &btime.tm_mday);
            btime.tm_year = year-1900;
            btime.tm_mon = month-1;

            strncpy0(buf, t_new_time->GetValue().mb_str(wxConvLocal), sizeof(buf));
            n2 = sscanf(buf, "%d:%d:%d", &btime.tm_hour, &btime.tm_min, &btime.tm_sec);
            new_time = mktime(&btime);

            if((n1 != 3) || (n2 != 3) || (new_time == (time_t)(-1)))
            {
                wxLogError(_T("Invalid date and time"));
                return;
            }
            settime_offset = new_time - time(NULL);
        }
        else
        {
            settime_offset = 0;
        }
        QueueCommand(inverter, cmdSetInvTime2);

    }
    settime_timer.Stop();
    Show(false);
}
