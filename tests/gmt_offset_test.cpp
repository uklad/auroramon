// Prints the UTC offset which auroramon calculates, for each time_t on the command line.
#include <cstdio>
#include <cstdlib>
#include "gmt_offset.h"

int main(int argc, char **argv)
{
    for(int ix = 1; ix < argc; ix++)
    {
        time_t t = (time_t)atoll(argv[ix]);
        printf("%lld %d\n", (long long)t, GmtOffsetOfTime(t));
    }
    return 0;
}
