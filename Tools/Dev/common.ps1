# Shared by the Tools/Dev scripts (M62). Dot-source it: . "$PSScriptRoot/common.ps1"

# cmake, ctest, ...: on PATH, or the copies inside Visual Studio. $null if
# neither has it.
function Find-DevTool([string]$name) {
    $onPath = Get-Command $name -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    $vs = Get-VisualStudioPath
    if ($vs) {
        $candidate = Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\$name.exe"
        if (Test-Path $candidate) { return $candidate }
    }
    return $null
}

# The latest Visual Studio with C++ tools, or $null.
function Get-VisualStudioPath {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { return $null }
    $path = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($path) { return $path }
    return $null
}

# dxc: the Windows SDK's (newest first) or the Vulkan SDK's, as Shaders/CMakeLists.txt looks.
function Find-Dxc {
    $onPath = Get-Command dxc -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    $kits = "${env:ProgramFiles(x86)}\Windows Kits\10\bin"
    if (Test-Path $kits) {
        $found = Get-ChildItem $kits -Directory -Filter "10.*" | Sort-Object Name -Descending |
            ForEach-Object { Join-Path $_.FullName "x64\dxc.exe" } | Where-Object { Test-Path $_ } | Select-Object -First 1
        if ($found) { return $found }
    }
    if ($env:VULKAN_SDK -and (Test-Path "$env:VULKAN_SDK\Bin\dxc.exe")) { return "$env:VULKAN_SDK\Bin\dxc.exe" }
    return $null
}

# "plugged in", "battery", or "unknown" (desktops report no battery).
function Get-PowerSource {
    $battery = Get-CimInstance Win32_Battery -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $battery) { return "mains (no battery)" }
    # BatteryStatus 1 = discharging; 2 = on AC; others are charging states.
    if ($battery.BatteryStatus -eq 1) { return "battery" }
    return "plugged in"
}
