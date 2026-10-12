# ATOM ARENA's benchmark matrix (v0.0.15, M94): every scenario, several
# rounds, then one table.
#
#   pwsh Tools/Perf/arena.ps1 [-Exe build/bin/Release/AtomArena/AtomArena.exe] [-Rounds 3]
#        [-Scenarios update_500,collision_500] [-Frames 1200] [-Warmup 120] [-Seed 42]
#        [-Label baseline] [-TimeoutSeconds 120]
#
# Rounds run the whole matrix in turn (round 1: every scenario, round 2:
# every scenario...), so a laptop's drift - heat, boost clocks - spreads
# over all scenarios instead of landing on the last ones. Vsync is off
# (ATOM_VSYNC=0, ATOM_PRESENT=immediate): a frame capped by the display
# measures the display. Each run writes its own report
# (out/arena/<label>/<scenario>-r<round>.txt); this script reads them back
# into <label>.csv and prints, per scenario, the median over rounds of each
# run's median frame / sim / submit time, with the spread (min..max).
#
# Plugged in, on the laptop's own screen, the window left alone (README,
# Measuring performance). A hung run is killed after -TimeoutSeconds and
# recorded as missing.
param(
    [string]$Exe = "build/bin/Release/AtomArena/AtomArena.exe",
    [int]$Rounds = 3,
    [string[]]$Scenarios = @(),
    [int]$Frames = 1200,
    [int]$Warmup = 120,
    [int]$Seed = 42,
    [string]$Label = "baseline",
    # M96: a build with the minimal tracer (-DATOM_TRACE=ON) records each run
    # (ATOM_TRACE): <scenario>-r<round>.json (Perfetto), -zones.csv, -frames.txt.
    [switch]$Trace,
    [double]$TimeoutSeconds = 120
)
$ErrorActionPreference = "Stop"
[System.Threading.Thread]::CurrentThread.CurrentCulture = [System.Globalization.CultureInfo]::InvariantCulture
$repo = Resolve-Path "$PSScriptRoot/../.."
. "$PSScriptRoot/../Dev/common.ps1"

$exePath = if ([System.IO.Path]::IsPathRooted($Exe)) { $Exe } else { Join-Path $repo $Exe }
if (-not (Test-Path $exePath)) { throw "No ATOM ARENA at $exePath (build it: cmake --build build --config Release --target AtomArena)" }
$power = Get-PowerSource
if ($power -eq "battery") {
    Write-Host "WARNING: on battery - timings won't compare with plugged-in runs." -ForegroundColor Yellow
}
# "a,b" from another shell arrives as one string.
$Scenarios = @($Scenarios | ForEach-Object { $_ -split "," } | Where-Object { $_ })
# The default matrix: StandardScenarios() in ArenaBenchmark.cpp (keep the two the same).
if ($Scenarios.Count -eq 0) {
    $Scenarios = @("update_50","collision_50","collision_naive_50","draw_50",
                   "update_100","collision_100","collision_naive_100","draw_100",
                   "update_250","collision_250","collision_naive_250","draw_250",
                   "update_500","collision_500","collision_naive_500","draw_500",
                   "update_1000","collision_1000","collision_naive_1000","draw_1000",
                   "projectiles_100","projectiles_500","projectiles_1000","projectiles_2000",
                   "spike_500","full_250")
}
$outDir = Join-Path $repo "out/arena/$Label"
New-Item -ItemType Directory -Force $outDir | Out-Null
$head = (git -C $repo rev-parse --short HEAD).Trim()
$dirty = if ((git -C $repo status --porcelain).Count -gt 0) { "+changes" } else { "" }

$variables = @{ ATOM_VSYNC = "0"; ATOM_PRESENT = "immediate" }
$saved = @{}
foreach ($name in $variables.Keys) {
    $saved[$name] = [Environment]::GetEnvironmentVariable($name)
    [Environment]::SetEnvironmentVariable($name, $variables[$name])
}

# A report's "key: value" lines as a hashtable.
function Read-Report([string]$path) {
    $values = @{}
    foreach ($line in Get-Content $path) {
        $i = $line.IndexOf(": ")
        if ($i -gt 0) { $values[$line.Substring(0, $i)] = $line.Substring($i + 2) }
    }
    return $values
}

$rows = @()
$started = Get-Date
try {
    for ($round = 1; $round -le $Rounds; $round++) {
        foreach ($scenario in $Scenarios) {
            $report = Join-Path $outDir "$scenario-r$round.txt"
            $arguments = @("--benchmark", "--scenario", $scenario, "--seed", $Seed, "--frames", $Frames,
                           "--warmup", $Warmup, "--out", "`"$report`"") # quoted: the path may have spaces
            if ($Trace) { [Environment]::SetEnvironmentVariable("ATOM_TRACE", (Join-Path $outDir "$scenario-r$round")) }
            $process = Start-Process -FilePath $exePath -ArgumentList $arguments -PassThru -WindowStyle Normal `
                -RedirectStandardOutput (Join-Path $outDir "last_stdout.txt") -RedirectStandardError (Join-Path $outDir "last_stderr.txt")
            if (-not $process.WaitForExit([int]($TimeoutSeconds * 1000))) {
                $process.Kill()
                Write-Host ("  round {0} {1,-22} HUNG (killed after {2} s)" -f $round, $scenario, $TimeoutSeconds) -ForegroundColor Red
                continue
            }
            if (-not (Test-Path $report)) {
                Write-Host ("  round {0} {1,-22} no report (exit {2})" -f $round, $scenario, $process.ExitCode) -ForegroundColor Red
                continue
            }
            $r = Read-Report $report
            $rows += [pscustomobject]@{
                label = $Label; round = $round; scenario = $scenario; commit = "$head$dirty"
                profiling = $r["profiling"]; connected_frames = [int]$r["profiling_connected_frames"]
                present_mode = $r["present_mode"]; frames = [int]$r["frames"]
                frame_median = [double]$r["frame_median_ms"]; frame_p95 = [double]$r["frame_p95_ms"]
                frame_p99 = [double]$r["frame_p99_ms"]; frame_worst = [double]$r["frame_worst_ms"]
                update_median = [double]$r["update_median_ms"]; sim_median = [double]$r["sim_median_ms"]
                sim_p95 = [double]$r["sim_p95_ms"]; submit_median = [double]$r["submit_median_ms"]
                cpu_median = [double]$r["cpu_median_ms"]; gpu_wait_median = [double]$r["gpu_wait_median_ms"] # M99
                spike_update_median = if ($r.ContainsKey("spike_update_median_ms")) { [double]$r["spike_update_median_ms"] } else { $null }
                contacts_mean = [double]$r["contacts_mean"]; state_hash = $r["state_hash"]
            }
            Write-Host ("  round {0} {1,-22} frame {2,7:F3} ms   sim {3,7:F3} ms   submit {4,7:F3} ms" -f `
                $round, $scenario, $rows[-1].frame_median, $rows[-1].sim_median, $rows[-1].submit_median)
        }
    }
} finally {
    foreach ($name in $saved.Keys) { [Environment]::SetEnvironmentVariable($name, $saved[$name]) }
    if ($Trace) { [Environment]::SetEnvironmentVariable("ATOM_TRACE", $null) }
}

$csv = Join-Path $repo "out/arena/$Label.csv"
$rows | Export-Csv -NoTypeInformation -Path $csv

function Median([double[]]$values) {
    $sorted = $values | Sort-Object
    $n = $sorted.Count
    if ($n -eq 0) { return [double]::NaN }
    if ($n % 2) { return $sorted[[int][Math]::Floor($n / 2)] }
    return ($sorted[$n / 2 - 1] + $sorted[$n / 2]) / 2
}

Write-Host ""
Write-Host ("{0} - commit {1}{2}, {3}, {4} rounds x {5} frames (+{6} warm-up), seed {7}, {8:F1} min" -f `
    $Label, $head, $dirty, $power, $Rounds, $Frames, $Warmup, $Seed, ((Get-Date) - $started).TotalMinutes)
Write-Host ("{0,-22} {1,10} {2,18} {3,10} {4,10} {5,10} {6,10} {7,10} {8,8}" -f "scenario", "frame ms", "(range)", "p99 ms",
    "cpu ms", "gpu wait", "sim ms", "submit ms", "hash ok")
foreach ($scenario in $Scenarios) {
    $set = @($rows | Where-Object { $_.scenario -eq $scenario })
    if ($set.Count -eq 0) { continue }
    $frame = @($set | ForEach-Object { $_.frame_median })
    $hashes = @($set | ForEach-Object { $_.state_hash } | Sort-Object -Unique)
    Write-Host ("{0,-22} {1,10:F3} {2,18} {3,10:F3} {4,10:F3} {5,10:F3} {6,10:F3} {7,10:F3} {8,8}" -f $scenario, (Median $frame),
        ("{0:F3}..{1:F3}" -f ($frame | Measure-Object -Minimum).Minimum, ($frame | Measure-Object -Maximum).Maximum),
        (Median @($set | ForEach-Object { $_.frame_p99 })), (Median @($set | ForEach-Object { $_.cpu_median })),
        (Median @($set | ForEach-Object { $_.gpu_wait_median })), (Median @($set | ForEach-Object { $_.sim_median })),
        (Median @($set | ForEach-Object { $_.submit_median })), $(if ($hashes.Count -eq 1) { "yes" } else { "NO" }))
}
Write-Host "Rows: $csv"
