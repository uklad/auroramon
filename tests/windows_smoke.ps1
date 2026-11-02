# Start the Windows program, open each dialog through its menu command and take a picture of it,
# then quit.  It fails if the program crashes or doesn't exit.  The serial port isn't connected.
param([string]$Exe, [string]$Header, [string]$Out)
$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class Win {
    public delegate bool EnumProc(IntPtr h, IntPtr p);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    [DllImport("user32")] static extern bool EnumWindows(EnumProc f, IntPtr p);
    [DllImport("user32")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32")] static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32", CharSet=CharSet.Unicode)] static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
    [DllImport("user32")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
    public static List<IntPtr> Windows(int processId) {
        var found = new List<IntPtr>();
        EnumWindows((h, p) => {
            uint pid; GetWindowThreadProcessId(h, out pid);
            if (pid == processId && IsWindowVisible(h)) found.Add(h);
            return true;
        }, IntPtr.Zero);
        return found;
    }
    public static string Title(IntPtr h) { var s = new StringBuilder(200); GetWindowText(h, s, 200); return s.ToString(); }
}
'@

$WM_CLOSE = 0x10
$WM_COMMAND = 0x111

# the menu command numbers, from the enum in the header
$ids = @{}
$next = -1
foreach ($line in Get-Content $Header) {
    if ($line -match '^\s*(id\w+)\s*(=\s*(\d+))?\s*,?\s*(//.*)?$') {
        $next = if ($Matches[3]) { [int]$Matches[3] } else { $next + 1 }
        $ids[$Matches[1]] = $next
    } elseif ($next -ge 0 -and $line -match '^\s*\}') { break }
}

function Save-Window([IntPtr]$h, [string]$name) {
    $r = New-Object Win+RECT
    [void][Win]::GetWindowRect($h, [ref]$r)
    $w = $r.R - $r.L; $ht = $r.B - $r.T
    Write-Host ("  {0}: '{1}' {2}x{3}" -f $name, [Win]::Title($h), $w, $ht)
    if ($w -le 0 -or $ht -le 0) { return }
    $bmp = New-Object System.Drawing.Bitmap $w, $ht
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $dc = $g.GetHdc()
    [void][Win]::PrintWindow($h, $dc, 2)
    $g.ReleaseHdc($dc)
    $g.Dispose()
    $bmp.Save((Join-Path $Out "$name.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

function Assert-Running { if ($script:app.HasExited) { throw "The program stopped, exit code $($script:app.ExitCode), during: $args" } }

New-Item -ItemType Directory -Force $Out | Out-Null
$Out = (Resolve-Path $Out).Path
$Exe = (Resolve-Path $Exe).Path

# only the system's libraries, so that a library missing from the package shows up
$env:Path = "$env:SystemRoot\system32;$env:SystemRoot"

$script:app = Start-Process -FilePath $Exe -WorkingDirectory $env:TEMP -PassThru
$main = [IntPtr]::Zero
for ($i = 0; $i -lt 30 -and $main -eq [IntPtr]::Zero; $i++) {
    Start-Sleep 1
    Assert-Running 'start'
    $script:app.Refresh()
    $main = $script:app.MainWindowHandle
}
if ($main -eq [IntPtr]::Zero) { throw 'No main window' }
Start-Sleep 8
Assert-Running 'first polls'
Write-Host "Main window $main, windows: $(([Win]::Windows($script:app.Id)).Count)"
Save-Window $main 'main'

$dialogs = 'idMenuSetup', 'idMenuLocation', 'idMenuSetInverter', 'idMenuEditCharts', 'idMenuEditHistogram',
           'idMenuExtraReadings', 'idMenuPvoutput', 'idMenuExportPvo', 'idMenuSunInfo', 'idMenuInverterState',
           'idMenuInverterSetTime', 'idMenuInverterDailyEnergy', 'idMenuInverter10SecEnergy', 'idMenuHelp', 'idMenuAbout'
foreach ($name in $dialogs) {
    if (-not $ids.ContainsKey($name)) { throw "$name isn't in the header" }
    Write-Host "$name ($($ids[$name]))"
    [void][Win]::PostMessage($main, $WM_COMMAND, [IntPtr]$ids[$name], [IntPtr]::Zero)
    Start-Sleep 2
    Assert-Running $name
    $n = 0
    foreach ($h in [Win]::Windows($script:app.Id)) {
        if ($h -ne $main) { Save-Window $h "$name-$n"; $n++ }
    }
    if ($n -eq 0) { Write-Host '  no window opened' }
    foreach ($h in [Win]::Windows($script:app.Id)) {
        if ($h -ne $main) { [void][Win]::PostMessage($h, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero) }
    }
    Start-Sleep 1
    Assert-Running "closing $name"
}

Save-Window $main 'main-end'
$data = Join-Path "$env:HOMEDRIVE$env:HOMEPATH" 'aurora'
Write-Host "Data directory $data exists: $(Test-Path $data)"
$log = Join-Path $data 'system\message_log.txt'
if (Test-Path $log) { Write-Host '--- message log'; Get-Content $log | Select-Object -First 15 | Write-Host }

[void][Win]::PostMessage($main, $WM_COMMAND, [IntPtr]$ids['idMenuQuit'], [IntPtr]::Zero)
if (-not $script:app.WaitForExit(20000)) {
    Save-Window $main 'quit-hung'
    $script:app.Kill()
    throw "The program didn't exit"
}
Write-Host "Exit code: $($script:app.ExitCode)"
if ($script:app.ExitCode -ne 0) { throw "Exit code $($script:app.ExitCode)" }
