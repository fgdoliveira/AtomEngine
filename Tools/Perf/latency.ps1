# Input-to-display latency (M72): how long from a click to the frame on
# screen, for each configuration, measured with Intel PresentMon.
#
#   pwsh Tools/Perf/latency.ps1 [-Configs fif2-vsync,fif3-vsync,...] [-Rounds 4] [-Seconds 10] [-Level street]
#
# Configurations (any list, repeats allowed - the same twice is the trust
# check):
#   fif2-vsync, fif3-vsync           ATOM_FRAMES_IN_FLIGHT 2 / 3, vsync (the default mode)
#   fif2-immediate, fif3-immediate   the same, uncapped (ATOM_VSYNC=0 ATOM_PRESENT=immediate)
#
# Each run starts the game in the level (an idle script, ATOM_LATENCY_FLASH
# on: each click turns that frame black), records it with PresentMon, and
# after a warm-up injects a left click every 120-250 ms (random, so clicks
# don't lock to the display's refresh) at the window's centre. The runs
# alternate configurations, forward then backward each round (ABBA), so
# drift lands on all of them alike.
#
# The metric is PresentMon's MsAllInputToPhotonLatency: from Windows
# receiving the input to the first frame displayed after it. Injected clicks
# don't reach its click-only column (it stays NA), and the all-input column
# counts *any* input - so don't touch the mouse or keyboard while it runs.
# It's a software measurement: no mouse hardware, no panel response time.
# And PresentMon credits an input to the next present after Windows saw
# it, which isn't always the frame that read it (see ATOM_LATENCY_LOG, M73).
#
# Needs administrator rights (PresentMon's event tracing): if not elevated
# it relaunches itself elevated once (one UAC prompt) and shows its output
# when done. Plugged in, on the laptop's own screen. Never a ctest gate.
param(
    [string[]]$Configs = @("fif2-vsync", "fif3-vsync"),
    [int]$Rounds = 4,
    [double]$Seconds = 10,
    [string]$Level = "street",
    [string]$Game = "build/bin/Release/AtomGame.exe",
    [string]$PresentMon = "C:\Program Files\Intel\PresentMon\PresentMonConsoleApplication\PresentMon-2.6.0-x64.exe",
    [switch]$EngineLog, # M73: also ATOM_LATENCY_LOG=1, and the engine's own stages per configuration
    [string]$Transcript = ""
)
$ErrorActionPreference = "Stop"
[System.Threading.Thread]::CurrentThread.CurrentCulture = [System.Globalization.CultureInfo]::InvariantCulture
$root = Resolve-Path (Join-Path $PSScriptRoot "../..")
Set-Location $root
# "-Configs a,b" arrives as one string through `pwsh -File`: split it.
$Configs = @($Configs | ForEach-Object { $_ -split "," } | ForEach-Object { $_.Trim() } | Where-Object { $_ })

$known = @{
    "fif2-vsync"     = @{ ATOM_FRAMES_IN_FLIGHT = "2" }
    "fif3-vsync"     = @{ ATOM_FRAMES_IN_FLIGHT = "3" }
    "fif2-immediate" = @{ ATOM_FRAMES_IN_FLIGHT = "2"; ATOM_VSYNC = "0"; ATOM_PRESENT = "immediate" }
    "fif3-immediate" = @{ ATOM_FRAMES_IN_FLIGHT = "3"; ATOM_VSYNC = "0"; ATOM_PRESENT = "immediate" }
}
foreach ($config in $Configs) { if (-not $known.ContainsKey($config)) { throw "Unknown configuration '$config' (known: $($known.Keys -join ', '))" } }
if (-not (Test-Path $Game)) { throw "No game at $Game (build Release first)" }
if (-not (Test-Path $PresentMon)) { throw "PresentMon console not found at $PresentMon (-PresentMon <path>)" }

# Elevate once; the elevated copy writes a transcript this one prints.
$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $admin) {
    $log = Join-Path ([System.IO.Path]::GetTempPath()) "atom_latency_transcript.txt"
    if (Test-Path $log) { Remove-Item $log }
    $arguments = @("-NoProfile", "-File", "`"$PSCommandPath`"", "-Configs", ($Configs -join ","), "-Rounds", $Rounds,
        "-Seconds", $Seconds, "-Level", $Level, "-Game", "`"$((Resolve-Path $Game).Path)`"",
        "-PresentMon", "`"$PresentMon`"", "-Transcript", "`"$log`"")
    if ($EngineLog) { $arguments += "-EngineLog" }
    Write-Host "Asking for administrator rights (PresentMon needs them)..."
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
$variables = @("ATOM_TEST_SCRIPT", "ATOM_START_LEVEL", "ATOM_LATENCY_FLASH", "ATOM_LATENCY_LOG", "ATOM_FRAMES_IN_FLIGHT", "ATOM_VSYNC", "ATOM_PRESENT")
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

# One run: returns its latency samples and displayed-frame intervals, or $null.
function Invoke-Run([string]$config) {
    foreach ($name in "ATOM_FRAMES_IN_FLIGHT", "ATOM_VSYNC", "ATOM_PRESENT") { [Environment]::SetEnvironmentVariable($name, $null) }
    foreach ($entry in $known[$config].GetEnumerator()) { [Environment]::SetEnvironmentVariable($entry.Key, $entry.Value) }
    [Environment]::SetEnvironmentVariable("ATOM_TEST_SCRIPT", $script)
    [Environment]::SetEnvironmentVariable("ATOM_START_LEVEL", $Level)
    [Environment]::SetEnvironmentVariable("ATOM_LATENCY_FLASH", "1")
    [Environment]::SetEnvironmentVariable("ATOM_LATENCY_LOG", $(if ($EngineLog) { "1" } else { $null }))

    $csv = Join-Path $temp "atom_latency_run.csv"
    if (Test-Path $csv) { Remove-Item $csv }
    $gameOut = Join-Path $temp "atom_latency_game.txt"
    $game = Start-Process $Game -ArgumentList "--no-settings" -PassThru -WindowStyle Normal -RedirectStandardOutput $gameOut
    Start-Sleep -Seconds $warmup
    $pm = Start-Process $PresentMon -ArgumentList "--process_name AtomGame.exe --output_file `"$csv`" --timed $Seconds --terminate_after_timed --no_console_stats --stop_existing_session" -PassThru -WindowStyle Hidden
    Start-Sleep -Milliseconds 500

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
    $limit = $warmup + $Seconds + 30
    if (-not $game.WaitForExit([int](($limit - $warmup - $Seconds) * 1000))) {
        Stop-Process -Id $game.Id -Force -ErrorAction SilentlyContinue
        Write-Host "  hung: killed" -ForegroundColor Yellow
    }
    if (-not $pm.WaitForExit(15000)) { Stop-Process -Id $pm.Id -Force -ErrorAction SilentlyContinue }
    if (-not (Test-Path $csv)) { Write-Host "  no PresentMon CSV" -ForegroundColor Yellow; return $null }

    $rows = Import-Csv $csv
    $column = "MsAllInputToPhotonLatency"
    if (-not ($rows[0].PSObject.Properties.Name -contains $column)) { throw "PresentMon CSV has no $column column" }
    $latency = [double[]]@($rows | ForEach-Object { $_.$column } | Where-Object { $_ -and $_ -ne "NA" } | ForEach-Object { [double]$_ })
    $display = [double[]]@($rows | ForEach-Object { $_.MsBetweenDisplayChange } | Where-Object { $_ -and $_ -ne "NA" } | ForEach-Object { [double]$_ } | Where-Object { $_ -gt 0 })
    # The engine's LAT blocks (M73), when asked for: each stage's block medians.
    $stages = @{}
    if ($EngineLog -and (Test-Path $gameOut)) {
        foreach ($match in [regex]::Matches((Get-Content $gameOut -Raw),
                'LAT block \d+ samples \d+ input_to_frame ([\d.]+) p95 [\d.]+ wait ([\d.]+) p95 [\d.]+ input_to_submit ([\d.]+) p95 [\d.]+ input_to_gpu ([\d.]+)')) {
            foreach ($stage in @(@("toFrame", 1), @("wait", 2), @("toSubmit", 3), @("toGpu", 4))) {
                if (-not $stages.ContainsKey($stage[0])) { $stages[$stage[0]] = [System.Collections.Generic.List[double]]::new() }
                $stages[$stage[0]].Add([double]$match.Groups[$stage[1]].Value)
            }
        }
    }
    return [pscustomobject]@{ Clicks = $clicks; Latency = $latency; Display = $display; Stages = $stages }
}

Write-Host ("Latency: {0}, {1} rounds of {2} s, level {3}. Hands off the mouse and keyboard." -f ($Configs -join " / "), $Rounds, $Seconds, $Level)
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
            Write-Host ("round {0}  {1,-16} clicks {2,3}  samples {3,3}  latency median {4,6:N2} ms  display {5,5:N2} ms" -f $round, $Configs[$i],
                $run.Clicks, $run.Latency.Count, (Get-Median $run.Latency), (Get-Median $run.Display))
        }
    }
}
finally {
    foreach ($name in $variables) { [Environment]::SetEnvironmentVariable($name, $saved[$name]) }
}

Write-Host ""
Write-Host "Configuration     samples  latency median   p95    display interval (fps)"
for ($i = 0; $i -lt $Configs.Count; $i++) {
    $latency = [double[]]@($results[$i] | ForEach-Object { $_.Latency })
    $display = [double[]]@($results[$i] | ForEach-Object { $_.Display })
    $interval = Get-Median $display
    Write-Host ("{0,-16} {1,7}  {2,8:N2} ms  {3,6:N2} ms  {4,6:N2} ms ({5:N0} fps)" -f $Configs[$i], $latency.Count,
        (Get-Median $latency), (Get-P95 $latency), $interval, (1000 / $interval))
}
if ($EngineLog) {
    # Medians of the engine's block medians: where the time goes before the
    # GPU is done. Not subtracted from PresentMon's (different clocks, and
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
    if ($deltas.Count) {
        $sorted = $deltas | Sort-Object
        Write-Host ("{0} - {1}: median paired delta {2:+0.00;-0.00} ms over {3} rounds, range {4:+0.00;-0.00}..{5:+0.00;-0.00}" -f
            $Configs[$i], $Configs[0], (Get-Median $deltas), $deltas.Count, $sorted[0], $sorted[-1])
    }
}
if ($Transcript) { Stop-Transcript | Out-Null }
