# Interleaved build A/B (M46): is build B slower or faster than build A?
#
#   pwsh Tools/Perf/ab.ps1 -A <path\AtomGame.exe> -B <path\AtomGame.exe> -Level night_street [-Rounds 8] [-Seconds 8] [-TimeoutSeconds N]
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
#
# Hang guard: every run has a wall-clock limit (default: 60 s to start +
# Seconds + 30 s). The game's own script timeout counts *game* time, which
# stops if the game stops drawing frames (a covered or minimised window, a
# driver stall, a dialog box), so this script watches the clock itself. A
# run over its limit is killed and retried once; a second hang stops the
# comparison, keeping the rounds already measured. The environment
# variables it sets are restored afterwards, however it ends.
param(
    [Parameter(Mandatory)] [string]$A,
    [Parameter(Mandatory)] [string]$B,
    [Parameter(Mandatory)] [string]$Level,
    [int]$Rounds = 8,
    [double]$Seconds = 8,
    [double]$TimeoutSeconds = 0 # 0: 60 + Seconds + 30
)
$ErrorActionPreference = "Stop"
# Numbers with a dot, whatever the system's locale.
[System.Threading.Thread]::CurrentThread.CurrentCulture = [System.Globalization.CultureInfo]::InvariantCulture
foreach ($exe in @($A, $B)) {
    if (-not (Test-Path $exe)) { throw "No game at $exe" }
}
$limit = if ($TimeoutSeconds -gt 0) { $TimeoutSeconds } else { 60 + $Seconds + 30 }

# M62: on battery this laptop ran about 3x slower (M46) - say so up front.
. "$PSScriptRoot/../Dev/common.ps1"
if ((Get-PowerSource) -eq "battery") {
    Write-Host "WARNING: on battery - timings won't compare with plugged-in runs. Plug in and run again." -ForegroundColor Yellow
}

# One run: the level, idle, then quit; its PERF blocks' median.
$temp = [System.IO.Path]::GetTempPath()
$script = Join-Path $temp "atom_ab.atomtest"
$stdout = Join-Path $temp "atom_ab_out.txt"
$stderr = Join-Path $temp "atom_ab_err.txt"
# Warm-up (300 frames) comes first; then enough time for several blocks.
"wait $Seconds`nquit`n" | Set-Content -NoNewline -Path $script

$variables = @{ ATOM_PERF_LOG = "1"; ATOM_VSYNC = "0"; ATOM_START_LEVEL = $Level; ATOM_TEST_SCRIPT = $script }
$saved = @{}
foreach ($name in $variables.Keys) { $saved[$name] = [Environment]::GetEnvironmentVariable($name) }

$killed = 0

# The run's median frame time, or $null if it hung (and was killed).
function Invoke-Run([string]$exe) {
    $process = Start-Process -FilePath $exe -PassThru -NoNewWindow `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if (-not $process.WaitForExit([int]($limit * 1000))) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit(5000) | Out-Null
        return $null
    }
    $output = Get-Content -Raw $stdout
    $medians = [regex]::Matches("$output", "PERF block \d+ samples \d+ median ([0-9.]+)") |
        ForEach-Object { [double]$_.Groups[1].Value }
    if ($medians.Count -eq 0) { throw "$exe printed no PERF lines (does it have ATOM_PERF_LOG?)" }
    $sorted = $medians | Sort-Object
    return $sorted[[int][math]::Floor(($sorted.Count - 1) / 2)]
}

# A run, retried once if it hangs; a second hang ends the comparison.
function Measure-Run([string]$exe, [int]$round) {
    for ($attempt = 1; $attempt -le 2; $attempt++) {
        $ms = Invoke-Run $exe
        if ($null -ne $ms) { return $ms }
        $script:killed++
        Write-Host ("round {0}: {1} timed out after {2:N0} s, killed{3}" -f $round, $exe, $limit,
            $(if ($attempt -eq 1) { "; retrying" } else { "" }))
    }
    throw "HUNG"
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
$stopped = $null
try {
    foreach ($name in $variables.Keys) { [Environment]::SetEnvironmentVariable($name, $variables[$name]) }
    Write-Host ("{0,-6} {1,-6} {2,9} {3,9} {4,9}" -f "Round", "Order", "A ms", "B ms", "B-A ms")
    for ($round = 1; $round -le $Rounds; $round++) {
        $aFirst = ($round % 2) -eq 1
        # (PowerShell names ignore case: $aMs, not $a, or it would overwrite $A.)
        try {
            if ($aFirst) { $aMs = Measure-Run $A $round; $bMs = Measure-Run $B $round }
            else { $bMs = Measure-Run $B $round; $aMs = Measure-Run $A $round }
        }
        catch {
            if ("$_" -ne "HUNG") { throw }
            $stopped = $round
            break
        }
        $deltas += ($bMs - $aMs)
        $aAll += $aMs
        $bAll += $bMs
        Write-Host ("{0,-6} {1,-6} {2,9:N3} {3,9:N3} {4,9:+0.000;-0.000}" -f $round, ($(if ($aFirst) { "AB" } else { "BA" })), $aMs, $bMs, ($bMs - $aMs))
    }
}
finally {
    # However it ends - done, hung, an error, Ctrl+C - the shell is left as it was.
    foreach ($name in $saved.Keys) { [Environment]::SetEnvironmentVariable($name, $saved[$name]) }
    Remove-Item -ErrorAction SilentlyContinue $script, $stdout, $stderr
}

Write-Host ""
if ($killed -gt 0) { Write-Host ("{0} run(s) hung and were killed." -f $killed) }
if ($stopped) {
    Write-Host ("Stopped at round {0}: a run hung twice." -f $stopped)
}
if ($deltas.Count -eq 0) {
    Write-Host "No complete rounds: no result."
    exit 1
}
Write-Host ("A median {0:N3} ms, B median {1:N3} ms (context only)" -f (Median $aAll), (Median $bAll))
Write-Host ("Median paired delta (B - A): {0:+0.000;-0.000} ms over {1} rounds, range {2:+0.000;-0.000}..{3:+0.000;-0.000}" -f `
    (Median $deltas), $deltas.Count, ($deltas | Measure-Object -Minimum).Minimum, ($deltas | Measure-Object -Maximum).Maximum)
if ($stopped) { exit 1 }
