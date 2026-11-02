// Make an HTTPS request the way PVOutput uploads do, and report which library handled it.
// usage: webrequest_test <url>   Exit code 0 if the request completed, with any HTTP status.
#include <wx/app.h>
#include <wx/evtloop.h>
#include <wx/webrequest.h>
#include <cstdio>

class TestApp : public wxAppConsole
{
public:
    int OnRun() override
    {
        wxWebSession &session = wxWebSession::GetDefault();
        if(!session.IsOpened())
        {
            printf("No session\n");
            return 2;
        }
        printf("Library: %s\n", (const char *)session.GetLibraryVersionInfo().ToString().utf8_str());

        int result = 3;
        wxEventLoop loop;
        wxEvtHandler handler;
        handler.Bind(wxEVT_WEBREQUEST_STATE, [&](wxWebRequestEvent &event)
        {
            switch(event.GetState())
            {
            case wxWebRequest::State_Completed:
                printf("Completed, HTTP status %d\n", event.GetResponse().GetStatus());
                result = 0;
                loop.Exit();
                break;
            case wxWebRequest::State_Failed:
            case wxWebRequest::State_Unauthorized:
            case wxWebRequest::State_Cancelled:
                printf("Failed: %s\n", (const char *)event.GetErrorDescription().utf8_str());
                loop.Exit();
                break;
            default:
                break;
            }
        });
        wxWebRequest request = session.CreateRequest(&handler, argv[1]);
        request.Start();
        loop.Run();
        fflush(stdout);
        return result;
    }
};

wxIMPLEMENT_APP_CONSOLE(TestApp);
