
auroramon-1.07c.zip
Contains the source code files and also binaries for 32-bit and 64-bit
Linux.

setup_auroramon-1.07.exe
Is the installer for the Windows version.


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
