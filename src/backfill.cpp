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

// Fills the gaps in the 5 minute energy files (yyyy_out/e5min_yyyymmdd.txt), and
// in the data sent to pvoutput.org, from the 10 second energy data which is
// retrieved from the inverters.
//
// The 5 minute file has a line for each 5 minute period while the program was
// running and the inverters were producing, and these are the readings which
// were sent to pvoutput.org.  So a period which is missing from it, but has
// retrieved 10 second data, is a gap.  Its power is the average of the 10 second
// data in the period.  The energy so far today is worked out from the nearest
// line in the file, before the gap if possible, plus or minus the energy in the
// 10 second data in between.
//
// With two inverters, a period is only filled when both inverters' retrieved
// data covers it, because pvoutput.org gets their total.

#include <map>
#include <string>
#include <vector>
#include <wx/filename.h>
#include "auroramon.h"

#define BACKFILL_MAX_AGE_DAYS  13   // pvoutput.org takes statuses up to 14 days old
#define BACKFILL_MIN_SAMPLES    6   // a period needs at least a minute of 10 second data
#define N_PERIODS             288   // 5 minute periods in a day


// The retrieved 10 second data of one day, by 5 minute period.  Period 'q'
// is the one which ends at q*5 minutes, which is the time of its status.
struct BACKFILL_DAY
{
    double wh[N_PERIODS];
    int samples[N_PERIODS];

    BACKFILL_DAY()
    {
        memset(wh, 0, sizeof(wh));
        memset(samples, 0, sizeof(samples));
    }
};

typedef std::map<std::string, BACKFILL_DAY> BACKFILL_DAYS;   // by yyyymmdd


static time_t LocalTime(int year, int month, int day, int secs)
{//============================================================
    struct tm t;

    memset(&t, 0, sizeof(t));
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_sec = secs;     // mktime() normalises this
    t.tm_isdst = -1;
    return(mktime(&t));
}


wxString TenSecFileName(int address)
{//=================================
    return(data_dir + wxString::Format(_T("/system/energy_10sec_%d.txt"), address));
}


// Reads one inverter's retrieved data, which covers from its first reading
// to when it was retrieved.
static bool ReadTenSec(int address, time_t &first, time_t &retrieved, BACKFILL_DAYS &days)
{//=====================================================================================
    FILE *f;
    wxString fname = TenSecFileName(address);
    int year, month, day, hours, mins, secs;
    float power;
    int seconds;
    int q;
    time_t t;
    char ymd[12];
    char buf[100];

    if(!wxFileExists(fname))
        return(false);
    retrieved = wxFileModificationTime(fname);
    first = 0;

    if((f = fopen(fname.mb_str(wxConvLocal), "r")) == NULL)
        return(false);

    while(fgets(buf, sizeof(buf), f) != NULL)
    {
        if(sscanf(buf, "%4d%2d%2d %d:%d:%d %f", &year, &month, &day, &hours, &mins, &secs, &power) != 7)
            continue;
        seconds = hours*3600 + mins*60 + secs;
        if((seconds < 0) || (seconds >= 86400) || (power < 0) || (power > 1000000))
            continue;
        t = LocalTime(year, month, day, seconds);
        if(t > retrieved)
            continue;   // not possible, unless a clock is wrong
        if((first == 0) || (t < first))
            first = t;

        q = seconds/300 + 1;
        if(q >= N_PERIODS)
            continue;   // after 23:55, it would be the next day's 00:00
        snprintf(ymd, sizeof(ymd), "%.4d%.2d%.2d", year, month, day);
        days[ymd].wh[q] += power * 10 / 3600;
        days[ymd].samples[q]++;
    }
    fclose(f);
    return(first != 0);
}


// One line of a 5 minute file
struct E5MIN_LINE
{
    std::string text;
    int energy;    // Wh so far today
};


// Fills the gaps in one day.  Returns the number of periods filled.
static int BackfillDay(const std::string &ymd, const BACKFILL_DAY &data, time_t cov_start, time_t cov_end,
                       time_t now, int &n_queued, int &n_skipped)
{//===================================================================================================
    int year, month, day;
    int hours, mins, energy, power;
    int q;
    int q0, q1;
    int filled = 0;
    int pvo_period = pvoutput.live_updates & PVO_PERIOD;
    double cum[N_PERIODS];
    double e;
    double lo;
    double hi;
    double last_e = 0;
    time_t day_start;
    FILE *f;
    char buf[200];
    wxString fname;
    wxString fname2;
    wxString dir;
    std::map<int, E5MIN_LINE> lines;    // by period
    std::map<int, E5MIN_LINE> new_lines;
    std::map<int, E5MIN_LINE>::iterator it;

    if(sscanf(ymd.c_str(), "%4d%2d%2d", &year, &month, &day) != 3)
        return(0);
    day_start = LocalTime(year, month, day, 0);

    dir = data_dir + wxString::Format(_T("/%.4d_out"), year);
    fname = dir + _T("/e5min_") + wxString(ymd.c_str(), wxConvLocal) + _T(".txt");

    if((f = fopen(fname.mb_str(wxConvLocal), "r")) != NULL)
    {
        while(fgets(buf, sizeof(buf), f) != NULL)
        {
            std::string text(buf);
            while(!text.empty() && ((text[text.size()-1] == '\n') || (text[text.size()-1] == '\r')))
                text.erase(text.size()-1);
            if(text.empty())
                continue;
            if(sscanf(buf, "%d:%d,%d", &hours, &mins, &energy) != 3)
            {
                // not a line which this program wrote, so leave this day alone
                fclose(f);
                return(0);
            }
            q = (hours*60 + mins)/5;
            if((q >= 0) && (q < N_PERIODS))
            {
                lines[q].text = text;
                lines[q].energy = energy;
            }
        }
        fclose(f);
    }

    cum[0] = 0;
    for(q=1; q < N_PERIODS; q++)
        cum[q] = cum[q-1] + data.wh[q];

    for(q=1; q < N_PERIODS; q++)
    {
        if((it = lines.find(q)) != lines.end())
        {
            last_e = it->second.energy;
            continue;    // not a gap
        }
        if(data.samples[q] < BACKFILL_MIN_SAMPLES)
            continue;    // the inverters weren't producing
        if((LocalTime(year, month, day, (q-1)*300) < cov_start) || (LocalTime(year, month, day, q*300) > cov_end)
           || (LocalTime(year, month, day, q*300) > now))
            continue;    // not all of this period was retrieved

        // the nearest lines before and after the gap
        q0 = -1;
        q1 = -1;
        it = lines.lower_bound(q);
        if(it != lines.end())
            q1 = it->first;
        if(it != lines.begin())
        {
            --it;
            q0 = it->first;
        }

        if((q0 >= 0) && (LocalTime(year, month, day, q0*300) >= cov_start))
            e = lines[q0].energy + (cum[q] - cum[q0]);
        else
        if(cov_start < day_start)
            e = cum[q];     // all of today was retrieved
        else
        if((q1 >= 0) && (LocalTime(year, month, day, q1*300) <= cov_end))
            e = lines[q1].energy - (cum[q1] - cum[q]);
        else
        {
            n_skipped++;    // the energy so far today isn't known
            continue;
        }

        // The 10 second data doesn't quite match the inverter's energy counter,
        // so keep the energy between the lines on each side of the gap.
        lo = last_e;
        hi = (q1 >= 0) ? lines[q1].energy : 1e12;
        if(e > hi)
            e = hi;
        if(e < lo)
            e = lo;

        energy = (int)(e + 0.5);
        power = (int)(data.wh[q] * 12 + 0.5);    // the average over 5 minutes
        last_e = energy;

        snprintf(buf, sizeof(buf), "%2d:%.2d,%6d,%5d", q/12, (q%12)*5, energy, power);
        new_lines[q].text = buf;
        new_lines[q].energy = energy;
        filled++;

        if((pvo_period > 0) && ((pvo_period == 1) || ((q % pvo_period) == 0)))
        {
            wxString line = wxString::Format(_T("%s,%d:%.2d,%d"), wxString(ymd.c_str(), wxConvLocal), q/12, (q%12)*5, energy);
            if(pvoutput.live_updates & PVO_INSTANTANEOUS)
                line += wxString::Format(_T(",%d"), power);
            PvoQueueStatus(line);
            n_queued++;
        }
    }

    if(filled == 0)
        return(0);

    // rewrite the 5 minute file with the new lines, in time order
    for(it = new_lines.begin(); it != new_lines.end(); ++it)
        lines[it->first] = it->second;

    if(!wxDirExists(dir))
        wxFileName::Mkdir(dir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    fname2 = fname + _T("2");
    if((f = fopen(fname2.mb_str(wxConvLocal), "w")) == NULL)
        return(filled);
    for(it = lines.begin(); it != lines.end(); ++it)
        fprintf(f, "%s\n", it->second.text.c_str());
    fclose(f);
    wxRenameFile(fname2, fname, true);
    return(filled);
}


// Called after 10 second energy data has been retrieved from an inverter, and
// saved with TenSecFileName().
void PvoBackfill()
{//===============
    int inv;
    int n_inverters = 0;
    int n_filled = 0;
    int n_queued = 0;
    int n_skipped = 0;
    time_t first, retrieved;
    time_t cov_start = 0;
    time_t cov_end = 0;
    time_t now = time(NULL);
    time_t oldest;
    BACKFILL_DAYS days;
    BACKFILL_DAYS::iterator it;
    int q;

    for(inv=0; inv < N_INV; inv++)
    {
        if(inverter_address[inv] == 0)
            continue;

        BACKFILL_DAYS inv_days;
        if(!ReadTenSec(inverter_address[inv], first, retrieved, inv_days))
            return;    // we need the data of all the inverters

        if((n_inverters == 0) || (first > cov_start))
            cov_start = first;
        if((n_inverters == 0) || (retrieved < cov_end))
            cov_end = retrieved;
        n_inverters++;

        for(it = inv_days.begin(); it != inv_days.end(); ++it)
        {
            BACKFILL_DAY &total = days[it->first];
            for(q=0; q < N_PERIODS; q++)
            {
                total.wh[q] += it->second.wh[q];
                if(it->second.samples[q] > total.samples[q])
                    total.samples[q] = it->second.samples[q];
            }
        }
    }
    if((n_inverters == 0) || (cov_start >= cov_end))
        return;

    // not older than pvoutput.org accepts
    struct tm *btime = localtime(&now);
    oldest = LocalTime(btime->tm_year+1900, btime->tm_mon+1, btime->tm_mday - BACKFILL_MAX_AGE_DAYS, 0);
    if(cov_start < oldest)
        cov_start = oldest;

    for(it = days.begin(); it != days.end(); ++it)
        n_filled += BackfillDay(it->first, it->second, cov_start, cov_end, now, n_queued, n_skipped);

    // The earliest day of the retrieved data usually can't be filled, because its
    // start may have been overwritten, so that alone isn't worth a message.
    if(n_filled > 0)
    {
        wxString message = wxString::Format(_T("Filled %d missing 5 minute readings from the retrieved 10 second data, %d queued for PVOutput.org"),
                                            n_filled, n_queued);
        if(n_skipped > 0)
            message += wxString::Format(_T(". %d others couldn't be filled, because the energy so far that day isn't known"), n_skipped);
        LogMessage(message, 1);
        PvoUpdateStatus();
    }
}
