#ifndef GMT_OFFSET_H
#define GMT_OFFSET_H

#include <ctime>

// The difference of local time from UTC at time t, in seconds, including any daylight saving.
inline int GmtOffsetOfTime(time_t t)
{
    struct tm local;
#ifdef _WIN32
    localtime_s(&local, &t);
    return (int)(_mkgmtime(&local) - t);
#else
    localtime_r(&t, &local);
    return (int)local.tm_gmtoff;
#endif
}

#endif
