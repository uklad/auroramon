
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

// Uploads to pvoutput.org.  Requests are asynchronous, so a slow or absent
// network never blocks the GUI thread (which also drives inverter polling).
// The API key is sent in a request header, never in the URL, so it can't reach
// the log files.

#include <wx/webrequest.h>
#include "auroramon.h"

#define MAX_PVOUTPUT_BATCH  30
#define PVO_TIMEOUT_MS      30000
#define PVO_MAX_AGE_DAYS    13


static wxString FailFileName()
{//===========================
    return(data_dir + _T("/system/pvoutput_fail.txt"));
}


// The outcome of the most recent upload, for the status bar
enum {PVO_NOT_SENT, PVO_SENT_OK, PVO_SEND_FAILED};
static int status_outcome = PVO_NOT_SENT;
static wxString status_time;


static int CountWaiting()
{//======================
    FILE *f;
    int count = 0;
    char buf[256];

    if((f = fopen(FailFileName().mb_str(wxConvLocal), "r")) == NULL)
        return(0);

    while(fgets(buf, sizeof(buf), f) != NULL)
    {
        if(buf[0] == '2')
            count++;
    }
    fclose(f);
    return(count);
}


static void SetOutcome(int outcome)
{//================================
    status_outcome = outcome;
    status_time = wxDateTime::Now().Format(_T("%H:%M"));
}


void PvoUpdateStatus()
{//===================
    wxString text;
    int waiting = CountWaiting();

    if(status_outcome == PVO_SENT_OK)
        text = _T("PVOutput: OK at ") + status_time;
    else
    if(status_outcome == PVO_SEND_FAILED)
        text = _T("PVOutput: failed at ") + status_time;
    else
    if((pvoutput.live_updates & PVO_PERIOD) == 0)
        text = _T("PVOutput: off");
    else
        text = _T("PVOutput: no update yet");

    if(waiting > 0)
        text += wxString::Format(_T(", %d waiting"), waiting);

    mainframe->SetStatusText(text, 1);
}


void PvoResetStatus()
{//==================
    // the settings have changed, so the last outcome no longer applies
    status_outcome = PVO_NOT_SENT;
    PvoUpdateStatus();
}


class PvoUploader : public wxEvtHandler
{//====================================
    public:
        PvoUploader();
        ~PvoUploader();
        void SendLive(const wxString &query, const wxString &fail_line);
        void SendPending();

    private:
        bool Start(const wxString &url);
        void Finish(bool ok, const wxString &error);
        void OnRequestState(wxWebRequestEvent &event);
        void OnTimeout(wxTimerEvent &event);
        void SaveForRetry(const wxString &line);
        void RemoveSentLines(int n_lines);

        wxWebRequest request;
        wxTimer timer;
        bool in_flight;
        wxString live_fail_line;  // saved for a later retry if this live update fails
        int batch_lines;          // lines of the fail file covered by this batch request
        wxString pending_error;   // reason for a request which we had to cancel
};


PvoUploader::PvoUploader()
{//=======================
    in_flight = false;
    batch_lines = 0;
    timer.SetOwner(this);
    Bind(wxEVT_WEBREQUEST_STATE, &PvoUploader::OnRequestState, this);
    Bind(wxEVT_TIMER, &PvoUploader::OnTimeout, this);
}


PvoUploader::~PvoUploader()
{//========================
    timer.Stop();
    Unbind(wxEVT_WEBREQUEST_STATE, &PvoUploader::OnRequestState, this);
    if(in_flight)
    {
        request.Cancel();
        if(!live_fail_line.empty())
            SaveForRetry(live_fail_line);  // keep the data, we won't see the outcome
    }
}


bool PvoUploader::Start(const wxString &url)
{//=========================================
    request = wxWebSession::GetDefault().CreateRequest(this, url);
    if(!request.IsOk())
    {
        Finish(false, _T("Can't create request for ") + pvoutput.url);
        return(false);
    }

    request.SetHeader(_T("X-Pvoutput-Apikey"), pvoutput.key);
    request.SetHeader(_T("X-Pvoutput-SystemId"), pvoutput.sid);
    in_flight = true;
    pending_error.clear();
    timer.StartOnce(PVO_TIMEOUT_MS);
    request.Start();
    return(true);
}


void PvoUploader::SaveForRetry(const wxString &line)
{//=================================================
    FILE *f;

    if((f = fopen(FailFileName().mb_str(wxConvLocal), "a")) != NULL)
    {
        fprintf(f, "%s\n", (const char *)line.mb_str(wxConvLocal));
        fclose(f);
    }
}


void PvoUploader::RemoveSentLines(int n_lines)
{//===========================================
    // Lines may have been appended while the request was in progress, so
    // only remove the ones which this batch covered.
    FILE *f_in;
    FILE *f_out = NULL;
    wxString fname = FailFileName();
    wxString fname2 = fname + _T("2");
    char buf[256];

    if((f_in = fopen(fname.mb_str(wxConvLocal), "r")) == NULL)
        return;

    while(fgets(buf, sizeof(buf), f_in) != NULL)
    {
        if(n_lines > 0)
        {
            n_lines--;
            continue;
        }
        if(buf[0] != '2')
            continue;

        if((f_out == NULL) && ((f_out = fopen(fname2.mb_str(wxConvLocal), "w")) == NULL))
        {
            // can't write the remainder, keep the whole file and resend later
            fclose(f_in);
            return;
        }
        fputs(buf, f_out);
    }
    fclose(f_in);

    if(f_out != NULL)
    {
        fclose(f_out);
        wxRenameFile(fname2, fname, true);
    }
    else
    {
        wxRemoveFile(fname);
    }
}


void PvoUploader::Finish(bool ok, const wxString &error)
{//=====================================================
    int n_lines = batch_lines;
    wxString fail_line = live_fail_line;

    timer.Stop();
    in_flight = false;
    batch_lines = 0;
    live_fail_line.clear();

    if(ok)
    {
        if(n_lines > 0)
        {
            RemoveSentLines(n_lines);
            LogMessage(_T("PVOutput.org bulk update OK"), 1);
        }
        pvoutput.failed = 0;
        SetOutcome(PVO_SENT_OK);
        PvoUpdateStatus();
        return;
    }

    // only write the first of a series of failures to the log file
    LogMessage(_T("PVOutput.org update failed: ") + error, (pvoutput.failed == 0) ? 1 : 0);
    pvoutput.failed = 1;

    if(!fail_line.empty())
        SaveForRetry(fail_line);

    SetOutcome(PVO_SEND_FAILED);
    PvoUpdateStatus();
}


void PvoUploader::OnRequestState(wxWebRequestEvent &event)
{//=======================================================
    switch(event.GetState())
    {
    case wxWebRequest::State_Completed:
        Finish(true, wxEmptyString);
        break;

    case wxWebRequest::State_Failed:
    {
        wxString error = event.GetErrorDescription();
        wxWebResponse response = event.GetResponse();
        if(response.IsOk() && (response.GetStatus() >= 400))
            error = wxString::Format(_T("HTTP status %d"), response.GetStatus());
        Finish(false, error);
        break;
    }

    case wxWebRequest::State_Unauthorized:
        // this isn't a final state, the request would wait for credentials
        pending_error = _T("not authorized, check the API key and system id");
        request.Cancel();
        break;

    case wxWebRequest::State_Cancelled:
        Finish(false, pending_error.empty() ? wxString(_T("cancelled")) : pending_error);
        break;

    default:
        break;
    }
}


void PvoUploader::OnTimeout(wxTimerEvent& WXUNUSED(event))
{//=======================================================
    if(in_flight)
    {
        pending_error = _T("timed out");
        request.Cancel();
    }
}


void PvoUploader::SendLive(const wxString &query, const wxString &fail_line)
{//=========================================================================
    if(in_flight)
    {
        // the previous request hasn't finished, send this one with the next batch
        SaveForRetry(fail_line);
        PvoUpdateStatus();
        return;
    }
    live_fail_line = fail_line;
    batch_lines = 0;
    Start(pvoutput.url + query);
}


void PvoUploader::SendPending()
{//============================
    FILE *f_in;
    wxString data;
    wxString line;
    wxDateTime date;
    wxDateTime today;
    int count = 0;
    int lines_read = 0;
    int age;
    char buf[256];

    if(in_flight)
        return;

    if((f_in = fopen(FailFileName().mb_str(wxConvLocal), "r")) == NULL)
        return;

    today = wxDateTime::Now();

    while((count < MAX_PVOUTPUT_BATCH) && (fgets(buf, sizeof(buf), f_in) != NULL))
    {
        lines_read++;
        if(buf[0] != '2')
            continue;

        line = wxString(buf, wxConvLocal);
        if(!MakeDate(date, line.Left(8)))
            continue;   // not a line which we wrote

        age = today.Subtract(date).GetDays();
        if((age < 0) || (age > PVO_MAX_AGE_DAYS))
            continue;   // discard old data

        line.Trim();
        line.Replace(_T(" "), wxEmptyString);   // earlier versions padded the numbers

        if(count > 0)
            data += _T(";");
        data += line;
        count++;
    }
    fclose(f_in);

    if(count == 0)
    {
        // nothing is worth sending, only obsolete lines
        RemoveSentLines(lines_read);
        PvoUpdateStatus();
        return;
    }

    live_fail_line.clear();
    batch_lines = lines_read;
    Start(pvoutput.url + _T("addbatchstatus.jsp?data=") + data);
}


// Tests the settings in the PVOutput dialog.  The specification has no test mode,
// so this uses Get System, which needs a valid System Id and API Key and changes nothing.
class PvoTester : public wxEvtHandler
{//==================================
    public:
        PvoTester();
        ~PvoTester();
        void Start(const wxString &url, const wxString &sid, const wxString &key, wxStaticText *result_text, wxButton *test_button);
        void Cancel();

    private:
        void Done(const wxString &text);
        void OnRequestState(wxWebRequestEvent &event);
        void OnTimeout(wxTimerEvent &event);

        wxWebRequest request;
        wxTimer timer;
        wxStaticText *result;
        wxButton *button;
        bool active;
        wxString pending_error;
};


#define PVO_TEST_NOT_AUTHORIZED  _T("Not authorized.  Check the System Id and API Key, and that API access is enabled in your pvoutput.org settings.")


static wxString DescribeSystem(wxWebResponse &response)
{//====================================================
    // The fields before the first ';' are the system's details, and the last 15 of those
    // follow the name, which could itself contain a comma.
    wxString line = response.AsString().BeforeFirst('\n').BeforeFirst(';');
    wxArrayString fields = wxSplit(line.Trim(), ',', '\0');
    wxString name;
    wxString text;
    int n_name = (int)fields.GetCount() - 15;
    int ix;

    if((n_name < 1) || line.Contains(_T("<")))
        return(_T("The server answered, but not as PVOutput would.  Check the URL."));

    for(ix = 0; ix < n_name; ix++)
    {
        if(ix > 0)
            name += _T(",");
        name += fields[ix];
    }

    text = _T("Connected to \"") + name + _T("\".");

    wxString remaining = response.GetHeader(_T("X-Rate-Limit-Remaining"));
    wxString limit = response.GetHeader(_T("X-Rate-Limit-Limit"));
    if(!remaining.empty() && !limit.empty())
        text += _T("  ") + remaining + _T(" of ") + limit + _T(" requests left this hour.");
    return(text);
}


static wxString DescribeRefusal(wxWebResponse &response)
{//=====================================================
    wxString text;
    wxString body;

    if(response.GetStatus() == 401)
        return(PVO_TEST_NOT_AUTHORIZED);

    text = wxString::Format(_T("Refused, HTTP status %d"), response.GetStatus());

    // PVOutput gives the reason as a line of text, such as having reached the hourly limit
    body = response.AsString().BeforeFirst('\n').Trim().Left(150);
    if(!body.empty() && !body.Contains(_T("<")))
        text += _T(": ") + body;
    return(text);
}


PvoTester::PvoTester()
{//===================
    result = NULL;
    button = NULL;
    active = false;
    timer.SetOwner(this);
    Bind(wxEVT_WEBREQUEST_STATE, &PvoTester::OnRequestState, this);
    Bind(wxEVT_TIMER, &PvoTester::OnTimeout, this);
}


PvoTester::~PvoTester()
{//====================
    Unbind(wxEVT_WEBREQUEST_STATE, &PvoTester::OnRequestState, this);
    Cancel();
}


void PvoTester::Start(const wxString &url, const wxString &sid, const wxString &key, wxStaticText *result_text, wxButton *test_button)
{//=================================================================================================================================
    wxString base = url;

    if(active)
        return;

    result = result_text;
    button = test_button;

    if(url.empty() || sid.empty() || key.empty())
    {
        Done(_T("Enter the URL, System Id and API Key first."));
        return;
    }

    if(base.Right(1) != _T("/"))
        base += _T("/");

    request = wxWebSession::GetDefault().CreateRequest(this, base + _T("getsystem.jsp"));
    if(!request.IsOk())
    {
        Done(_T("Can't create a request for this URL."));
        return;
    }

    request.SetHeader(_T("X-Pvoutput-Apikey"), key);
    request.SetHeader(_T("X-Pvoutput-SystemId"), sid);
    request.SetHeader(_T("X-Rate-Limit"), _T("1"));

    active = true;
    pending_error.clear();
    button->Disable();
    result->SetLabelText(_T("Testing..."));
    timer.StartOnce(PVO_TIMEOUT_MS);
    request.Start();
}


void PvoTester::Cancel()
{//=====================
    if(active)
    {
        active = false;
        timer.Stop();
        request.Cancel();
    }
}


void PvoTester::Done(const wxString &text)
{//=======================================
    active = false;
    timer.Stop();
    result->SetLabelText(text);
    result->Wrap(result->GetSize().x);
    button->Enable();
}


void PvoTester::OnRequestState(wxWebRequestEvent &event)
{//=====================================================
    // ignore a request which was cancelled when the dialog closed
    if(!active || (event.GetRequest().GetId() != request.GetId()))
        return;

    switch(event.GetState())
    {
    case wxWebRequest::State_Completed:
    {
        wxWebResponse response = event.GetResponse();
        Done(DescribeSystem(response));
        break;
    }

    case wxWebRequest::State_Failed:
    {
        wxWebResponse response = event.GetResponse();
        if(response.IsOk() && (response.GetStatus() >= 400))
            Done(DescribeRefusal(response));
        else
            Done(_T("Can't connect: ") + event.GetErrorDescription());
        break;
    }

    case wxWebRequest::State_Unauthorized:
        // this isn't a final state, the request would wait for credentials
        pending_error = PVO_TEST_NOT_AUTHORIZED;
        request.Cancel();
        break;

    case wxWebRequest::State_Cancelled:
        Done(pending_error.empty() ? wxString(_T("Cancelled.")) : pending_error);
        break;

    default:
        break;
    }
}


void PvoTester::OnTimeout(wxTimerEvent& WXUNUSED(event))
{//=====================================================
    if(active)
    {
        pending_error = wxString::Format(_T("No answer after %d seconds."), PVO_TIMEOUT_MS/1000);
        request.Cancel();
    }
}


static PvoTester *tester = NULL;

void PvoTestStart(const wxString &url, const wxString &sid, const wxString &key, wxStaticText *result, wxButton *button)
{//=====================================================================================================================
    if(tester == NULL)
        tester = new PvoTester();
    tester->Start(url, sid, key, result, button);
}


void PvoTestCancel()
{//=================
    if(tester != NULL)
        tester->Cancel();
}


static PvoUploader *uploader = NULL;

static PvoUploader *GetUploader()
{//==============================
    if(uploader == NULL)
        uploader = new PvoUploader();
    return(uploader);
}


void PvoSendLive(const wxString &query, const wxString &fail_line)
{//===============================================================
    GetUploader()->SendLive(query, fail_line);
}


void ResendPvoutput(int timeval)
{//=============================
    // 'timeval' only needs to change whenever a resend attempt is due
    static int prev_time = -1;

    if(prev_time < 0)
    {
        // ignore the first call after program start
        prev_time = timeval;
    }

    if(timeval == prev_time)
        return;
    prev_time = timeval;

    GetUploader()->SendPending();
}


void PvoShutdown()
{//===============
    delete uploader;
    uploader = NULL;
    delete tester;
    tester = NULL;
}
