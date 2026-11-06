
/***************************************************************************
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

// Publishes the readings to an MQTT broker, with Home Assistant's MQTT discovery
// so that each inverter appears there as a device with its sensors.
//
// This is a minimal MQTT 3.1.1 client.  It only connects, publishes at QoS 0 and
// keeps the connection alive.  It has no TLS, so it's intended for a broker on the
// local network.  It runs on the GUI thread (which also drives inverter polling)
// with non-blocking sockets.  Only the host name lookup, which can block, runs on
// another thread.
//
// Topics, where <prefix> is "auroramon" unless changed in the settings:
//   <prefix>/status                    online / offline (retained, offline is also the will)
//   <prefix>/inverter<N>/state         JSON readings of the inverter at address N (retained)
//   <prefix>/inverter<N>/availability  online / offline, whether the inverter is responding
//   <prefix>/total/state               total of both inverters, only when there are two
//   <discovery>/sensor/<node>_inverter<N>/<key>/config   Home Assistant discovery

#include <wx/socket.h>
#include <atomic>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <thread>
#include "auroramon.h"

#define MQTT_KEEPALIVE         60      // seconds, sent to the broker in CONNECT
#define MQTT_CONNECT_TIMEOUT   10      // seconds, to look up the host, connect and get CONNACK
#define MQTT_RETRY_MIN          5      // seconds between attempts to reconnect, doubling to
#define MQTT_RETRY_MAX        120
#define MQTT_MAX_OUTPUT  (256*1024)    // bytes waiting to be sent, before giving up on the broker

MQTT_SETTINGS mqtt;


//=======================================================================================
//  Sensors
//=======================================================================================

typedef struct {
    int dsp_code;             // for extra readings, otherwise 0
    const char *key;          // in the state JSON, and part of the unique id
    const char *name;
    const char *unit;
    const char *device_class;
    const char *state_class;
    int precision;
} MQTT_SENSOR;

#define DEG_C   "\xC2\xB0" "C"
#define MOHM    "M\xCE\xA9"

static const MQTT_SENSOR inverter_sensors[] = {
    {0, "power",          "Power",            "W",   "power",       "measurement", 0},
    {0, "input1_power",   "Input 1 power",    "W",   "power",       "measurement", 0},
    {0, "input1_voltage", "Input 1 voltage",  "V",   "voltage",     "measurement", 1},
    {0, "input1_current", "Input 1 current",  "A",   "current",     "measurement", 2},
    {0, "input2_power",   "Input 2 power",    "W",   "power",       "measurement", 0},
    {0, "input2_voltage", "Input 2 voltage",  "V",   "voltage",     "measurement", 1},
    {0, "input2_current", "Input 2 current",  "A",   "current",     "measurement", 2},
    {0, "efficiency",     "Efficiency",       "%",   NULL,          "measurement", 1},
    {0, "temperature",    "Temperature",      DEG_C, "temperature", "measurement", 1},
    {0, "energy_today",   "Energy today",     "kWh", "energy",      "total_increasing", 3},
    {0, "energy_week",    "Energy this week", "kWh", "energy",      "total_increasing", 3},
    {0, "energy_month",   "Energy this month","kWh", "energy",      "total_increasing", 3},
    {0, "energy_year",    "Energy this year", "kWh", "energy",      "total_increasing", 3},
    {0, "energy_total",   "Lifetime energy",  "kWh", "energy",      "total_increasing", 3},
    {0, "energy_partial", "Partial energy",   "kWh", "energy",      "total_increasing", 3},
    {0, "peak_today",     "Peak power today", "W",   "power",       NULL,          0},
    {0, NULL, NULL, NULL, NULL, NULL, 0}
};

// The extra readings (Settings -> Extra Readings).  Energy Today (64) and Peak Today (35)
// are left out because they're already among the sensors above.
static const MQTT_SENSOR extra_sensors[] = {
    { 1, "grid_voltage",         "Grid voltage",           "V",   "voltage",     "measurement", 1},
    { 2, "grid_current",         "Grid current",           "A",   "current",     "measurement", 2},
    { 4, "grid_frequency",       "Grid frequency",         "Hz",  "frequency",   "measurement", 2},
    { 8, "pin1_power",           "Pin 1 power",            "W",   "power",       "measurement", 0},
    { 9, "pin2_power",           "Pin 2 power",            "W",   "power",       "measurement", 0},
    {34, "peak_power",           "Peak power",             "W",   "power",       NULL,          0},
    { 6, "leakage_dcdc",         "Leakage current DC/DC",  "mA",  "current",     "measurement", 1},
    { 7, "leakage_inverter",     "Leakage current inverter","mA", "current",     "measurement", 1},
    {30, "isolation_resistance", "Isolation resistance",   MOHM,  NULL,          "measurement", 1},
    { 5, "bulk_voltage",         "Bulk voltage",           "V",   "voltage",     "measurement", 1},
    {33, "bulk_voltage_mid",     "Bulk voltage mid",       "V",   "voltage",     "measurement", 1},
    {31, "bulk_voltage_dcdc",    "Bulk voltage DC/DC",     "V",   "voltage",     "measurement", 1},
    {45, "bulk_voltage_plus",    "Bulk voltage +",         "V",   "voltage",     "measurement", 1},
    {46, "bulk_voltage_minus",   "Bulk voltage -",         "V",   "voltage",     "measurement", 1},
    {28, "grid_voltage_dcdc",    "Grid voltage DC/DC",     "V",   "voltage",     "measurement", 1},
    {32, "grid_voltage_average", "Grid voltage average",   "V",   "voltage",     "measurement", 1},
    {36, "grid_voltage_neutral", "Grid voltage neutral",   "V",   "voltage",     "measurement", 1},
    {29, "grid_frequency_dcdc",  "Grid frequency DC/DC",   "Hz",  "frequency",   "measurement", 2},
    {21, "temperature_booster",  "Booster temperature",    DEG_C, "temperature", "measurement", 1},
    {22, "temperature_inverter", "Inverter temperature",   DEG_C, "temperature", "measurement", 1},
    {47, "temperature_supervisor","Supervisor temperature",DEG_C, "temperature", "measurement", 1},
    {48, "temperature_alim",     "Alim temperature",       DEG_C, "temperature", "measurement", 1},
    {49, "temperature_heatsink", "Heatsink temperature",   DEG_C, "temperature", "measurement", 1},
    {53, "fan1_speed",           "Fan 1 speed",            NULL,  NULL,          "measurement", 0},
    {54, "fan2_speed",           "Fan 2 speed",            NULL,  NULL,          "measurement", 0},
    {55, "fan3_speed",           "Fan 3 speed",            NULL,  NULL,          "measurement", 0},
    {37, "wind_frequency",       "Wind generator frequency","Hz", "frequency",   "measurement", 1},
    {0, NULL, NULL, NULL, NULL, NULL, 0}
};

static const MQTT_SENSOR total_sensors[] = {
    {0, "power",        "Total power",        "W",   "power",  "measurement", 0},
    {0, "energy_today", "Total energy today", "kWh", "energy", "total_increasing", 3},
    {0, NULL, NULL, NULL, NULL, NULL, 0}
};


static const MQTT_SENSOR *FindSensor(const MQTT_SENSOR *table, const char *key)
{//===========================================================================
    for(; table->key != NULL; table++)
    {
        if(strcmp(table->key, key) == 0)
            return(table);
    }
    return(NULL);
}


static std::string Utf8(const wxString &s)
{//======================================
    wxScopedCharBuffer buf = s.ToUTF8();
    return(std::string(buf.data(), buf.length()));
}


static std::string JsonString(const std::string &s)
{//================================================
    std::string out = "\"";
    char hex[8];

    for(size_t ix = 0; ix < s.size(); ix++)
    {
        unsigned char c = s[ix];
        if((c == '"') || (c == '\\'))
        {
            out += '\\';
            out += c;
        }
        else
        if(c < 0x20)
        {
            snprintf(hex, sizeof(hex), "\\u%.4x", c);
            out += hex;
        }
        else
            out += c;
    }
    return(out + "\"");
}


static std::string JsonNumber(double value, int precision)
{//======================================================
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f", precision, value);
    for(char *p = buf; *p != 0; p++)
    {
        if(*p == ',')
            *p = '.';   // in case the C locale isn't "C"
    }
    if(strcmp(buf, "-0") == 0)
        return("0");
    return(buf);
}


// The topic prefix, without '/' at either end and without wildcards
static std::string TopicPrefix(const wxString &setting, const char *default_value)
{//===============================================================================
    std::string s = Utf8(setting);
    std::string out;

    while(!s.empty() && ((s[0] == '/') || (s[0] == ' ')))
        s.erase(0, 1);
    while(!s.empty() && ((s[s.size()-1] == '/') || (s[s.size()-1] == ' ')))
        s.erase(s.size()-1);

    for(size_t ix = 0; ix < s.size(); ix++)
        out += ((s[ix] == '+') || (s[ix] == '#')) ? '_' : s[ix];

    if(out.empty())
        out = default_value;
    return(out);
}


// For unique ids and discovery topics, which only allow [a-zA-Z0-9_-]
static std::string NodeId(const std::string &prefix)
{//=================================================
    std::string out;

    for(size_t ix = 0; ix < prefix.size(); ix++)
    {
        char c = prefix[ix];
        out += (isalnum((unsigned char)c) || (c == '-')) ? c : '_';
    }
    return(out);
}


//=======================================================================================
//  MqttConnection: a connection to the broker
//=======================================================================================

class MqttConnection : public wxEvtHandler
{//======================================
    public:
        MqttConnection();
        ~MqttConnection();
        void Start(const MQTT_SETTINGS &settings, const std::string &client_id, const std::string &will_topic);
        void Close(bool graceful);
        bool Publish(const std::string &topic, const std::string &payload, bool retain);
        bool IsConnected() { return(state == ST_CONNECTED); }
        bool IsIdle() { return(state == ST_IDLE); }
        wxString Where() { return(where); }

        // called with CallAfter, so they may close or restart the connection
        std::function<void()> on_connected;
        std::function<void(const wxString &reason)> on_failed;

    private:
        enum {ST_IDLE, ST_RESOLVING, ST_CONNECTING, ST_WAIT_CONNACK, ST_CONNECTED};

        void Resolved(const std::string &ip);
        void SendConnect();
        void Fail(const wxString &reason);
        void Flush();
        void ReadInput();
        void HandlePacket(unsigned char type, const std::string &body);
        void Queue(unsigned char type, const std::string &body);
        void DropSocket();
        void OnSocket(wxSocketEvent &event);
        void OnTimer(wxTimerEvent &event);

        int id;                  // to find this connection again after the host name lookup
        int state;
        wxSocketClient *sock;
        wxTimer timer;
        time_t started;
        time_t last_tx;
        time_t last_rx;
        std::string outbuf;
        std::string inbuf;
        MQTT_SETTINGS settings;
        std::string client_id;
        std::string will_topic;
        wxString where;          // host:port, for messages
};

static std::map<int, MqttConnection *> live_connections;   // used on the GUI thread only
static int next_connection_id = 0;
static std::atomic<bool> shutting_down(false);


static void PutLength(std::string &s, size_t n)
{//===========================================
    do {
        unsigned char b = n % 128;
        n /= 128;
        if(n > 0)
            b |= 0x80;
        s += (char)b;
    } while(n > 0);
}


static void PutString(std::string &s, const std::string &value)
{//===========================================================
    s += (char)((value.size() >> 8) & 0xff);
    s += (char)(value.size() & 0xff);
    s += value;
}


MqttConnection::MqttConnection()
{//=============================
    id = ++next_connection_id;
    live_connections[id] = this;
    state = ST_IDLE;
    sock = NULL;
    started = last_tx = last_rx = 0;
    timer.SetOwner(this);
    Bind(wxEVT_TIMER, &MqttConnection::OnTimer, this);
    Bind(wxEVT_SOCKET, &MqttConnection::OnSocket, this);
}


MqttConnection::~MqttConnection()
{//==============================
    live_connections.erase(id);
    timer.Stop();
    DropSocket();
}


void MqttConnection::Start(const MQTT_SETTINGS &new_settings, const std::string &new_client_id, const std::string &new_will_topic)
{//=============================================================================================================================
    std::string host;
    int my_id;

    Close(false);
    settings = new_settings;
    client_id = new_client_id;
    will_topic = new_will_topic;
    where = settings.host + wxString::Format(_T(":%d"), settings.port);
    outbuf.clear();
    inbuf.clear();

    // a new id, so that the result of an earlier lookup is ignored
    live_connections.erase(id);
    id = ++next_connection_id;
    live_connections[id] = this;
    my_id = id;

    state = ST_RESOLVING;
    started = time(NULL);
    timer.Start(1000);

    if(!wxSocketBase::IsInitialized())
        wxSocketBase::Initialize();

    // The lookup can take several seconds if the name isn't known, so it mustn't block
    // the GUI thread.  Only plain strings cross between the threads.
    host = Utf8(settings.host);
    std::thread([my_id, host]() {
        wxIPV4address addr;
        std::string ip;
        if(addr.Hostname(wxString::FromUTF8(host.c_str())))
            ip = Utf8(addr.IPAddress());
        if(shutting_down)
            return;
        wxTheApp->CallAfter([my_id, ip]() {
            std::map<int, MqttConnection *>::iterator it = live_connections.find(my_id);
            if(it != live_connections.end())
                it->second->Resolved(ip);
        });
    }).detach();
}


void MqttConnection::Resolved(const std::string &ip)
{//=================================================
    wxIPV4address addr;

    if(state != ST_RESOLVING)
        return;

    if(ip.empty() || !addr.Hostname(wxString::FromUTF8(ip.c_str())))
    {
        Fail(_T("can't find ") + settings.host);
        return;
    }
    addr.Service(settings.port);

    sock = new wxSocketClient(wxSOCKET_NOWAIT);
    sock->SetEventHandler(*this);
    sock->SetNotify(wxSOCKET_CONNECTION_FLAG | wxSOCKET_INPUT_FLAG | wxSOCKET_OUTPUT_FLAG | wxSOCKET_LOST_FLAG);
    sock->Notify(true);
    state = ST_CONNECTING;
    if(sock->Connect(addr, false))
        SendConnect();
    // otherwise wait for wxSOCKET_CONNECTION, or wxSOCKET_LOST if it fails
}


void MqttConnection::SendConnect()
{//===============================
    std::string body;
    std::string username = Utf8(settings.username);
    std::string password = Utf8(settings.password);
    unsigned char flags = 0x02;   // clean session

    if(!will_topic.empty())
        flags |= 0x04 | 0x20;     // a retained will, at QoS 0
    if(!username.empty())
    {
        flags |= 0x80;
        if(!password.empty())
            flags |= 0x40;        // a password isn't allowed without a user name
    }

    PutString(body, "MQTT");
    body += (char)4;              // protocol level, 3.1.1
    body += (char)flags;
    body += (char)(MQTT_KEEPALIVE >> 8);
    body += (char)(MQTT_KEEPALIVE & 0xff);
    PutString(body, client_id);
    if(!will_topic.empty())
    {
        PutString(body, will_topic);
        PutString(body, "offline");
    }
    if(flags & 0x80)
        PutString(body, username);
    if(flags & 0x40)
        PutString(body, password);

    state = ST_WAIT_CONNACK;
    last_rx = time(NULL);
    Queue(0x10, body);
}


void MqttConnection::Queue(unsigned char type, const std::string &body)
{//====================================================================
    outbuf += (char)type;
    PutLength(outbuf, body.size());
    outbuf += body;
    last_tx = time(NULL);
    Flush();
}


bool MqttConnection::Publish(const std::string &topic, const std::string &payload, bool retain)
{//============================================================================================
    std::string body;

    if(state != ST_CONNECTED)
        return(false);

    if(outbuf.size() > MQTT_MAX_OUTPUT)
    {
        Fail(_T("the broker isn't accepting data"));
        return(false);
    }

    PutString(body, topic);
    body += payload;     // QoS 0, so there's no packet id
    Queue(retain ? 0x31 : 0x30, body);
    return(state == ST_CONNECTED);
}


void MqttConnection::Flush()
{//=========================
    if((sock == NULL) || outbuf.empty() || (state < ST_WAIT_CONNACK))
        return;

    sock->Write(outbuf.data(), outbuf.size());
    if(sock->LastWriteCount() > 0)
        outbuf.erase(0, sock->LastWriteCount());
    else
    if(sock->Error() && (sock->LastError() != wxSOCKET_WOULDBLOCK))
        Fail(_T("can't send to ") + where);
}


void MqttConnection::ReadInput()
{//=============================
    char buf[1024];
    size_t pos;
    size_t length;
    int shift;

    while(sock != NULL)
    {
        sock->Read(buf, sizeof(buf));
        if(sock->LastReadCount() == 0)
            break;
        inbuf.append(buf, sock->LastReadCount());
        last_rx = time(NULL);
    }

    // complete packets: a type byte, the remaining length (1 to 4 bytes) and the body
    while((sock != NULL) && (inbuf.size() >= 2))
    {
        length = 0;
        shift = 0;
        for(pos = 1; ; pos++)
        {
            if(pos >= inbuf.size())
                return;      // wait for the rest of the length
            if(pos > 4)
            {
                Fail(_T("bad data from the broker"));
                return;
            }
            length |= (size_t)(inbuf[pos] & 0x7f) << shift;
            shift += 7;
            if((inbuf[pos] & 0x80) == 0)
                break;
        }
        pos++;
        if(inbuf.size() < pos + length)
            return;          // wait for the rest of the packet

        std::string body = inbuf.substr(pos, length);
        unsigned char type = inbuf[0];
        inbuf.erase(0, pos + length);
        HandlePacket(type, body);
    }
}


void MqttConnection::HandlePacket(unsigned char type, const std::string &body)
{//===========================================================================
    static const wxChar *refusals[] = {
        NULL,
        _T("the broker doesn't support MQTT 3.1.1"),
        _T("the broker rejected the client id"),
        _T("the broker is unavailable"),
        _T("bad user name or password"),
        _T("not authorized"),
    };

    if((type & 0xf0) == 0x20)
    {
        // CONNACK
        if(state != ST_WAIT_CONNACK)
            return;
        int code = (body.size() >= 2) ? (unsigned char)body[1] : 255;
        if(code == 0)
        {
            state = ST_CONNECTED;
            int my_id = id;
            wxTheApp->CallAfter([my_id]() {
                std::map<int, MqttConnection *>::iterator it = live_connections.find(my_id);
                if((it != live_connections.end()) && it->second->IsConnected() && it->second->on_connected)
                    it->second->on_connected();
            });
        }
        else
        if(code <= 5)
            Fail(wxString(_T("refused, ")) + refusals[code]);
        else
            Fail(wxString::Format(_T("refused, code %d"), code));
    }
    // PINGRESP, and anything else, only shows that the broker is still there
}


void MqttConnection::DropSocket()
{//==============================
    if(sock != NULL)
    {
        sock->Notify(false);
        sock->Destroy();   // deleted later, any events for it are ignored
        sock = NULL;
    }
}


void MqttConnection::Fail(const wxString &reason)
{//==============================================
    int my_id = id;

    if(state == ST_IDLE)
        return;

    DropSocket();
    timer.Stop();
    state = ST_IDLE;
    outbuf.clear();
    inbuf.clear();

    wxTheApp->CallAfter([my_id, reason]() {
        std::map<int, MqttConnection *>::iterator it = live_connections.find(my_id);
        if((it != live_connections.end()) && it->second->on_failed)
            it->second->on_failed(reason);
    });
}


void MqttConnection::Close(bool graceful)
{//======================================
    // graceful: tell the broker, so that it doesn't publish the will
    if((state == ST_CONNECTED) && graceful && (sock != NULL))
    {
        Queue(0xe0, std::string());   // DISCONNECT
        if(!outbuf.empty())
        {
            // at shutdown there's no later chance to send it
            sock->SetFlags(wxSOCKET_WAITALL | wxSOCKET_BLOCK);
            sock->SetTimeout(2);
            sock->Write(outbuf.data(), outbuf.size());
        }
    }
    DropSocket();
    timer.Stop();
    state = ST_IDLE;
    outbuf.clear();
    inbuf.clear();
}


void MqttConnection::OnSocket(wxSocketEvent &event)
{//================================================
    if((sock == NULL) || (event.GetSocket() != sock))
        return;   // a socket we've finished with

    switch(event.GetSocketEvent())
    {
    case wxSOCKET_CONNECTION:
        if(state == ST_CONNECTING)
            SendConnect();
        break;

    case wxSOCKET_INPUT:
        ReadInput();
        break;

    case wxSOCKET_OUTPUT:
        Flush();
        break;

    case wxSOCKET_LOST:
        if(state == ST_CONNECTING)
            Fail(_T("can't connect to ") + where);
        else
        if(state == ST_WAIT_CONNACK)
            Fail(_T("the broker closed the connection"));
        else
            Fail(_T("lost the connection to ") + where);
        break;
    }
}


void MqttConnection::OnTimer(wxTimerEvent& WXUNUSED(event))
{//========================================================
    time_t now = time(NULL);

    if(state == ST_CONNECTED)
    {
        if(now - last_rx > MQTT_KEEPALIVE * 3 / 2)
        {
            Fail(_T("no answer from the broker"));
            return;
        }
        if(now - last_tx >= MQTT_KEEPALIVE / 2)
            Queue(0xc0, std::string());   // PINGREQ, the broker answers with PINGRESP
        Flush();
        return;
    }

    if((state != ST_IDLE) && (now - started > MQTT_CONNECT_TIMEOUT))
    {
        if(state == ST_RESOLVING)
            Fail(_T("can't find ") + settings.host);
        else
            Fail(wxString::Format(_T("no answer from %s after %d seconds"), where, MQTT_CONNECT_TIMEOUT));
    }
}


static std::string RandomClientId(const char *start)
{//================================================
    static bool seeded = false;
    char buf[48];

    if(!seeded)
    {
        srand((unsigned)time(NULL) ^ (unsigned)wxGetProcessId());
        seeded = true;
    }
    snprintf(buf, sizeof(buf), "%s-%.4x%.4x", start, rand() & 0xffff, rand() & 0xffff);
    return(buf);
}


//=======================================================================================
//  MqttPublisher: keeps the connection open and publishes the readings
//=======================================================================================

typedef struct {
    double value;
    const MQTT_SENSOR *sensor;
} MQTT_READING;

typedef struct {
    int address;                                // the inverter which these readings are from
    std::map<std::string, MQTT_READING> readings;
    std::set<std::string> announced;            // sensors whose discovery config has been sent
    int availability;                           // last published, -1 = not since connecting
    bool changed;
} MQTT_INVERTER;


class MqttPublisher : public wxEvtHandler
{//=====================================
    public:
        MqttPublisher();
        ~MqttPublisher();
        void Reconfigure();
        void Shutdown();
        void Readings(int inv, INVERTER_RESPONSE *ir, float temperature);
        wxString StatusText();

    private:
        void OnTimer(wxTimerEvent &event);
        void Connected();
        void Failed(const wxString &reason);
        void CheckAddress(int inv);
        void PublishInverter(int inv);
        void PublishTotal();
        void ResetAnnounced();
        void Announce(const std::string &node, const std::string &device, const std::string &state_topic,
                      const std::string &availability_topic, const MQTT_SENSOR *sensor);
        void Unannounce(const std::string &node, const std::string &key);
        std::string InverterNode(int address);
        std::string InverterTopic(int address);

        MqttConnection conn;
        wxTimer timer;
        MQTT_INVERTER inverter[N_INV];
        std::set<std::string> total_announced;
        int total_availability;
        time_t last_publish;
        time_t retry_at;
        int retry_wait;
        bool failed;              // the last attempt failed, only the first failure is logged
        wxString failed_time;
        std::string prefix;       // as connected
        std::string discovery;
        std::string node;
};


MqttPublisher::MqttPublisher()
{//===========================
    int inv;

    for(inv = 0; inv < N_INV; inv++)
    {
        inverter[inv].address = 0;
        inverter[inv].availability = -1;
        inverter[inv].changed = false;
    }
    total_availability = -1;
    last_publish = 0;
    retry_at = 0;
    retry_wait = MQTT_RETRY_MIN;
    failed = false;

    conn.on_connected = [this]() { Connected(); };
    conn.on_failed = [this](const wxString &reason) { Failed(reason); };

    timer.SetOwner(this);
    Bind(wxEVT_TIMER, &MqttPublisher::OnTimer, this);
    timer.Start(1000);
}


MqttPublisher::~MqttPublisher()
{//============================
    timer.Stop();
}


std::string MqttPublisher::InverterNode(int address)
{//=================================================
    return(node + "_inverter" + std::to_string(address));
}


std::string MqttPublisher::InverterTopic(int address)
{//==================================================
    return(prefix + "/inverter" + std::to_string(address));
}


wxString MqttPublisher::StatusText()
{//=================================
    if(!mqtt.enabled)
        return(_T("MQTT: off"));
    if(conn.IsConnected())
        return(_T("MQTT: connected"));
    if(failed)
        return(_T("MQTT: failed at ") + failed_time);
    return(_T("MQTT: connecting"));
}


void MqttPublisher::ResetAnnounced()
{//=================================
    int inv;

    for(inv = 0; inv < N_INV; inv++)
    {
        inverter[inv].announced.clear();
        inverter[inv].availability = -1;
        inverter[inv].changed = true;
    }
    total_announced.clear();
    total_availability = -1;
}


void MqttPublisher::Shutdown()
{//===========================
    // Mark everything as unavailable.  Closing gracefully stops the broker from
    // publishing the will, so publish "offline" here instead.
    if(conn.IsConnected())
        conn.Publish(prefix + "/status", "offline", true);
    conn.Close(true);
}


void MqttPublisher::Reconfigure()
{//==============================
    Shutdown();
    ResetAnnounced();
    failed = false;
    retry_wait = MQTT_RETRY_MIN;
    retry_at = 0;   // connect at the next tick
    MqttUpdateStatus();
}


void MqttPublisher::Connected()
{//============================
    LogMessage(_T("MQTT connected to ") + conn.Where(), 1);
    failed = false;
    retry_wait = MQTT_RETRY_MIN;
    ResetAnnounced();
    conn.Publish(prefix + "/status", "online", true);
    last_publish = 0;   // publish the readings now
    MqttUpdateStatus();
}


void MqttPublisher::Failed(const wxString &reason)
{//===============================================
    // only write the first of a series of failures to the log file
    LogMessage(_T("MQTT: ") + reason, failed ? 0 : 1);
    if(!failed)
        failed_time = wxDateTime::Now().Format(_T("%H:%M"));
    failed = true;
    retry_at = time(NULL) + retry_wait;
    retry_wait *= 2;
    if(retry_wait > MQTT_RETRY_MAX)
        retry_wait = MQTT_RETRY_MAX;
    MqttUpdateStatus();
}


void MqttPublisher::CheckAddress(int inv)
{//======================================
    // If the address has changed, the readings and sensors belong to another inverter
    MQTT_INVERTER *mi = &inverter[inv];

    if(mi->address == inverter_address[inv])
        return;

    if((mi->address != 0) && conn.IsConnected())
        conn.Publish(InverterTopic(mi->address) + "/availability", "offline", true);

    mi->address = inverter_address[inv];
    mi->readings.clear();
    mi->announced.clear();
    mi->availability = -1;
    mi->changed = true;
}


void MqttPublisher::Readings(int inv, INVERTER_RESPONSE *ir, float temperature)
{//============================================================================
    MQTT_INVERTER *mi = &inverter[inv];
    double *etot = inverters[inv].energy_total;
    int ix;

    CheckAddress(inv);

    auto set = [mi](const MQTT_SENSOR *table, const char *key, double value) {
        const MQTT_SENSOR *sensor = FindSensor(table, key);
        if((sensor != NULL) && std::isfinite(value))
        {
            MQTT_READING r = {value, sensor};
            mi->readings[key] = r;
        }
    };

    set(inverter_sensors, "power", ir->pw[0]);

    if(ir->flags & IR_HAS_POWERIN)
    {
        set(inverter_sensors, "input1_power", ir->pw[1]);
        set(inverter_sensors, "input1_voltage", ir->vt[1]);
        set(inverter_sensors, "input1_current", ir->ct[1]);
        set(inverter_sensors, "input2_power", ir->pw[2]);
        set(inverter_sensors, "input2_voltage", ir->vt[2]);
        set(inverter_sensors, "input2_current", ir->ct[2]);
        set(inverter_sensors, "efficiency", ir->effic);
    }

    if(ir->flags & IR_HAS_TEMPR)
        set(inverter_sensors, "temperature", temperature);

    if(ir->flags & IR_HAS_ENERGY_TODAY)
        set(inverter_sensors, "energy_today", etot[0]);

    if((ir->flags & IR_HAS_ENERGY) && (etot[4] > 0))   // zero means that the totals aren't available
    {
        set(inverter_sensors, "energy_week", etot[1]);
        set(inverter_sensors, "energy_month", etot[2]);
        set(inverter_sensors, "energy_year", etot[3]);
        set(inverter_sensors, "energy_total", etot[4]);
        set(inverter_sensors, "energy_partial", etot[5]);
    }

    if(ir->flags & IR_HAS_PEAK)
        set(inverter_sensors, "peak_today", ir->dsp[DSP_PEAK_TODAY]);

    if(ir->flags & IR_HAS_EXTRA)
    {
        // all the chosen extra readings are read together, so any others are no longer wanted
        for(std::map<std::string, MQTT_READING>::iterator it = mi->readings.begin(); it != mi->readings.end(); )
        {
            if(it->second.sensor->dsp_code > 0)
                it = mi->readings.erase(it);
            else
                ++it;
        }

        for(ix = 0; ix < N_EXTRA_READINGS; ix++)
        {
            int code = extra_readings[ix].dsp_code;
            if((code <= 0) || ((extra_readings[ix].status_slot == 0) && (extra_readings[ix].graph_slot == 0)))
                continue;

            for(const MQTT_SENSOR *s = extra_sensors; s->key != NULL; s++)
            {
                if(s->dsp_code == code)
                {
                    set(extra_sensors, s->key, ir->dsp[code] * extra_reading_types[extra_readings[ix].type].multiplier);
                    break;
                }
            }
        }
    }

    mi->changed = true;
}


void MqttPublisher::Announce(const std::string &node_id, const std::string &device, const std::string &state_topic,
                             const std::string &availability_topic, const MQTT_SENSOR *sensor)
{//================================================================================================================
    std::string c = "{";

    c += "\"name\":" + JsonString(sensor->name);
    c += ",\"unique_id\":" + JsonString(node_id + "_" + sensor->key);
    c += ",\"state_topic\":" + JsonString(state_topic);
    c += ",\"value_template\":" + JsonString(std::string("{{ value_json.") + sensor->key + " }}");
    if(sensor->unit != NULL)
        c += ",\"unit_of_measurement\":" + JsonString(sensor->unit);
    if(sensor->device_class != NULL)
        c += ",\"device_class\":" + JsonString(sensor->device_class);
    if(sensor->state_class != NULL)
        c += ",\"state_class\":" + JsonString(sensor->state_class);
    c += ",\"suggested_display_precision\":" + std::to_string(sensor->precision);
    c += ",\"availability_mode\":\"all\",\"availability\":[{\"topic\":" + JsonString(prefix + "/status") + "}";
    c += ",{\"topic\":" + JsonString(availability_topic) + "}]";
    c += ",\"device\":" + device;
    c += ",\"origin\":{\"name\":\"Aurora Monitor\",\"support_url\":\"https://github.com/uklad/auroramon\"}";
    c += "}";

    conn.Publish(discovery + "/sensor/" + node_id + "/" + sensor->key + "/config", c, true);
}


void MqttPublisher::Unannounce(const std::string &node_id, const std::string &key)
{//===============================================================================
    // an empty retained config removes the sensor from Home Assistant
    conn.Publish(discovery + "/sensor/" + node_id + "/" + key + "/config", std::string(), true);
}


void MqttPublisher::PublishInverter(int inv)
{//=========================================
    MQTT_INVERTER *mi = &inverter[inv];
    std::string topic;
    std::string node_id;
    std::string device;
    std::string state;
    int available;

    CheckAddress(inv);
    if(mi->address == 0)
        return;

    topic = InverterTopic(mi->address);
    node_id = InverterNode(mi->address);

    available = (inverters[inv].alive > 0) ? 1 : 0;
    if(mi->availability != available)
    {
        conn.Publish(topic + "/availability", available ? "online" : "offline", true);
        mi->availability = available;
    }

    if(!mi->changed || mi->readings.empty())
        return;

    device = "{\"identifiers\":[" + JsonString(node_id) + "]";
    device += ",\"name\":" + JsonString("Aurora inverter " + std::to_string(mi->address));
    device += ",\"manufacturer\":\"Power-One\",\"model\":\"Aurora\"}";

    // remove sensors which are no longer read, such as an extra reading which was turned off
    for(std::set<std::string>::iterator it = mi->announced.begin(); it != mi->announced.end(); )
    {
        if(mi->readings.find(*it) == mi->readings.end())
        {
            Unannounce(node_id, *it);
            it = mi->announced.erase(it);
        }
        else
            ++it;
    }

    state = "{";
    for(std::map<std::string, MQTT_READING>::iterator it = mi->readings.begin(); it != mi->readings.end(); ++it)
    {
        if(mi->announced.find(it->first) == mi->announced.end())
        {
            Announce(node_id, device, topic + "/state", topic + "/availability", it->second.sensor);
            mi->announced.insert(it->first);
        }
        if(state.size() > 1)
            state += ",";
        state += JsonString(it->first) + ":" + JsonNumber(it->second.value, it->second.sensor->precision);
    }
    state += "}";

    if(conn.Publish(topic + "/state", state, true))
        mi->changed = false;
}


void MqttPublisher::PublishTotal()
{//===============================
    // The total of both inverters, only when there are two
    std::string topic = prefix + "/total";
    std::string node_id = node + "_total";
    std::string device;
    std::string state;
    double power = 0;
    double energy = 0;
    bool got_energy = false;
    int available = ((inverter_address[0] != 0) && (inverter_address[1] != 0)) ? 1 : 0;
    int inv;

    if(total_availability != available)
    {
        conn.Publish(topic + "/availability", available ? "online" : "offline", true);
        total_availability = available;
    }
    if(!available)
        return;

    for(inv = 0; inv < N_INV; inv++)
    {
        std::map<std::string, MQTT_READING>::iterator it;
        MQTT_INVERTER *mi = &inverter[inv];

        if((inverters[inv].alive > 0) && ((it = mi->readings.find("power")) != mi->readings.end()))
            power += it->second.value;
        if((it = mi->readings.find("energy_today")) != mi->readings.end())
        {
            energy += it->second.value;
            got_energy = true;
        }
    }

    device = "{\"identifiers\":[" + JsonString(node_id) + "],\"name\":\"Aurora Monitor total\",\"manufacturer\":\"Power-One\",\"model\":\"Aurora\"}";

    state = "{\"power\":" + JsonNumber(power, 0);
    if(got_energy)
        state += ",\"energy_today\":" + JsonNumber(energy, 3);
    state += "}";

    for(const MQTT_SENSOR *s = total_sensors; s->key != NULL; s++)
    {
        if((strcmp(s->key, "energy_today") == 0) && !got_energy)
            continue;
        if(total_announced.find(s->key) == total_announced.end())
        {
            Announce(node_id, device, topic + "/state", topic + "/availability", s);
            total_announced.insert(s->key);
        }
    }
    conn.Publish(topic + "/state", state, true);
}


void MqttPublisher::OnTimer(wxTimerEvent& WXUNUSED(event))
{//=======================================================
    time_t now = time(NULL);
    int inv;

    if(!mqtt.enabled)
        return;

    if(conn.IsIdle())
    {
        if(now >= retry_at)
        {
            prefix = TopicPrefix(mqtt.prefix, "auroramon");
            discovery = TopicPrefix(mqtt.discovery, "homeassistant");
            node = NodeId(prefix);
            conn.Start(mqtt, RandomClientId("auroramon"), prefix + "/status");
            MqttUpdateStatus();
        }
        return;
    }

    if(!conn.IsConnected())
        return;

    for(inv = 0; inv < N_INV; inv++)
        CheckAddress(inv);   // an inverter may have been removed in Settings -> Setup

    if((now - last_publish) < mqtt.interval)
        return;
    last_publish = now;

    for(inv = 0; inv < N_INV; inv++)
        PublishInverter(inv);
    PublishTotal();
}


//=======================================================================================
//  MqttTester: for the Test button in the MQTT dialog
//=======================================================================================

class MqttTester
{//==============
    public:
        MqttTester();
        void Start(const MQTT_SETTINGS &settings, wxStaticText *result_text, wxButton *test_button);
        void Cancel();

    private:
        void Done(const wxString &text);

        MqttConnection conn;
        wxStaticText *result;
        wxButton *button;
        bool active;
};


MqttTester::MqttTester()
{//=====================
    result = NULL;
    button = NULL;
    active = false;
    // these ignore a test which was cancelled when the dialog closed
    conn.on_connected = [this]() {
        if(!active)
            return;
        wxString where = conn.Where();
        conn.Close(true);
        Done(_T("Connected to the broker at ") + where + _T("."));
    };
    conn.on_failed = [this](const wxString &reason) {
        // eg. "Can't connect to 192.168.1.5:1883." or "Refused, not authorized."
        if(active)
            Done(reason.Left(1).Upper() + reason.Mid(1) + _T("."));
    };
}


void MqttTester::Start(const MQTT_SETTINGS &settings, wxStaticText *result_text, wxButton *test_button)
{//===================================================================================================
    if(active)
        return;

    result = result_text;
    button = test_button;

    if(settings.host.empty())
    {
        Done(_T("Enter the broker's address first."));
        return;
    }

    active = true;
    button->Disable();
    result->SetLabelText(_T("Testing..."));
    conn.Start(settings, RandomClientId("auroramon-test"), std::string());   // no will, it isn't publishing
}


void MqttTester::Cancel()
{//======================
    if(active)
    {
        active = false;
        conn.Close(false);
    }
}


void MqttTester::Done(const wxString &text)
{//========================================
    active = false;
    result->SetLabelText(text);
    result->Wrap(result->GetSize().x);
    button->Enable();
}


//=======================================================================================
//  Called from the rest of the program
//=======================================================================================

static MqttPublisher *publisher = NULL;
static MqttTester *tester = NULL;


void MqttStart()
{//=============
    if(publisher == NULL)
        publisher = new MqttPublisher();
    MqttUpdateStatus();
}


void MqttReadings(int inv, INVERTER_RESPONSE *ir, float temperature)
{//=================================================================
    if(mqtt.enabled && (publisher != NULL))
        publisher->Readings(inv, ir, temperature);
}


void MqttRestart()
{//===============
    // the settings have changed
    if(publisher != NULL)
        publisher->Reconfigure();
}


void MqttUpdateStatus()
{//====================
    if(publisher != NULL)
        mainframe->SetStatusText(publisher->StatusText(), 2);
    else
        mainframe->SetStatusText(mqtt.enabled ? _T("MQTT: connecting") : _T("MQTT: off"), 2);
}


void MqttTestStart(const MQTT_SETTINGS &settings, wxStaticText *result, wxButton *button)
{//=====================================================================================
    if(tester == NULL)
        tester = new MqttTester();
    tester->Start(settings, result, button);
}


void MqttTestCancel()
{//==================
    if(tester != NULL)
        tester->Cancel();
}


void MqttShutdown()
{//================
    shutting_down = true;
    if(publisher != NULL)
        publisher->Shutdown();
    delete publisher;
    publisher = NULL;
    delete tester;
    tester = NULL;
}
