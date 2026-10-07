# Input latency (M72-M74): how long a click takes, per configuration.
#
#   pwsh Tools/Perf/latency.ps1 [-Mode Engine|PresentMon] [-Configs default,fif3-vsync,...]
#                               [-Rounds 4] [-Seconds 10] [-Level street]
#                               [-EngineLog] [-PresentMonPath <exe>] [-NoElevate]
#                               [-Game build/bin/Release/Drift.exe]   (M83: DRIFT, by its autopilot)
#
# Two modes, by design:
#   Engine (default)  the engine's own timing (ATOM_LATENCY_LOG, M73): from the
#                     click (the OS's timestamp) to the frame that reads it,
#                     that frame's swapchain wait, its submit, its GPU
#                     completion. Needs no rights at all: use it every day and
#                     in automation. It doesn't see the compositor or display.
#   PresentMon        end to end, from Windows receiving the click to the frame
#                     on screen (Intel PresentMon's MsAllInputToPhotonLatency).
#                     An explicit developer operation: before a milestone's PR,
#                     or after a change to presentation, display or frame
#                     pacing. -EngineLog adds the engine's stages.
#
# PresentMon starts realtime event tracing (ETW), which needs an administrator
# or a member of the built-in Performance Log Users group (SID S-1-5-32-559).
# The script checks; if neither, it explains and asks for elevation for this
# run only, or with -NoElevate exits instead (no dialog: for unattended use).
# Joining the group is an optional, manual, one-time step (README, Measuring
# performance). This script never changes group membership, and never
# downloads or installs PresentMon: install it from Intel / GameTechDev and
# pass -PresentMonPath, set ATOM_PRESENTMON, or use the default install path.
#
# Configurations (any list, repeats allowed - the same twice is the trust
# check):
#   default                          nothing set: the build's own defaults
#   fif2-vsync, fif3-vsync           ATOM_FRAMES_IN_FLIGHT 2 / 3, vsync, the wait late (v0.0.11's order)
#   fif2-immediate, fif3-immediate   the same, uncapped (ATOM_VSYNC=0 ATOM_PRESENT=immediate)
#   fif2-vsync-early, fif3-vsync-early, fif2-immediate-early
#                                    the swapchain wait before input (ATOM_LATENCY_WAIT=early, M74)
#
# Each run starts the game in the level (an idle script; ATOM_LATENCY_FLASH on,
# so each click turns that frame black) and, after a warm-up, injects a left
# click every 120-250 ms (random, so clicks don't lock to the refresh) at the
# window's centre. Runs alternate configurations, forward then backward each
# round (ABBA), so drift lands on all alike. Hands off the mouse and keyboard
# while it runs: PresentMon's metric counts any input. Plugged in, on the
# laptop's own screen. Never a ctest gate.
param(
    [ValidateSet("Engine", "PresentMon")] [string]$Mode = "Engine",
    [string[]]$Configs = @("default", "fif3-vsync"),
    [int]$Rounds = 4,
    [double]$Seconds = 10,
    [string]$Level = "street",
    [string]$Game = "build/bin/Release/AtomGame.exe",
    [string]$PresentMonPath = "",
    [switch]$EngineLog,
    [switch]$NoElevate,
    [string]$Transcript = "" # internal: the elevated copy's output
)
$ErrorActionPreference = "Stop"
[System.Threading.Thread]::CurrentThread.CurrentCulture = [System.Globalization.CultureInfo]::InvariantCulture
$root = Resolve-Path (Join-Path $PSScriptRoot "../..")
Set-Location $root
# "-Configs a,b" arrives as one string through `pwsh -File`: split it.
$Configs = @($Configs | ForEach-Object { $_ -split "," } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
$engineStages = $Mode -eq "Engine" -or $EngineLog

$known = @{
    "default"              = @{}
    "fif2-vsync"           = @{ ATOM_FRAMES_IN_FLIGHT = "2"; ATOM_LATENCY_WAIT = "late" }
    "fif3-vsync"           = @{ ATOM_FRAMES_IN_FLIGHT = "3"; ATOM_LATENCY_WAIT = "late" }
    "fif2-immediate"       = @{ ATOM_FRAMES_IN_FLIGHT = "2"; ATOM_VSYNC = "0"; ATOM_PRESENT = "immediate"; ATOM_LATENCY_WAIT = "late" }
    "fif3-immediate"       = @{ ATOM_FRAMES_IN_FLIGHT = "3"; ATOM_VSYNC = "0"; ATOM_PRESENT = "immediate"; ATOM_LATENCY_WAIT = "late" }
    "fif2-vsync-early"     = @{ ATOM_FRAMES_IN_FLIGHT = "2"; ATOM_LATENCY_WAIT = "early" }
    "fif3-vsync-early"     = @{ ATOM_FRAMES_IN_FLIGHT = "3"; ATOM_LATENCY_WAIT = "early" }
    "fif2-immediate-early" = @{ ATOM_FRAMES_IN_FLIGHT = "2"; ATOM_VSYNC = "0"; ATOM_PRESENT = "immediate"; ATOM_LATENCY_WAIT = "early" }
}
foreach ($config in $Configs) { if (-not $known.ContainsKey($config)) { throw "Unknown configuration '$config' (known: $($known.Keys -join ', '))" } }
if (-not (Test-Path $Game)) { throw "No game at $Game (build Release first)" }

if ($Mode -eq "PresentMon") {
    # Find PresentMon - never fetch it.
    if (-not $PresentMonPath) { $PresentMonPath = $env:ATOM_PRESENTMON }
    if (-not $PresentMonPath) {
        $PresentMonPath = Get-ChildItem "$env:ProgramFiles\Intel\PresentMon\PresentMonConsoleApplication\PresentMon-*-x64.exe" -ErrorAction SilentlyContinue |
            Sort-Object { [version]($_.BaseName -replace '^PresentMon-|-x64$', '') } | Select-Object -Last 1 -ExpandProperty FullName
    }
    if (-not $PresentMonPath -or -not (Test-Path $PresentMonPath)) {
        throw "PresentMon's console application wasn't found. Install it from Intel or github.com/GameTechDev/PresentMon, then pass -PresentMonPath <exe> or set ATOM_PRESENTMON. (Or use -Mode Engine, which needs nothing.)"
    }

    # ETW access: an administrator, or a member of Performance Log Users.
    $admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
    # By SID (the group's name and whoami's headers are translated).
    $perfLogUser = [bool]((whoami /groups) -match 'S-1-5-32-559')
    if (-not $admin -and -not $perfLogUser) {
        Write-Host "PresentMon needs realtime event tracing (ETW): an administrator, or a member of the" -ForegroundColor Yellow
        Write-Host "Performance Log Users group (optional one-time setup: README, Measuring performance)." -ForegroundColor Yellow
        if ($NoElevate) {
            Write-Host "-NoElevate: not asking for elevation. Run -Mode Engine, join the group, or run without -NoElevate." -ForegroundColor Yellow
            exit 2
        }
        # Elevate this run only; the elevated copy writes a transcript this one prints.
        $log = Join-Path ([System.IO.Path]::GetTempPath()) "atom_latency_transcript.txt"
        if (Test-Path $log) { Remove-Item $log }
        $arguments = @("-NoProfile", "-File", "`"$PSCommandPath`"", "-Mode", "PresentMon", "-Configs", ($Configs -join ","),
            "-Rounds", $Rounds, "-Seconds", $Seconds, "-Level", $Level, "-Game", "`"$((Resolve-Path $Game).Path)`"",
            "-PresentMonPath", "`"$PresentMonPath`"", "-Transcript", "`"$log`"")
        if ($EngineLog) { $arguments += "-EngineLog" }
        Write-Host "Asking for elevation for this run only..."
        $elevated = Start-Process pwsh -ArgumentList $arguments -Verb RunAs -PassThru -WindowStyle Minimized
        $elevated.WaitForExit()
        if (Test-Path $log) {
            # Our lines only, not the transcript's banner.
            $lines = Get-Content $log
            $first = [array]::FindIndex([string[]]$lines, [Predicate[string]] { param($l) $l.StartsWith("Latency:") })
            if ($first -ge 0) { $lines[$first..($lines.Count - 1)] | Where-Object { $_ -notmatch '^\*+$' -and $_ -notmatch '^(Fin de|End time|Hora de)' } }
            else { $lines }
        }
        exit $elevated.ExitCode
    }
}
if ($Transcript) { Start-Transcript -Path $Transcript -Force | Out-Null }

. "$PSScriptRoot/../Dev/common.ps1"
if ((Get-PowerSource) -eq "battery") {
    Write-Host "WARNING: on battery - timings won't compare with plugged-in runs. Plug in and run again." -ForegroundColor Yellow
}

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class AtomClicker {
    [StructLayout(LayoutKind.Sequential)] struct MOUSEINPUT { public int dx, dy; public uint mouseData, dwFlags, time; public IntPtr extra; }
    [StructLayout(LayoutKind.Sequential)] struct INPUT { public uint type; public MOUSEINPUT mi; }
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] static extern uint SendInput(uint n, INPUT[] inputs, int size);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    // A left click (down, up): 2 events accepted when it worked.
    public static uint Click() {
        var down = new INPUT { type = 0, mi = new MOUSEINPUT { dwFlags = 0x0002 } };
        var up = new INPUT { type = 0, mi = new MOUSEINPUT { dwFlags = 0x0004 } };
        return SendInput(2, new[] { down, up }, Marshal.SizeOf(typeof(INPUT)));
    }
}
"@

$temp = [System.IO.Path]::GetTempPath()
$script = Join-Path $temp "atom_latency.atomtest"
$warmup = 4.0
"wait $($warmup + $Seconds + 2)`nquit`n" | Set-Content -NoNewline -Path $script
$variables = @("ATOM_TEST_SCRIPT", "ATOM_START_LEVEL", "ATOM_LATENCY_FLASH", "ATOM_LATENCY_LOG", "ATOM_LATENCY_WAIT",
    "ATOM_FRAMES_IN_FLIGHT", "ATOM_VSYNC", "ATOM_PRESENT", "ATOM_PERF_LOG", "ATOM_DRIFT_SECONDS", "ATOM_DRIFT_SEED")
# M83: DRIFT (-Game .../Drift.exe) has no scenario scripts or levels: its
# autopilot flies a fixed course and quits after the same span. The
# latency log is the engine's, so the stages are the same; the frame
# interval (the demo's PERF log) and the click flash are the demo's only.
$exeName = Split-Path $Game -Leaf
$isDrift = $exeName -like "Drift*"
$saved = @{}
foreach ($name in $variables) { $saved[$name] = [Environment]::GetEnvironmentVariable($name) }

function Get-Median([double[]]$values) {
    if ($values.Count -eq 0) { return [double]::NaN }
    $sorted = $values | Sort-Object
    return $sorted[[int][math]::Floor($sorted.Count / 2)]
}
function Get-P95([double[]]$values) {
    if ($values.Count -eq 0) { return [double]::NaN }
    $sorted = $values | Sort-Object
    return $sorted[[int][math]::Min($sorted.Count - 1, [math]::Ceiling($sorted.Count * 0.95) - 1)]
}

# One run: its clicks, PresentMon's samples (PresentMon mode), the engine's
# stage block medians, and the frame interval; $null if it produced nothing.
function Invoke-Run([string]$config) {
    foreach ($name in "ATOM_FRAMES_IN_FLIGHT", "ATOM_VSYNC", "ATOM_PRESENT", "ATOM_LATENCY_WAIT") { [Environment]::SetEnvironmentVariable($name, $null) }
    foreach ($entry in $known[$config].GetEnumerator()) { [Environment]::SetEnvironmentVariable($entry.Key, $entry.Value) }
    if ($isDrift) {
        [Environment]::SetEnvironmentVariable("ATOM_DRIFT_SECONDS", "$($warmup + $Seconds + 2)")
        [Environment]::SetEnvironmentVariable("ATOM_DRIFT_SEED", "7")
    } else {
        [Environment]::SetEnvironmentVariable("ATOM_TEST_SCRIPT", $script)
        [Environment]::SetEnvironmentVariable("ATOM_START_LEVEL", $Level)
    }
    [Environment]::SetEnvironmentVariable("ATOM_LATENCY_FLASH", "1")
    [Environment]::SetEnvironmentVariable("ATOM_LATENCY_LOG", $(if ($engineStages) { "1" } else { $null }))
    [Environment]::SetEnvironmentVariable("ATOM_PERF_LOG", $(if ($Mode -eq "Engine") { "1" } else { $null }))

    $csv = Join-Path $temp "atom_latency_run.csv"
    if (Test-Path $csv) { Remove-Item $csv }
    $gameOut = Join-Path $temp "atom_latency_game.txt"
    $game = Start-Process $Game -ArgumentList "--no-settings" -PassThru -WindowStyle Normal -RedirectStandardOutput $gameOut
    Start-Sleep -Seconds $warmup
    $pm = $null
    if ($Mode -eq "PresentMon") {
        $pm = Start-Process $PresentMonPath -ArgumentList "--process_name $exeName --output_file `"$csv`" --timed $Seconds --terminate_after_timed --no_console_stats --stop_existing_session" -PassThru -WindowStyle Hidden
        Start-Sleep -Milliseconds 500
    }

    $game.Refresh()
    $rect = New-Object AtomClicker+RECT
    [AtomClicker]::GetWindowRect($game.MainWindowHandle, [ref]$rect) | Out-Null
    [AtomClicker]::SetForegroundWindow($game.MainWindowHandle) | Out-Null
    [AtomClicker]::SetCursorPos([int](($rect.Left + $rect.Right) / 2), [int](($rect.Top + $rect.Bottom) / 2)) | Out-Null
    $random = [System.Random]::new()
    $clicks = 0
    $until = (Get-Date).AddSeconds($Seconds - 1)
    while ((Get-Date) -lt $until) {
        if ([AtomClicker]::Click() -eq 2) { $clicks++ }
        Start-Sleep -Milliseconds $random.Next(120, 250)
    }

    # Hang guard: the game's own script quits; past the wall-clock limit, kill.
    if (-not $game.WaitForExit(30000)) {
        Stop-Process -Id $game.Id -Force -ErrorAction SilentlyContinue
        Write-Host "  hung: killed" -ForegroundColor Yellow
    }
    if ($pm -and -not $pm.WaitForExit(15000)) { Stop-Process -Id $pm.Id -Force -ErrorAction SilentlyContinue }

    $latency = [double[]]@()
    $display = [double[]]@()
    if ($Mode -eq "PresentMon") {
        if (-not (Test-Path $csv)) { Write-Host "  PresentMon wrote no CSV (permissions, or the process wasn't found)" -ForegroundColor Yellow; return $null }
        $rows = Import-Csv $csv
        $column = "MsAllInputToPhotonLatency"
        if (-not ($rows[0].PSObject.Properties.Name -contains $column)) {
            Write-Host "  PresentMon's CSV has no $column column (a different PresentMon version?)" -ForegroundColor Yellow; return $null
        }
        $latency = [double[]]@($rows | ForEach-Object { $_.$column } | Where-Object { $_ -and $_ -ne "NA" } | ForEach-Object { [double]$_ })
        $display = [double[]]@($rows | ForEach-Object { $_.MsBetweenDisplayChange } | Where-Object { $_ -and $_ -ne "NA" } | ForEach-Object { [double]$_ } | Where-Object { $_ -gt 0 })
    }
    $stages = @{}
    $text = if (Test-Path $gameOut) { Get-Content $gameOut -Raw } else { "" }
    if ($engineStages) {
        foreach ($match in [regex]::Matches($text,
                'LAT block \d+ samples \d+ input_to_frame ([\d.]+) p95 [\d.]+ wait ([\d.]+) p95 [\d.]+ input_to_submit ([\d.]+) p95 [\d.]+ input_to_gpu ([\d.]+)')) {
            foreach ($stage in @(@("toFrame", 1), @("wait", 2), @("toSubmit", 3), @("toGpu", 4))) {
                if (-not $stages.ContainsKey($stage[0])) { $stages[$stage[0]] = [System.Collections.Generic.List[double]]::new() }
                $stages[$stage[0]].Add([double]$match.Groups[$stage[1]].Value)
            }
        }
    }
    if ($Mode -eq "Engine") {
        # The game's own frame interval (PERF blocks) stands in for the display's.
        $display = [double[]]@([regex]::Matches($text, 'PERF block .*?median ([\d.]+)') | ForEach-Object { [double]$_.Groups[1].Value })
        $latency = [double[]]@(if ($stages["toGpu"]) { $stages["toGpu"] })
    }
    return [pscustomobject]@{ Clicks = $clicks; Latency = $latency; Display = $display; Stages = $stages }
}

# PresentMon gives one sample per click; the engine one LAT block per 20
# clicks (its "samples" and p95 are then of block medians).
$metric = if ($Mode -eq "PresentMon") { "click to display (PresentMon)" } else { "click to GPU done (engine, per 20-click block)" }
Write-Host ("Latency: {0}, {1} mode, {2} rounds of {3} s, {4}. Hands off the mouse and keyboard." -f ($Configs -join " / "), $Mode, $Rounds, $Seconds, $(if ($isDrift) { "DRIFT's autopilot (seed 7)" } else { "level $Level" }))
$results = @{}
for ($i = 0; $i -lt $Configs.Count; $i++) { $results[$i] = [System.Collections.Generic.List[object]]::new() }
try {
    for ($round = 1; $round -le $Rounds; $round++) {
        $order = 0..($Configs.Count - 1)
        if ($round % 2 -eq 0) { [array]::Reverse($order) } # ABBA
        foreach ($i in $order) {
            $run = Invoke-Run $Configs[$i]
            if (-not $run) { continue }
            $results[$i].Add($run)
            Write-Host ("round {0}  {1,-20} clicks {2,3}  samples {3,3}  median {4,6:N2} ms  frame {5,5:N2} ms" -f $round, $Configs[$i],
                $run.Clicks, $run.Latency.Count, (Get-Median $run.Latency), (Get-Median $run.Display))
        }
    }
}
finally {
    foreach ($name in $variables) { [Environment]::SetEnvironmentVariable($name, $saved[$name]) }
}

Write-Host ""
Write-Host ("Configuration         samples  {0}: median   p95    frame interval (fps)" -f $metric)
$empty = $true
for ($i = 0; $i -lt $Configs.Count; $i++) {
    $latency = [double[]]@($results[$i] | ForEach-Object { $_.Latency })
    $display = [double[]]@($results[$i] | ForEach-Object { $_.Display })
    if ($latency.Count) { $empty = $false }
    $interval = Get-Median $display
    Write-Host ("{0,-20} {1,7}  {2,8:N2} ms  {3,6:N2} ms  {4,6:N2} ms ({5:N0} fps)" -f $Configs[$i], $latency.Count,
        (Get-Median $latency), (Get-P95 $latency), $interval, (1000 / $interval))
}
if ($empty -and $Mode -eq "PresentMon") {
    # A neutral diagnosis: an empty column has more than one possible cause.
    Write-Host "PresentMon produced no usable input-latency samples. Check input injection (focus, hands off)," -ForegroundColor Yellow
    Write-Host "target selection (process name), PresentMon permissions and the metric's availability in this version." -ForegroundColor Yellow
}
if ($engineStages) {
    # Medians of the engine's block medians: where the time goes before the
    # GPU is done. Never subtracted from PresentMon's (different clocks, and
    # PresentMon credits input to the next present, not the frame that read it).
    Write-Host ""
    Write-Host "Engine (ATOM_LATENCY_LOG)  input->frame   wait   input->submit   input->GPU done"
    for ($i = 0; $i -lt $Configs.Count; $i++) {
        $stage = { param($name) Get-Median ([double[]]@($results[$i] | ForEach-Object { if ($_.Stages[$name]) { $_.Stages[$name] } })) }
        Write-Host ("{0,-24} {1,9:N2} ms {2,6:N2} ms {3,10:N2} ms {4,12:N2} ms" -f $Configs[$i],
            (& $stage "toFrame"), (& $stage "wait"), (& $stage "toSubmit"), (& $stage "toGpu"))
    }
}
# Paired per round against the first configuration: each round's median
# difference, then the median of those (as ab.ps1 reports builds).
for ($i = 1; $i -lt $Configs.Count; $i++) {
    $deltas = [double[]]@(for ($r = 0; $r -lt [math]::Min($results[0].Count, $results[$i].Count); $r++) {
        (Get-Median $results[$i][$r].Latency) - (Get-Median $results[0][$r].Latency)
    })
    $deltas = [double[]]@($deltas | Where-Object { -not [double]::IsNaN($_) })
    if ($deltas.Count) {
        $sorted = $deltas | Sort-Object
        Write-Host ("{0} - {1}: median paired delta {2:+0.00;-0.00} ms over {3} rounds, range {4:+0.00;-0.00}..{5:+0.00;-0.00}" -f
            $Configs[$i], $Configs[0], (Get-Median $deltas), $deltas.Count, $sorted[0], $sorted[-1])
    }
}
if ($Transcript) { Stop-Transcript | Out-Null }
