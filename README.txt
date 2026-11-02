
auroramon-1.07c.zip
Contains the source code files and also binaries for 32-bit and 64-bit
Linux.  The binaries are of version 1.07, they don't include the changes
listed under version 1.08.

setup_auroramon-1.07.exe
Is the installer for the Windows version.


---------
Version 1.08   02.11.2026  (modified from 1.07, not by the original author)

Building now requires wxWidgets 3.1.5 or later (eg. libwxgtk3.2-dev).
The Windows version is built by GitHub Actions.  It has been run on Windows,
but its serial port code hasn't been tested with an inverter.

Crashes and memory errors:
1. Crash when inverter B has data but inverter A has none.
2. Crash on loading a 1.07 log which has no extra-reading columns.  The total
   energy column now has a header, "Etotal".
3. Errors in two message formats, the alarm file was never pruned.
4. Graph buffer overflow when extra readings are added while running.
5. Memory error in the sun calculations with an out of range sunset time.
6. Hang on a stray file in a year directory, such as dx_20120301.txt.
7. Limits on the values read from log files and chartinfo.  Replaced unsafe
   conversions of the data received from the inverter.
8. Clicking past the end of the day on a chart.

Wrong data:
9.  Daily energy retrieved from inverter B was stored as inverter A.
10. Inverter B's log file could miss a change of column headers.
11. Inverter B's energy was omitted from the 5 minute data until inverter A
    had reported.
12. Sun position: the time zone was used as days instead of hours, half-hour
    time zones were truncated, and leap years were one day out.
13. Values too large for the charts now stop at the maximum instead of wrapping
    round.

PVOutput.org:
14. Uploads no longer block the program, so a network failure doesn't stop
    the inverter polling.
15. The default URL is now https, and an existing http://pvoutput.org setting
    is changed to https.
16. The API key is sent in a request header, so it's no longer in the URL or
    the logs.  The config file is now only readable by its owner.

Other:
17. "Settings->Inverter B" opened the settings for inverter A.
18. Saves "every 5 minutes" did not happen every 5 minutes.
19. The date information is cached instead of the data directory being read on
    every redraw.
20. Serial port names may be longer than 19 characters (eg. /dev/serial/by-id/).
21. Polling restarts if the inverter thread can't be started.
22. "Reset partial energy and time" now asks for confirmation.
23. A failed data retrieval is no longer reported as complete.
24. Added tests/fake_inverter.py to run the program without an inverter.

Screen layout:
25. All the dialogs and the status bar now size themselves to their contents
    and the font, so text no longer overlaps the input boxes.  The status bar
    continues on a second row if it doesn't fit across the window.
26. "Display->Inverter status" showed only part of the second inverter.
27. The chart title no longer overlaps the labels of the left scale.
28. The status lights are drawn with paint events, for Wayland.

Inverter clock and daylight saving:
29. New option in Settings->Inverter, default Never: sync the inverter's clock
    to computer time every hour, 6 hours, day or week.  It also syncs at program
    start and when daylight saving starts or ends.  A difference of 5 seconds or
    less is left alone, and more than 26 hours is only reported in the message log.
30. The offset from UTC was wrong for some hours around a daylight saving change.
31. "Set inverter time" was an hour out for a specified time during daylight
    saving, or on a date with different daylight saving from now.
32. The automatic guess of longitude was 15 degrees out if it was first made
    during daylight saving.
33. The chart no longer loses its right part when the clock goes back an hour.

Icon:
34. Added a program icon, see icons/README.txt.

---------
Version 1.07   16.05.2013

1. Fix problem where auroramon sometimes frezzes on startup.

2. Added "total energy" reading at the end of each line in the
   d2_yyyymmdd.txt files.

---------
Version 1.06a  26.03.2012

1.  Fix problem where "Retrieve 10 second energy data" did not fill in
    gaps in the power output graph.

2.  Adjust the times of retrieved energy data to match computer time.


Version 1.06   23.Mar.2012

1.  Double-click on a day in the energy histogram shows the charts for
    that day.

2.  Added horizontal scroll-bar.

3.  Choice of black or white background colour for chart and histogram
    pages.

4.  Inverter->Inverter Information now includes the time difference
    from computer time.

5.  If connection to pvoutput.org is not available, the data is saved
    and uploaded when the connection returns.

6.  Add an option in Settings->Inverter to automatically retrieve
    5 minute energy data each day (into the yyyymmdd_out directory).

7.  Whenever 10 second (or 5 minute) energy data is retrieved from the
    inverter, it is used to fill in any gaps in the power output graph.

---------
Version 1.05a  28.Feb.2012

1. Added 'Inverter temperature' to PVOutput options


Version 1.05  28.Feb.2012

1. Added feature to automatically send Live Updates to pvoutput.org
   every 5, 10, or 15 minutes.

2. Double-click on the graph window opens a small window which shows
   the numerical values for that time position.

3. Display->Select_Day now uses a date-picker dialog.

4. Option to show the date in different formats.

5. Fixes to Inverter->Retrieve_daily_energy.

6. Inverter -> Set_inverter_time now shows a dialog which displays
   compter time and inverter time, and allows the inverter time to
   be set to a specified value, or to match computer time.


---------
Version 1.04b 26.Feb.2012

1. Fix problem with change in "today" date not being recognized. Data
   was being written to yesterday's log file, and yesterday's date and
   data remained on the screen.


Version 1.04a 26.Feb.2012

1. Fix: Display->Select Day doesn't load data.

2. If "Serial port" is blank, don't show spurious error message, don't
   blink the red light.
 

Version 1.04  24.Feb.2012

1. Added files "aurora/2012_out/e5min_yyyymmdd.txt" which contain
Energy Generated and Power Generated values at 5 minute intervals in a
form suitable for pasting into the "Live Loader" screen at www.pvoutput.org.

2. Added "Inverter" menu, including "Set time" and "Reset partial counts".

3. Added "Inverter->Retrieve 10sec energy" to retrieve 10sec energy data
from the inverter.  This includes an option to produce a file of power
generated values at 5 minute intervals, suitable for www.pvoutput.org.

4. Added "Inverter->Retrieve daily energy" to retrieve the daily energy
totals from the inverter.  NOTE, this is experimental and may possible
give wrong information for some models of inverters (please let me know
it this is so).

5. "Inverter Information" now includes the "time running" and
"time connected" information.

6. Fixed: Time values on Windows did not include daylight saving.

7. Added "Peak power today" on the Status Panel (in place of "week energy").

8. Added file "aurora/system/alarm0_2.txt" which contains a list of alarms
which occurred while Aurora Monitor was running.

9. Added the peak power output value for each day to the information
saved in aurora/2012/inverter0_2_2012.txt


---------
Version 1.03  06.Feb.2012

1. Added an "Energy today" graph in Settings -> Extra readings.

2. Energy Today value is now shown to 3 decimal places.

3. Show an updating Energy Today total (for both inverters) at the top left
of the graph window. 

4. Fixed a crash bug.

5. Re-enabled flashing status light in the Windows version.

6. Reverse the sign of the auto Timezone value in the Windows version.

---------
Version 1.02a 03.Feb.2012

1. Flashing of the red/green status indicates has been disabled on
the Windows version because it seems to cause a crash.  I don't know why
but I'll investigate further.

2. Fixed from v1.02: Energy totals (other than "today") were not being
updated.


Version 1.02  02.Feb.2012

1. Added graph type "Solar Intensity" which shows the theoretical energy
in Watts per square metre, taking account of sun elevation and panel
direction and tilt.

2. Added "Settings -> Location" dialog, which includes panel direction and
tilt values.

3. Added "Settings -> Extra readings" dialog which allows additional
measurements from the inverter (such as leakage current) to be displayed
as numeric values or as graphs.

---------
Version 1.01  27.Jan.2012

Initial version.

---------
