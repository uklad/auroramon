# Compare auroramon's UTC offset with the Windows time zone data, for 48 hours around
# each daylight saving change.  This changes the computer's time zone, so it's for a CI machine.
param([string]$Test)
$ErrorActionPreference = 'Stop'

$cases = @(
    @('GMT Standard Time', '2026-03-29'), @('GMT Standard Time', '2026-10-25'),
    @('Eastern Standard Time', '2026-03-08'), @('Eastern Standard Time', '2026-11-01'),
    @('AUS Eastern Standard Time', '2026-04-05'), @('AUS Eastern Standard Time', '2026-10-04'),
    @('Cen. Australia Standard Time', '2026-04-05'), @('Cen. Australia Standard Time', '2026-10-04'),
    @('India Standard Time', '2026-06-01')
)
$original = (tzutil /g)
$env:TZ = $null
$failed = 0
try {
    foreach ($case in $cases) {
        $id = $case[0]
        tzutil /s $id
        $zone = [TimeZoneInfo]::FindSystemTimeZoneById($id)
        $start = [DateTimeOffset]::Parse("$($case[1])T00:00:00Z").AddHours(-24)
        $times = 0..95 | ForEach-Object { $start.AddMinutes(30 * $_).ToUnixTimeSeconds() }
        $lines = & $Test @times
        $wrong = 0
        foreach ($line in $lines) {
            $t, $offset = $line -split ' '
            $truth = $zone.GetUtcOffset([DateTimeOffset]::FromUnixTimeSeconds([long]$t)).TotalSeconds
            if ([int]$offset -ne [int]$truth) { $wrong++ }
        }
        Write-Host ("{0,-30} {1}: {2} results, {3} wrong" -f $id, $case[1], @($lines).Count, $wrong)
        if ($wrong -ne 0 -or @($lines).Count -ne 96) { $failed++ }
    }
} finally {
    tzutil /s $original
}
if ($failed) { throw "$failed time zone cases failed" }
