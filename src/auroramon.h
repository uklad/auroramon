
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


#ifndef AURORAMONMAIN_H
#define AURORAMONMAIN_H

#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include <wx/calctrl.h>
#include <wx/progdlg.h>
#include <wx/spinctrl.h>
#include <wx/thread.h>

#include <cctype>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/stat.h>
#ifndef __WXMSW__
#include <fcntl.h>
#include <unistd.h>
#endif


// #define FLASH_MEMORY   // write to files less often

enum
{
    idMenuInverterInfo  = 1000,
    idMenuInverterInfo1,
    idMenuInverterSetTime,
    idMenuInverterSetTime1,
    idMenuInverterResetPartial,
    idMenuInverterResetPartial1,
    idMenuInverterDailyEnergy,
    idMenuInverterDailyEnergy1,
    idMenuInverter10SecEnergy,
    idMenuInverter10SecEnergy1,
    idMenuAlarmLog,
    idMenuAlarmLog1,
    idMenu10secLog,
    idMenu10SecLog1,
    idMenuSetInverter,
    idMenuSetInverter1,

    idMenuQuit,
    idDisplayToday,
    idDisplayDate,
    idNextDate,
    idPrevDate,
    idImportStored,
    idDisplayEnergy,
    idMenuEditCharts,
    idMenuEditHistogram,
    idMenuSetup,
    idMenuLocation,
    idMenuExtraReadings,
    idMenuSunInfo,
    idMenuInverterState,
    idMenuPvoutput,
    idMenuMqtt,

    idMenuMessageLog,
    idMenuCommLog,
    idMenuPvoLog,

    idMenuHelp,
    idMenuFullscreen,
    idMenuExportPvo,
    idInverterResponse,
    idInverterData,
    idInverterFail,
    idInverterModelError,
    idMenuAbout,

	idStartupTimer,
    idPulseTimer,
    idLogStatusTimer,
    idSetTimeTimer,
    idRetryCommandTimer,

    dlgOK,
    dlgAccept,
    dlgCancel,
    dlgDelete,
    dlgInsert,
    dlgDefault,
    dlgTest,
    dlgSerialPort,
    dlgInverter1,
    dlgInverter2,
    dlgDataDir,

    idPageName,
    idPageSpin,
    idStatusType,
    idBackColour,
    idGraphA_1,
    idGraphA_2,
    idGraphA_3,
    idGraphA_4,
    idGraphA_5,
    idGraphB_1,
    idGraphB_2,
    idGraphB_3,
    idGraphB_4,
    idGraphB_5,
    idGraphC_1,
    idGraphC_2,
    idGraphC_3,
    idGraphC_4,
    idGraphC_5,
    idGraphD_1,
    idGraphD_2,
    idGraphD_3,
    idGraphD_4,
    idGraphD_5,
};

#define N_DATE_FORMATS  4
#define N_BACK_COLOURS   2

#define GLOBAL_STATE_RUN    6
#define INVERTER_STATE_RUN  2

#define IR_HAS_TIME    0x01
#define IR_HAS_POWERIN 0x02
#define IR_HAS_ENERGY  0x04
#define IR_HAS_ENERGY_TODAY 0x08
#define IR_HAS_TEMPR   0x10
#define IR_HAS_STATE   0x20
#define IR_HAS_EXTRA   0x80
#define IR_HAS_PEAK    0x100

#define N_DSP     65     // dsp measurement types, plus 64=Energy today

#define DSP_GRID_VOLTS    1
#define DSP_PEAK_TODAY   35

#define N_INV     2      // max. of 2 inverters
#define N_10SEC   8640   // number of 10 second periods in a day
#define MAX_GAP   18     // interpolate gaps of less than this, *10sec.
#define N_AVERAGES 7


typedef struct {
    int flags;
    time_t time;
//    int seconds;
    char timedate[18];
    float vt[3];
    float ct[3];
    float pw[3];
    float effic;
    float tempr[2];
    double energy[6];
    unsigned char state[6];
    float dsp[100];   // DSP measure values
    int n_av_pw0;
    int n_av_pwin;
    int n_av_tmpr;
} INVERTER_RESPONSE;


typedef struct {
    unsigned char address;
    int auto_retrieve;
    int peak;
    int alive;
    int alarm;
    int comms_error;
    int fails;
    int current_period;
    float temperature;
    int last_power_out;
    int last_power_out_time;
    int get_extra_dsp;
    int do_retrieve_data;
    FILE *f_log;
    int logheaders_changed;
    double previous_total;
    char today_done[10];
    double energy_total[6];
    int time_offset;
    int need_time_offset;
    int shown_time_offset;       // copies for the GUI, updated between polls
    int shown_need_time_offset;
    int auto_set_time;           // option: seconds between syncs of the inverter's clock, 0=never
    time_t last_clock_check;
    int clock_check_done;        // the result of this check has been dealt with
    int clock_set_failures;      // failed attempts to set the clock, during this check
    int previous_minute;
    int period_in_minute;
    unsigned int counters[4];
    char current_timestring[10];
    float averages[N_AVERAGES];
} INVERTER;

extern INVERTER inverters[N_INV];


#define X_PO    2        // max of 32.7 kW
#define X_PI    4        // max of 16.35 kW
#define X_VI    64       // max 1024 V

typedef struct {
    unsigned short pw0;  // total power * X_PO
    unsigned short pw1;  // string 1 power * X_PI
    unsigned short vt1;  // string 1 voltage * X_VI
    unsigned short pw2;  // string 2 power * X_PI
    unsigned short vt2;  // string 2 voltage * X_VI
    short tmpr;          // temperature * 100
    unsigned short pwi;  // stored power readings from the inverter
} DATAREC;

typedef struct {
    unsigned short time;
    unsigned short pw0;  // multipliers are the same as for DATAREC
    unsigned short pw1;
    unsigned short vt1;
    unsigned short pw2;
    unsigned short vt2;
    short tmpr;
    unsigned short spare;
} BINARY_LOG;

typedef struct {
    int sunrise;
    int sunset;
    int sunnoon;
    int noon_hrs;
    int noon_mins;
    double max_elev_day;
    double max_elev_year;
    double duration;
    double sunrise_time;   // fraction of day
    double sunrise_azim;   // dec cw from N
    double sunset_time;    // fraction of day
    double sunset_azim;    // dec cw from N
    double noon_azim;
} SUN;


#ifdef __WXMSW__
#define DLG_HX       27
#else
#define DLG_HX       0
#endif

#define N_EXTRA_READINGS  8

class Mainframe: public wxFrame
{//============================
    public:
        Mainframe(wxFrame *frame, const wxString& title);
        ~Mainframe();
        void SetupStatusPanel(int control);
        void MakeInverterMenu(int control);
        wxTimer logstatus_timer;
        wxTimer retry_command_timer;

    private:
        void OnClose(wxCloseEvent& event);
        void OnQuit();
        void OnAbout();
        void OnCommand(wxCommandEvent& event);
        void OnInverterEvent(wxCommandEvent &event);
        int DataResponse(int data_ok, INVERTER_RESPONSE *ir);
        void OnKey(wxKeyEvent& event);
        void OnStartupTimer(wxTimerEvent &event);
        void OnPulseTimer(wxTimerEvent &event);
        void OnLogStatusTimer(wxTimerEvent &event);
        void OnRetryCommandTimer(wxTimerEvent &event);
        void MakeMenus();
        void MakeStatusPanel();
        void PlaceGroup(wxSizer *group);
        void PlaceExtraReadings(int inv);
        void ShowEnergy(int inverter);
        void ShowHelp();

        wxTimer pulsetimer;
        wxTimer startuptimer;

        wxStaticText *txt_static[N_INV][20];
        wxTextCtrl *txt_powertot;
        wxTextCtrl *txt_power[N_INV];
        wxTextCtrl *txt_power1[N_INV];
        wxTextCtrl *txt_power2[N_INV];
        wxTextCtrl *txt_volts1[N_INV];
        wxTextCtrl *txt_volts2[N_INV];
        wxTextCtrl *txt_current1[N_INV];
        wxTextCtrl *txt_current2[N_INV];
        wxTextCtrl *txt_efficiency[N_INV];
        wxTextCtrl *txt_temperature[N_INV];
        wxTextCtrl *txt_energy[N_INV][6];
        wxTextCtrl *txt_today[N_INV];    // for the status bar style which only shows today's energy
        wxTextCtrl *txt_dsp_param[N_INV][N_EXTRA_READINGS];
        wxStaticText *txt_dsp_label[N_INV][N_EXTRA_READINGS];
        double power_total[N_INV];

        // the groups of readings in the status panel, which ones are shown depends on the chart page
        wxSizer *sizer_status;
        wxSizer *sizer_power[N_INV];
        wxSizer *sizer_input[N_INV];
        wxSizer *sizer_energy[N_INV];
        wxSizer *sizer_extra[N_INV];
        wxSizer *sizer_today;

        DECLARE_EVENT_TABLE()
};





#define N_HIST_SCALE 12
#define N_HISTOGRAM_PAGES 4
#define HIST_DAYS      0
#define HIST_DAYS_AVG  1
#define HIST_WEEKS     2
#define HIST_MONTHS    3

#define N_PAGES  14
#define N_PAGE_GRAPHS  5
#define N_POWERGRAPHS  5  // number of graph types with power values

enum {
    g_PowerTotal = 1,
    g_PowerOut,
    g_Power1,
    g_Power2,
    g_PowerMin12,

    g_PowerBalance,
    g_Voltage1,
    g_Voltage2,
    g_Current1,
    g_Current2,
    g_SunElevation,
    g_SolarIntensity,
    g_Temperature,
    g_Efficiency,
    g_GridVoltage,
};


#define gNONE  0
#define gLEFT  0x1
#define gRIGHT 0x2
#define gBACKG 0x3

#define gLINE   0x0
#define gSOLID  0x1
#define gTHICK  0x2
#define gTHICK2  0x3
#define gHATCH  0x4

#define gINUSE  0x1
#define gDATE   0x2
#define gBACK_COLOUR  0x10


typedef struct {
    unsigned char type;
    unsigned char inverter;
    unsigned char scaletype;
    unsigned char style;
    int colour;
    int base;
    int top;
} GRAPH;

typedef struct {
    int flags;
    int status_type;
    int range_ix;
    char name[80];
    GRAPH graph[N_PAGE_GRAPHS];
} CHART_PAGE;

extern unsigned short *dsp_data[N_INV][N_DSP];

class GraphPanel: public wxScrolledWindow
{//======================================
    public:
        GraphPanel(wxWindow *parent, const wxPoint& pos, const wxSize &size);
        virtual void OnDraw(wxDC &dc);
        int ReadEnergyFile(int inv, wxString filename, int control, const char *datastr);
        void AddPoint(int inverter, int time, DATAREC *datarec, int control);
        int OnKey2(wxKeyEvent& event);
        void ResetGraph(void);
        void PlotGraphs(wxDC &dc, int height, int pass);
        void ShowPage(int page);
        void NewDay(void);
        void SetMode(int mode);
		void SetColours(int black);

        int page;
        int start_ix;
        int top_ix;
        int enable_draw;

    private:
        void DrawGraph(wxDC &dc);
        void DrawEnergyHistogram(wxDC &dc);
        void OnMouse(wxMouseEvent& event);
        void OnKey(wxKeyEvent& event);
		wxString DateAtCursor(int x);
		void SetExtent();
		int HistogramExtent();

        int chart_mode;  // 0=power graphs, 1=energy histogram
        int newday;
        double x_scale;
        double xscale_hist;
        int hist_xscale_ix;
        int hist_range_ix;
        CHART_PAGE *chart_page;
        int nx,nx2;
        int histogram_page;
        int energy_n_days;
        int x_scroll, y_scroll;

        wxColour colour_backg;
        wxColour colour_grid1;
        wxColour colour_grid2;
        wxColour colour_text1;
        wxColour colour_text2;
        wxBitmap buffered_dc_bitmap;

        DECLARE_EVENT_TABLE()
};


class DlgSetup: public wxDialog
{//============================
    public:
        DlgSetup(wxWindow *parent);
        void ShowDlg(void);
    private:
        void OnButton(wxCommandEvent &event);

        wxButton *button_OK;
        wxButton *button_Cancel;
        wxTextCtrl *t_serialport;
        wxTextCtrl *t_inverter1;
        wxTextCtrl *t_inverter2;
        wxTextCtrl *t_data_dir;
        wxChoice *t_date_format;

    DECLARE_EVENT_TABLE()
};


#define N_PANEL_GROUPS   2

typedef struct {
    double tilt;
    double facing;
} PANEL_GROUP;

class DlgLocation: public wxDialog
{//============================
    public:
        DlgLocation(wxWindow *parent);
        void ShowDlg(void);
    private:
        void OnButton(wxCommandEvent &event);

        wxButton *button_OK;
        wxButton *button_Cancel;
        wxTextCtrl *t_longitude;
        wxTextCtrl *t_latitude;
        wxTextCtrl *t_timezone;
        wxCheckBox *t_tz_auto;
        wxTextCtrl *t_tilt[N_PANEL_GROUPS];
        wxTextCtrl *t_facing[N_PANEL_GROUPS];

    DECLARE_EVENT_TABLE()
};


class DlgChart: public wxDialog
{//============================
    public:
        DlgChart(wxWindow *parent);
        void ShowDlg(void);

    private:
        void OnButton(wxCommandEvent &event);
        void OnColourButton(wxCommandEvent &event);
        void OnGraphChoice(wxCommandEvent &event);
        void OnTextEnter(wxCommandEvent &event);
        void OnPageSpin(wxSpinEvent &event);
        void BuildGraphFields(int gnum);
        void ShowPage(int page);

        wxButton *button_OK;
        wxButton *button_Accept;
        wxButton *button_Cancel;
        wxButton *button_Delete;
        wxButton *button_Insert;
        wxButton *button_Default;

        wxSpinCtrl *t_pagenum;
        wxTextCtrl *t_pagename;
        wxChoice *t_statustype;
        wxChoice *t_back_colour;
        CHART_PAGE *cpage;
        int page;
        wxString fname_chartinfo;

    DECLARE_EVENT_TABLE()
};


class DlgHistogram: public wxDialog
{//================================
    public:
        DlgHistogram(wxWindow *parent);
        void ShowDlg(void);

    private:
        void OnButton(wxCommandEvent &event);

        wxButton *button_OK;
        wxButton *button_Cancel;
        wxTextCtrl *t_dailymax;
        wxChoice *t_back_colour;

    DECLARE_EVENT_TABLE()
};


class DlgInverter: public wxDialog
{//===============================
    public:
        DlgInverter(wxWindow *parent);
        void ShowDlg(int inv);

    private:
        int inverter;

        void OnButton(wxCommandEvent &event);
        wxButton *button_OK;
        wxButton *button_Cancel;
        wxCheckBox *t_retrieve;
        wxChoice *t_set_time;
        wxTextCtrl *t_scale10sec;
        wxTextCtrl *t_string_name[2];

    DECLARE_EVENT_TABLE()
};


class DlgRetrieveEnergy: public wxDialog
{//=====================================
    public:
        DlgRetrieveEnergy(wxWindow *parent);
        void ShowDlg(int inv, int type);
        wxTextCtrl *t_path;

        bool option_pvoutput;
        bool option_verbose;
    private:
        void OnButton(wxCommandEvent &event);

        wxButton *button_OK;
        wxButton *button_Cancel;

        wxStaticText *t_title;
        wxCheckBox *t_option1;

        int retrieve_type;
        int retrieve_inverter;
    DECLARE_EVENT_TABLE()
};

class DlgSelectDate: public wxDialog
{//=================================
    public:
        DlgSelectDate(wxWindow *parent);
        wxCalendarCtrl *t_calendar;

    private:
        void OnMonth(wxCalendarEvent &event);
        void OnDate(wxCalendarEvent &event);
        void ShowAvailableDates();

        wxButton *button_OK;
        wxButton *button_Cancel;

    DECLARE_EVENT_TABLE()
};


class DlgSetTime: public wxDialog
{//==============================
    public:
        DlgSetTime(wxWindow *parent);
        void ShowDlg(int inv);

    private:
        void OnButton(wxCommandEvent &event);
        void OnTimer(wxTimerEvent &event);

        wxTimer settime_timer;
        wxButton *button_OK;
        wxButton *button_Cancel;

        int inverter;
        wxStaticText *t_computer_date;
        wxStaticText *t_computer_time;
        wxStaticText *t_inverter_date;
        wxStaticText *t_inverter_time;
        wxRadioBox *t_radiobox;
        wxTextCtrl *t_new_date;
        wxTextCtrl *t_new_time;

    DECLARE_EVENT_TABLE()
};


#define PVO_PERIOD         0x07
#define PVO_INSTANTANEOUS  0x10
#define PVO_GRID_VOLTAGE   0x20
#define PVO_TEMPERATURE    0x40

typedef struct {
    wxString url;
    wxString sid;
    wxString key;
    int live_updates;
    int failed;
} PVOUTPUT;

extern PVOUTPUT pvoutput;

class DlgPvoutput: public wxDialog
{//================================
    public:
        DlgPvoutput(wxWindow *parent);
        void ShowDlg();

    private:
        void OnButton(wxCommandEvent &event);

        wxTextCtrl *t_url;
        wxTextCtrl *t_system_id;
        wxTextCtrl *t_api_key;
        wxChoice *t_live_updates;
        wxCheckBox *t_instantaneous;
        wxCheckBox *t_grid_voltage;
        wxCheckBox *t_temperature;
        wxButton *button_test;
        wxStaticText *t_test_result;

        wxButton *button_OK;
        wxButton *button_Cancel;

    DECLARE_EVENT_TABLE()
};


typedef struct {
    int enabled;
    wxString host;
    int port;
    wxString username;
    wxString password;
    wxString prefix;       // the start of the topics
    wxString discovery;    // Home Assistant's discovery prefix
    int interval;          // seconds between publishing the readings
} MQTT_SETTINGS;

extern MQTT_SETTINGS mqtt;

class DlgMqtt: public wxDialog
{//============================
    public:
        DlgMqtt(wxWindow *parent);
        void ShowDlg();

    private:
        void OnButton(wxCommandEvent &event);
        wxString GetSettings(MQTT_SETTINGS &settings);

        wxCheckBox *t_enabled;
        wxTextCtrl *t_host;
        wxTextCtrl *t_port;
        wxTextCtrl *t_username;
        wxTextCtrl *t_password;
        wxTextCtrl *t_prefix;
        wxTextCtrl *t_discovery;
        wxChoice *t_interval;
        wxButton *button_test;
        wxStaticText *t_test_result;

        wxButton *button_OK;
        wxButton *button_Cancel;

    DECLARE_EVENT_TABLE()
};


typedef struct {
    int dsp_code;
    const char *name;
    const char *mnemonic;
    const char *mnem2;
    unsigned char per_inverter;
    unsigned char decimalplaces;
    int default_range;
    int addition;
    int multiply;
} GRAPH_TYPE;

typedef struct {
    int dsp_code;
    const char *name;
    const char *mnemonic;
    unsigned char decimalplaces;
    int multiplier;
    int default_range;
    int addition;
    int multiply;
} EXTRA_READING_TYPE;

typedef struct {
    unsigned char type;
    unsigned char dsp_code;
    unsigned char graph_slot;
    unsigned char status_slot;
} EXTRA_READING;

class DlgExtraR: public wxDialog
{//============================
    public:
        DlgExtraR(wxWindow *parent);
        void ShowDlg(void);
    private:
        void OnButton(wxCommandEvent &event);

        wxButton *button_OK;
        wxButton *button_Cancel;
        wxChoice *t_type[N_EXTRA_READINGS];
        wxCheckBox *t_graph[N_EXTRA_READINGS];
        wxCheckBox *t_numeric[N_EXTRA_READINGS];

        wxArrayString ExtraRNames;
    DECLARE_EVENT_TABLE()
};


class DlgAlarms: public wxDialog
{//==============================
    public:
        DlgAlarms(wxWindow *parent);
        void Layout(int which_inverter);
        void Close(void);

        wxStaticText *inverter_name[2];
        wxStaticText *label[2][5];
        wxStaticText *message[2][5];

    private:
        void OnButton(wxCommandEvent &event);
    DECLARE_EVENT_TABLE()
};


class DlgCoords: public wxDialog
{//==============================
    public:
        DlgCoords(wxWindow *parent);
        void ShowDlg(CHART_PAGE *chart_page, int period);

        wxStaticText *t_time;
        wxStaticText *t_label[6];
        wxStaticText *t_value[6];
};


// inverter commands
#define cmdInverterInfo   20
#define cmdExtraDsp       21
#define cmdExtraDsp2      22
#define cmdGetInvTime     23
#define cmdSetInvTime     24
#define cmdSetInvTime2    25
#define cmdResetPartial   26
#define cmdInverter10SecEnergy 27
#define cmdInverterDailyEnergy 28
#define cmdAutoSetInvTime 29

// Automatic setting of the inverter's clock
#define CLOCK_TOLERANCE   5           // seconds, leave the clock alone if it's this close
#define CLOCK_MAX_CHANGE  (26*3600)   // seconds, a larger difference needs a person to look at it
#define CLOCK_SET_ATTEMPTS 3          // attempts to set the clock, before waiting
#ifndef CLOCK_RETRY_WAIT
#define CLOCK_RETRY_WAIT  3600        // seconds, the longest wait for another check after those attempts failed
#endif

extern wxString cmdoutput;
extern int command_inverter;
extern int command_type;

// Convert a measured value for storage in 16 bits.  Saturates rather than wraps.
// 0xffff is reserved to mean "no value"
inline unsigned short ToUShort(double value)
{
    if(!(value > 0))   // also catches NaN
        return(0);
    if(value >= 0xfffe)
        return(0xfffe);
    return((unsigned short)value);
}

inline short ToShort(double value)
{
    if(value != value)
        return(0);
    if(value <= -32768)
        return(-32768);
    if(value >= 32767)
        return(32767);
    return((short)value);
}

// The index of a 10 second period, or -1 if the time isn't valid.
// (seconds may be -1 in log files written by earlier versions, just after midnight)
inline int TimeToPeriod(int hours, int mins, int secs)
{
    if((hours < 0) || (hours > 23) || (mins < 0) || (mins > 59) || (secs < -1) || (secs > 59))
        return(-1);
    return(hours*360 + mins*6 + secs/10);
}

// Held by the inverter thread throughout each poll.  The GUI thread holds it
// while changing settings which that thread reads.  Not recursive.
extern wxMutex comms_mutex;

void PvoSendLive(const wxString &query, const wxString &fail_line);
void ResendPvoutput(int timeval);
void PvoShutdown();
void PvoTestStart(const wxString &url, const wxString &sid, const wxString &key, wxStaticText *result, wxButton *button);
void PvoTestCancel();
void PvoUpdateStatus();
void PvoResetStatus();
void MqttStart();
void MqttReadings(int inv, INVERTER_RESPONSE *ir, float temperature);
void MqttRestart();
void MqttShutdown();
void MqttUpdateStatus();
void MqttTestStart(const MQTT_SETTINGS &settings, wxStaticText *result, wxButton *button);
void MqttTestCancel();
void InvalidateDates();
void CommsShutdown();

// Dialog layout.  Controls have different sizes with different toolkits, themes and fonts,
// so dialogs use sizers rather than fixed positions.
#define DLG_BORDER  12
#define DLG_GAP      6

// a row of buttons, at the right
inline wxSizer *MakeButtonRow(wxButton *b1, wxButton *b2 = NULL, wxButton *b3 = NULL)
{
    wxBoxSizer *row = new wxBoxSizer(wxHORIZONTAL);
    row->AddStretchSpacer();
    row->Add(b1, 0, wxLEFT, DLG_GAP);
    if(b2 != NULL)
        row->Add(b2, 0, wxLEFT, DLG_GAP);
    if(b3 != NULL)
        row->Add(b3, 0, wxLEFT, DLG_GAP);
    return(row);
}

// a label in a grid, lined up with the control next to it
inline void AddGridLabel(wxWindow *parent, wxSizer *grid, const wxString &text)
{
    grid->Add(new wxStaticText(parent, -1, text), 0, wxALIGN_CENTER_VERTICAL);
}

int GetGmtOffset(time_t timeval);
bool MakeDate(wxDateTime &date, wxString ymd);
wxString MakeDateString(wxString ymd);
void SerialClose();
void ConfigSave(int control);
int CalcSun(wxString date_ymd);
void OpenLogFiles(int create, int control);
void LogColumnHeaders(int inverter, int control);
void strncpy0(char *to, const char* from, int size);
void DrawStatusPulse(int pulse);
void SetExtraReadings(wxString string);
void LogCommMsg(wxString string);
void LogMessage(wxString message, int control);
void PruneAlarmFile(int inv);
void UpdateDailyData(int inv, wxString fname_daily);

extern void GotInverterStatus(int inv);
extern void ShowInverterStatus();

extern int DisplayDateGraphs(const wxString date_str, int control);
extern wxString SelectDisplayDate();
extern wxString NextDate(const wxString date_str, int skip);
extern wxString PrevDate(const wxString date_str, int skip);
extern wxString YesterdayDate(const wxString date_str);
extern wxString DateAtCursor(int x);
extern int DateEnergy(const wxString date_str);
extern void GotInverterInfo(int inv, int ok);
extern int QueueCommand(int inv, int type);

extern Mainframe *mainframe;
extern wxPanel *status_panel;
extern wxCommandEvent my_event;

extern DlgChart *dlg_chart;
extern DlgSetup *dlg_setup;
extern DlgLocation *dlg_location;
extern DlgInverter *dlg_inverter;
extern DlgHistogram *dlg_histogram;
extern DlgExtraR *dlg_extrareadings;
extern DlgAlarms *dlg_alarms;
extern DlgRetrieveEnergy *dlg_retrieve_energy;
extern DlgPvoutput *dlg_pvoutput;
extern DlgMqtt *dlg_mqtt;
extern DlgSetTime *dlg_settime;
extern DlgCoords *dlg_coords;

extern CHART_PAGE chart_pages[N_PAGES];
extern double histogram_max;
extern int histogram_flags;
extern const double range_adjust[11];
extern int option_date_format;

extern wxString graph_date;     // with hyphens
extern wxString graph_date_ymd; // without hyphens
extern char date_ymd[12];
extern wxString date_ymd_string;
extern wxString date_logchange;
extern char ymd_energy[12];
extern char ymd_alarm[12];
extern int display_today;
extern int retrieving_data;
extern int retrieving_inv;
extern int retrieving_progress;
extern int retrieving_finished;
extern int retrieving_pvoutput;
extern wxString retrieving_fname;
extern wxString retrieve_message;

extern int settime_offset;
extern wxProgressDialog *progress_retrieval;
extern INVERTER_RESPONSE inverter_response[N_INV];

extern wxString data_dir;
extern wxString data_system_dir;
extern wxString data_year_dir;
extern wxString data_year_dir2;
extern wxString path_retrieve;
extern wxString serial_port;
extern int inverter_address[N_INV];

extern GraphPanel *graph_panel;
extern double Latitude;
extern double Longitude;
extern double TimeZone2;
extern double Timezone;
extern SUN sun;
unsigned extern short sun_azimuth[N_10SEC];
extern short sun_elevation[N_10SEC];
extern short sun_intensity[N_PANEL_GROUPS][N_10SEC];
extern PANEL_GROUP panel_groups[N_PANEL_GROUPS];

extern int n_extra_readings;
extern EXTRA_READING extra_readings[N_EXTRA_READINGS];
extern EXTRA_READING_TYPE extra_reading_types[];
extern GRAPH_TYPE graph_types[];
extern int n_graphtypes;
extern DATAREC *graph_data[N_INV];
extern int hist_scale[N_HIST_SCALE];

#endif // AURORAMONMAIN_H
