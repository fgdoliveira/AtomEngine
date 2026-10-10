# Is a staged distribution complete, and nothing more? (M68; M76: any game)
#
#   pwsh Tools/Dist/verify.ps1 [-Path Dist/Demo]
#
# Reads what the package must hold from its .payload file, which the game's
# CMake install rules write from the same lists that define the package -
# so there is no second, hand-kept manifest. It names the game's executable.
# Checks:
#   - present: the executable, SDL3.dll, the licences and README, every
#     shipped asset folder, every compiled shader;
#   - absent: development-only folders and files (authoring sources,
#     schemas, test scripts, debug symbols, build metadata, logs);
#   - the executable and SDL3.dll load only Windows DLLs and SDL3.dll (no
#     C++ runtime DLLs: M66);
#   - every shader is signed (a DXIL container whose digest isn't zero),
#     or retail drivers refuse it.
# Exit 0 if all pass; 1 otherwise, with every failure listed.
param([string]$Path = "Dist/Demo")
$ErrorActionPreference = "Stop"
. "$PSScriptRoot/../Dev/common.ps1"

$failures = [System.Collections.Generic.List[string]]::new()
function Fail([string]$what) { $failures.Add($what); Write-Host "  FAIL $what" -ForegroundColor Red }
function Pass([string]$what) { Write-Host "  ok   $what" }

if (-not (Test-Path $Path)) { throw "No staged package at $Path" }
$root = (Resolve-Path $Path).Path
$payloadFile = Join-Path $root ".payload"
if (-not (Test-Path $payloadFile)) { throw "$payloadFile is missing: stage with cmake --install (Tools/Dist/package.ps1)" }
$payload = @{}
foreach ($line in Get-Content $payloadFile) {
    if ($line -match "^(\w+)=(.*)$") { $payload[$Matches[1]] = $Matches[2] }
}
$shipped = @($payload.shipped -split "," | Where-Object { $_ })
$devOnly = @($payload.devonly -split "," | Where-Object { $_ })
if (-not $payload.exe) { throw "$payloadFile names no executable (exe=...)" }

Write-Host "== present ($($payload.game))"
foreach ($file in $payload.exe, "SDL3.dll", "LICENSE.txt", "THIRD_PARTY_NOTICES.txt", "README.txt") {
    if (Test-Path (Join-Path $root $file)) { Pass $file } else { Fail "missing $file" }
}
foreach ($folder in $shipped) {
    $dir = Join-Path $root "Assets/$folder"
    if ((Test-Path $dir) -and (Get-ChildItem $dir -Recurse -File).Count -gt 0) { Pass "Assets/$folder" }
    else { Fail "missing or empty Assets/$folder" }
}
$shaders = @(Get-ChildItem (Join-Path $root "shaders") -Filter *.dxil -ErrorAction SilentlyContinue)
if ($shaders.Count -eq [int]$payload.shaders) { Pass "$($shaders.Count) shaders" }
else { Fail "shaders: $($shaders.Count) found, $($payload.shaders) expected" }

Write-Host "== absent"
$extras = @(Get-ChildItem $root -Recurse -Force | Where-Object {
    $relative = $_.FullName.Substring($root.Length + 1)
    $_.Name -match '\.(blend|blend1|py|pyc|pdb|ilk|atomtest|hlsl|hlsli|cpp|h|log|cmake)$' -or
    $_.Name -match '^(CMakeLists\.txt|CMakeCache\.txt|\.git.*)$' -or
    $_.Name -like 'character_lab*' -or
    $relative -match '^(Assets\\Schemas|out|Testing)(\\|$)' -or
    ($devOnly | Where-Object { $relative -match "^Assets\\$_(\\|$)" })
})
if ($extras.Count -eq 0) { Pass "no development material" }
else { foreach ($extra in $extras) { Fail "should not ship: $($extra.FullName.Substring($root.Length + 1))" } }

Write-Host "== dependencies"
$vs = Get-VisualStudioPath
$dumpbin = if ($vs) { Get-ChildItem "$vs\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe" -ErrorAction SilentlyContinue | Select-Object -Last 1 } else { $null }
if (-not $dumpbin) { Fail "dumpbin not found (Visual Studio C++ tools): dependencies unchecked" }
else {
    # Windows system DLLs and API sets that every Windows 10/11 has.
    $system = 'kernel32|user32|gdi32|shell32|imm32|winmm|ole32|oleaut32|version|advapi32|setupapi|dxgi|d3d12|ws2_32|cfgmgr32|hid|dwmapi|uxtheme|shcore|comdlg32'
    foreach ($binary in $payload.exe, "SDL3.dll") {
        $dlls = & $dumpbin.FullName /nologo /dependents (Join-Path $root $binary) |
            ForEach-Object { $_.Trim() } | Where-Object { $_ -match '^[\w.-]+\.dll$' } # names only, not the "Dump of file" header
        $unexpected = @($dlls | Where-Object { $_ -notmatch "^($system|SDL3)\.dll$" })
        if ($unexpected.Count -eq 0) { Pass "$binary loads only Windows and SDL3 ($($dlls.Count) DLLs)" }
        else { Fail "$binary needs $($unexpected -join ', ') (a machine dependency: M66)" }
    }
}

Write-Host "== shaders signed"
$unsigned = @($shaders | Where-Object {
    $bytes = [System.IO.File]::ReadAllBytes($_.FullName)
    $magic = [System.Text.Encoding]::ASCII.GetString($bytes, 0, 4)
    $magic -ne "DXBC" -or -not ($bytes[4..19] | Where-Object { $_ -ne 0 })
})
if ($unsigned.Count -eq 0) { Pass "all $($shaders.Count) DXIL containers carry a digest" }
else { foreach ($shader in $unsigned) { Fail "unsigned or not DXIL: shaders/$($shader.Name)" } }

if ($failures.Count -gt 0) {
    Write-Host ("FAILED: {0} problem(s) in {1}" -f $failures.Count, $root) -ForegroundColor Red
    exit 1
}
Write-Host "PASS: $root" -ForegroundColor Green
exit 0
