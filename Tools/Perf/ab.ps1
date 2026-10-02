# Interleaved build A/B (M46): is build B slower or faster than build A?
#
#   pwsh Tools/Perf/ab.ps1 -A <path\AtomGame.exe> -B <path\AtomGame.exe> -Level night_street [-Rounds 8] [-Seconds 8]
#
# A laptop's speed drifts (heat, power, boost clocks), so two long runs
# can't be compared. This alternates short runs in the order AB BA AB BA...
# so drift lands on both builds alike; each round's *paired difference*
# (B - A, of the runs' median frame times) is the sample, and the result is
# their median. Each build's own median is printed only for context.
#
# Both builds need ATOM_PERF_LOG (v0.0.7 onward). Trust check: a build
# against itself must give ~0 ms within the spread; if not, don't trust any
# smaller difference. For what a single feature costs, use the in-process
# `bench` harness command instead - it's tighter.
param(
    [Parameter(Mandatory)] [string]$A,
    [Parameter(Mandatory)] [string]$B,
    [Parameter(Mandatory)] [string]$Level,
    [int]$Rounds = 8,
    [double]$Seconds = 8
)
$ErrorActionPreference = "Stop"
# Numbers with a dot, whatever the system's locale.
[System.Threading.Thread]::CurrentThread.CurrentCulture = [System.Globalization.CultureInfo]::InvariantCulture
foreach ($exe in @($A, $B)) {
    if (-not (Test-Path $exe)) { throw "No game at $exe" }
}

# One run: the level, idle, then quit; its PERF blocks' median.
$script = Join-Path ([System.IO.Path]::GetTempPath()) "atom_ab.atomtest"
# Warm-up (300 frames) comes first; then enough time for several blocks.
"wait $Seconds`nquit`n" | Set-Content -NoNewline -Path $script

function Measure-Run([string]$exe) {
    $env:ATOM_PERF_LOG = "1"
    $env:ATOM_VSYNC = "0"
    $env:ATOM_START_LEVEL = $Level
    $env:ATOM_TEST_SCRIPT = $script
    $output = & $exe 2>&1 | Out-String
    $medians = [regex]::Matches($output, "PERF block \d+ samples \d+ median ([0-9.]+)") |
        ForEach-Object { [double]$_.Groups[1].Value }
    if ($medians.Count -eq 0) { throw "$exe printed no PERF lines (does it have ATOM_PERF_LOG?)" }
    $sorted = $medians | Sort-Object
    return $sorted[[int][math]::Floor(($sorted.Count - 1) / 2)]
}

function Median([double[]]$values) {
    $sorted = $values | Sort-Object
    $n = $sorted.Count
    if ($n % 2 -eq 1) { return $sorted[($n - 1) / 2] }
    return ($sorted[$n / 2 - 1] + $sorted[$n / 2]) / 2
}

$deltas = @()
$aAll = @()
$bAll = @()
Write-Host ("{0,-6} {1,-6} {2,9} {3,9} {4,9}" -f "Round", "Order", "A ms", "B ms", "B-A ms")
for ($round = 1; $round -le $Rounds; $round++) {
    $aFirst = ($round % 2) -eq 1
    # (PowerShell names ignore case: $aMs, not $a, or it would overwrite $A.)
    if ($aFirst) { $aMs = Measure-Run $A; $bMs = Measure-Run $B } else { $bMs = Measure-Run $B; $aMs = Measure-Run $A }
    $deltas += ($bMs - $aMs)
    $aAll += $aMs
    $bAll += $bMs
    Write-Host ("{0,-6} {1,-6} {2,9:N3} {3,9:N3} {4,9:+0.000;-0.000}" -f $round, ($(if ($aFirst) { "AB" } else { "BA" })), $aMs, $bMs, ($bMs - $aMs))
}
Write-Host ""
Write-Host ("A median {0:N3} ms, B median {1:N3} ms (context only)" -f (Median $aAll), (Median $bAll))
Write-Host ("Median paired delta (B - A): {0:+0.000;-0.000} ms over {1} rounds, range {2:+0.000;-0.000}..{3:+0.000;-0.000}" -f `
    (Median $deltas), $Rounds, ($deltas | Measure-Object -Minimum).Minimum, ($deltas | Measure-Object -Maximum).Maximum)
