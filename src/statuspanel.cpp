
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

#include <wx/aboutdlg.h>
#include <wx/colordlg.h>
#include <wx/filename.h>
#include <wx/sizer.h>
#include <wx/gbsizer.h>
#include <wx/wrapsizer.h>
#include "auroramon.h"


wxFont font_big(24, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
wxFont font_medium(19, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);

#ifdef __WXMSW__
#define TE_BIG   wxTE_RICH | wxTE_DONTWRAP
#else
#define TE_BIG   wxTE_MULTILINE | wxTE_DONTWRAP
#endif



// The status lights have their own window, and are drawn with paint events
// (a wxClientDC doesn't draw on Wayland)
static wxPanel *pulse_panel = NULL;
static int pulse_colour[N_INV] = {0, 0};
static bool pulse_bright[N_INV] = {false, false};
#define PULSE_SIZE  18


void DrawStatusPulse(int pulse)
{//============================
    int state;
    int colour;
    int inv;

    static int colour_on[3] =  {0x0020ff, 0x00e800, 0x12c0ff};  // red, green, yellow
    static int colour_off[3] = {0x0000c0, 0x087808, 0x00aacc};

    for(inv = 0; inv < 2; inv++)
    {
        if(inverter_address[inv] > 0)
        {
            if((state = inverters[inv].alive) > 0)
            {
                if(inverters[inv].alarm != 0)
                    state = 2;
            }

            if(pulse & (1 << inv))
            {
                colour = colour_on[state];
                if(inverters[inv].comms_error)
                {
                    if(state == 1)
                        colour = 0x12c0ff;   // green, flashes yellow = retry
                    inverters[inv].comms_error = 0;
                }
                else
                if((retrieving_data > 0) && (retrieving_inv == inv))
                {
                    if(state == 1)
                        colour = 0xffe838;  // green, flashes cyan = retrieving data
                }
                pulse_colour[inv] = colour;
                pulse_bright[inv] = true;
            }
            else
            {
                pulse_colour[inv] = colour_off[state];
                pulse_bright[inv] = false;
            }
        }
    }
    pulse_panel->Refresh();
}


static void OnPulsePaint(wxPaintEvent& WXUNUSED(event))
{//====================================================
    int inv;
    int y;
    int height = pulse_panel->GetClientSize().GetHeight();
    wxPaintDC dc(pulse_panel);

    for(inv = 0; inv < 2; inv++)
    {
        if(inverter_address[inv] > 0)
        {
            // one in each half of the height
            y = ((height/2) - PULSE_SIZE)/2 + inv*(height/2);
            dc.SetPen(*wxBLACK_PEN);
            dc.SetBrush(wxBrush(wxColour(pulse_colour[inv])));
            if(pulse_bright[inv])
                dc.DrawRectangle(1, y+1, PULSE_SIZE-2, PULSE_SIZE-2);
            else
                dc.DrawRectangle(0, y, PULSE_SIZE, PULSE_SIZE);
        }
    }
}


void Mainframe::OnPulseTimer(wxTimerEvent& WXUNUSED(event))
{//==============================================
    DrawStatusPulse(0);
}


void Mainframe::OnLogStatusTimer(wxTimerEvent& WXUNUSED(event))
{//==============================================
    wxLogStatus(wxEmptyString);
}


// Add a group of readings to the status panel, after the ones already placed
void Mainframe::PlaceGroup(wxSizer *group)
{//=======================================
    sizer_status->Add(group, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 2*DLG_GAP);
    group->ShowItems(true);
}


void Mainframe::PlaceExtraReadings(int inv)
{//========================================
    int ix;
    int posn = 0;
    wxString label;

    PlaceGroup(sizer_extra[inv]);

    for(ix=0; ix < N_EXTRA_READINGS; ix++)
    {
        if(extra_readings[ix].status_slot != 0)
        {
            label = wxString(extra_reading_types[extra_readings[ix].type].mnemonic, wxConvLocal);
            label.Replace(_T("_"), _T(" "));
            txt_dsp_label[inv][posn]->SetLabel(label);
            extra_readings[ix].status_slot = posn+1;
            posn++;
        }
    }
    while(posn < N_EXTRA_READINGS)
    {
        txt_dsp_param[inv][posn]->Show(false);
        txt_dsp_label[inv][posn]->Show(false);
        posn++;
    }
}


void Mainframe::SetupStatusPanel(int control)
{//=========================================
// control bit 0: inverter addresses may have changed

// style 0:  show all
//         1:  show some
//         2:  show all inverter 0
//         3:  show all inverter 1

    int ix;
    int style;

    if(control & 1)
    {
        for(ix=0; ix<N_INV; ix++)
        {
            if(inverter_address[ix] == 0)
            {
                power_total[ix] = 0;
                inverters[ix].alive = 0;
            }
            else
            {
                txt_static[ix][0]->SetLabel(wxString::Format(_T("Inverter #%d"), inverter_address[ix]));
            }
        }
    }

    if(control & 2)
    {
        for(ix=0; ix < N_EXTRA_READINGS; ix++)
        {
            txt_dsp_param[0][ix]->Clear();
            txt_dsp_param[1][ix]->Clear();
        }
    }

    style = chart_pages[graph_panel->page].status_type;

    status_panel->Freeze();

    // take out all the groups (Detach doesn't delete them), and then place the ones for this style
    for(ix=0; ix<N_INV; ix++)
    {
        wxSizer *groups[4] = {sizer_power[ix], sizer_input[ix], sizer_energy[ix], sizer_extra[ix]};
        for(int g=0; g<4; g++)
        {
            sizer_status->Detach(groups[g]);
            groups[g]->ShowItems(false);
        }
    }
    sizer_status->Detach(sizer_today);
    sizer_today->ShowItems(false);

    if(inverter_address[1] == 0)
    {
        // only one inverter;
        PlaceGroup(sizer_input[0]);
        PlaceGroup(sizer_energy[0]);
        PlaceExtraReadings(0);
    }
    else
    if(style == 0)
    {
        // all
        PlaceGroup(sizer_power[0]);
        PlaceGroup(sizer_input[0]);
        PlaceGroup(sizer_power[1]);
        PlaceGroup(sizer_input[1]);
        PlaceGroup(sizer_energy[0]);
        PlaceGroup(sizer_energy[1]);
        PlaceExtraReadings(0);
    }
    else
    if(style == 1)
    {
        // some
        PlaceGroup(sizer_power[0]);
        PlaceGroup(sizer_input[0]);
        PlaceGroup(sizer_power[1]);
        PlaceGroup(sizer_input[1]);
        PlaceGroup(sizer_today);
        PlaceExtraReadings(0);
    }
    else
    if(style == 2)
    {
        // inverter A
        PlaceGroup(sizer_power[0]);
        PlaceGroup(sizer_input[0]);
        PlaceGroup(sizer_energy[0]);
        PlaceGroup(sizer_power[1]);
        PlaceExtraReadings(0);
    }
    else
    {
        // inverter B
        PlaceGroup(sizer_power[0]);
        PlaceGroup(sizer_power[1]);
        PlaceGroup(sizer_input[1]);
        PlaceGroup(sizer_energy[1]);
        PlaceExtraReadings(1);
    }

    for(ix=0; ix<2; ix++)
    {
        ShowEnergy(ix);
        txt_energy[ix][1]->ChangeValue(wxString::Format(_T("%d"), inverters[ix].peak));
    }

    status_panel->Layout();
    Layout();
    status_panel->Thaw();
    pulse_panel->Refresh();
}


// Size a text field to fit this sample of its contents, for the current font
static void FitTextField(wxTextCtrl *text, const wxString &sample, int extra_height)
{//=================================================================================
    wxSize size = text->GetTextExtent(sample);
    size.x += 2 * text->GetCharWidth();
    size.y = text->GetCharHeight() + extra_height;
    text->SetInitialSize(size);
}


static wxTextCtrl *NewField(wxWindow *parent, const wxString &sample)
{//==================================================================
    wxTextCtrl *text = new wxTextCtrl(parent, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CENTRE);
    FitTextField(text, sample, 8);
    return(text);
}


static wxTextCtrl *NewBigField(wxWindow *parent, wxFont &font, const wxString &sample)
{//===================================================================================
    wxTextCtrl *text = new wxTextCtrl(parent, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, TE_BIG | wxTE_READONLY | wxTE_CENTRE);
    text->SetFont(font);
    text->SetDefaultStyle(wxTextAttr(wxNullColour, wxNullColour, font));
    FitTextField(text, sample, 10);
    return(text);
}


static void AddField(wxSizer *grid, wxTextCtrl *field, wxStaticText *label)
{//========================================================================
    grid->Add(field, 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 3);
}


void Mainframe::MakeStatusPanel(void)
{//==================================
    int ix;
    int inv;
    wxFlexGridSizer *grid;
    wxGridBagSizer *bag;
    wxBoxSizer *box;

    status_panel->SetFont(wxFont(9, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

    // readings continue on another row if they don't fit across the window
    sizer_status = new wxWrapSizer(wxHORIZONTAL, wxREMOVE_LEADING_SPACES);

    txt_powertot = NewBigField(status_panel, font_big, _T("00000"));
    sizer_status->Add(txt_powertot, 0, wxALIGN_CENTER_VERTICAL | wxALL, 2);

    pulse_panel = new wxPanel(status_panel, -1, wxDefaultPosition, wxSize(PULSE_SIZE+1, 2*PULSE_SIZE + 6));
    pulse_panel->SetBackgroundStyle(wxBG_STYLE_SYSTEM);
    pulse_panel->Bind(wxEVT_PAINT, &OnPulsePaint);
    sizer_status->Add(pulse_panel, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, DLG_GAP);

    static const wxString energy_names[] = {_T("today"), _T("peakW"), _T("month"), _T("year"), _T("total"), _T("partial")};

    for(inv=0; inv<2; inv++)
    {
        // power output of this inverter
        txt_power[inv] = NewBigField(status_panel, font_medium, _T("00000"));
        txt_static[inv][0] = new wxStaticText(status_panel, -1, _T("Inverter #000"));
        box = new wxBoxSizer(wxVERTICAL);
        box->Add(txt_power[inv], 0, wxALIGN_CENTER_HORIZONTAL);
        box->Add(txt_static[inv][0], 0, wxALIGN_CENTER_HORIZONTAL);
        sizer_power[inv] = box;

        // power, voltage and current of the inputs.  Efficiency and temperature
        txt_power1[inv] = NewField(status_panel, _T("00000"));
        txt_power2[inv] = NewField(status_panel, _T("00000"));
        txt_volts1[inv] = NewField(status_panel, _T("0000"));
        txt_volts2[inv] = NewField(status_panel, _T("0000"));
        txt_current1[inv] = NewField(status_panel, _T("00.00"));
        txt_current2[inv] = NewField(status_panel, _T("00.00"));
        txt_efficiency[inv] = NewField(status_panel, _T("100.0"));
        txt_temperature[inv] = NewField(status_panel, _T("100.0"));

        txt_static[inv][2] = new wxStaticText(status_panel, -1, _T("W"));
        txt_static[inv][3] = new wxStaticText(status_panel, -1, _T("W"));
        txt_static[inv][4] = new wxStaticText(status_panel, -1, _T("V"));
        txt_static[inv][5] = new wxStaticText(status_panel, -1, _T("V"));
        txt_static[inv][6] = new wxStaticText(status_panel, -1, _T("A"));
        txt_static[inv][7] = new wxStaticText(status_panel, -1, _T("A"));
        txt_static[inv][8] = new wxStaticText(status_panel, -1, _T("%"));
        txt_static[inv][9] = new wxStaticText(status_panel, -1, _T("deg"));

        grid = new wxFlexGridSizer(8, 2, 2);
        AddField(grid, txt_power1[inv], txt_static[inv][2]);
        AddField(grid, txt_volts1[inv], txt_static[inv][4]);
        AddField(grid, txt_current1[inv], txt_static[inv][6]);
        AddField(grid, txt_efficiency[inv], txt_static[inv][8]);
        AddField(grid, txt_power2[inv], txt_static[inv][3]);
        AddField(grid, txt_volts2[inv], txt_static[inv][5]);
        AddField(grid, txt_current2[inv], txt_static[inv][7]);
        AddField(grid, txt_temperature[inv], txt_static[inv][9]);
        sizer_input[inv] = grid;

        // energy totals
        txt_static[inv][10] = new wxStaticText(status_panel, -1, _T("kWh"));
        for(ix=0; ix<6; ix++)
        {
            txt_energy[inv][ix] = NewField(status_panel, _T("000000.0"));
            txt_static[inv][ix+11] = new wxStaticText(status_panel, -1, energy_names[ix]);
        }
        grid = new wxFlexGridSizer(7, 2, 2);
        grid->Add(txt_static[inv][10], 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 3);
        for(ix=0; ix<6; ix+=2)
            AddField(grid, txt_energy[inv][ix], txt_static[inv][ix+11]);
        grid->AddSpacer(1);
        for(ix=1; ix<6; ix+=2)
            AddField(grid, txt_energy[inv][ix], txt_static[inv][ix+11]);
        sizer_energy[inv] = grid;

        // extra readings, placed in columns of two
        bag = new wxGridBagSizer(2, 2);
        for(ix=0; ix<N_EXTRA_READINGS; ix++)
        {
            txt_dsp_param[inv][ix] = NewField(status_panel, _T("0000.00"));
            txt_dsp_label[inv][ix] = new wxStaticText(status_panel, -1, _T("Grid Hz"));
            bag->Add(txt_dsp_param[inv][ix], wxGBPosition(ix & 1, (ix/2)*2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
            bag->Add(txt_dsp_label[inv][ix], wxGBPosition(ix & 1, (ix/2)*2 + 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 3);
        }
        sizer_extra[inv] = bag;
    }

    // today's energy of both inverters
    grid = new wxFlexGridSizer(2, 2, 2);
    for(inv=0; inv<2; inv++)
    {
        txt_today[inv] = NewField(status_panel, _T("000.000"));
        AddField(grid, txt_today[inv], new wxStaticText(status_panel, -1, _T("kWh")));
    }
    sizer_today = grid;

    // each group needs a place in the sizer, SetupStatusPanel() arranges them
    for(inv=0; inv<2; inv++)
    {
        sizer_status->Add(sizer_power[inv]);
        sizer_status->Add(sizer_input[inv]);
        sizer_status->Add(sizer_energy[inv]);
        sizer_status->Add(sizer_extra[inv]);
    }
    sizer_status->Add(sizer_today);

    status_panel->SetSizer(sizer_status);
}  // end of MakeStatusPanal
