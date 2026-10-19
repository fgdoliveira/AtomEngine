# The source SDK (v0.0.20, M131): one command from the repository to
# Dist/AtomSDK-v<version>-src.zip.
#
#   pwsh Tools/Dist/sdk.ps1 [-OutDir Dist] [-NoConfigure]
#
#   1. stages Dist/AtomSDK-v<version>/ from what git tracks (the repository
#      and its submodules) - never the working folder's build output or
#      untracked files - keeping only the SDK's folders, and pruning each
#      dependency of its tests, docs and samples;
#   2. writes SDK-README.txt;
#   3. verifies it (verify-sdk.ps1: required files and licences, nothing
#      development-only, and - unless -NoConfigure - that it configures on
#      its own in a fresh folder);
#   4. zips it, every entry stamped with the commit's time.
param(
    [string]$OutDir = "Dist",
    [switch]$NoConfigure
)
$ErrorActionPreference = "Stop"
$started = Get-Date
$root = Resolve-Path (Join-Path $PSScriptRoot "../..")
Set-Location $root
. "$PSScriptRoot/../Dev/common.ps1"
$cmake = Find-DevTool "cmake"
if (-not $cmake) { throw "cmake not found: run Tools/Dev/doctor.ps1" }

$version = if ((Get-Content CMakeLists.txt -Raw) -match "VERSION\s+(\d+\.\d+\.\d+)") { $Matches[1] } else { throw "no version in CMakeLists.txt" }
$name = "AtomSDK-v$version"

# What the SDK is: the engine and what builds with it. Paths relative to
# the repository, forward slashes; a folder ends with '/'.
$include = @(
    "CMakeLists.txt", "LICENSE.txt", "README.md", "CHANGELOG.md",
    "Engine/", "Framework/", "Authoring/", "Project/", "WorkspaceUI/", "Shaders/", "cmake/", "Content/",
    "Tools/Workspace/", "Tools/ProjectTool/", "Tools/LevelCheck/",
    "Apps/Samples/HelloAtom/", "docs/"
)
# Development-only, even inside an included folder.
$exclude = @('\.blend$', '(^|/)\.github/')

# The dependencies (submodules): everything they track, but their tests,
# docs, samples and IDE projects - nothing the build reads.
$prune = @{
    "SDL"     = @("test/", "examples/", "docs/", "android-project/", "VisualC/", "VisualC-GDK/", "Xcode/")
    "glm"     = @("test/", "doc/", "util/")
    "json"    = @("tests/", "docs/", "tools/")
    "cgltf"   = @("test/", "fuzz/")
    "stb"     = @("tests/", "tools/", "data/", "docs/", "deprecated/", "stb_image_resize_test/")
    "imgui"   = @("examples/", "docs/", "misc/")
    "doctest" = @("examples/", "scripts/", "doc/", "tests/")
}

function Step([string]$what, [scriptblock]$action) {
    Write-Host "== $what"
    & $action
    if ($LASTEXITCODE -ne 0) { Write-Host "FAILED: $what (exit $LASTEXITCODE)" -ForegroundColor Red; exit 1 }
}

$stage = Join-Path $OutDir $name
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null

$files = [System.Collections.Generic.List[string]]::new()
foreach ($file in git ls-files) {
    $kept = $include | Where-Object { if ($_.EndsWith("/")) { $file.StartsWith($_) } else { $file -eq $_ } }
    if ($kept -and -not ($exclude | Where-Object { $file -match $_ })) { $files.Add($file) }
}
foreach ($dependency in $prune.Keys) {
    foreach ($file in git -C "external/$dependency" ls-files) {
        $dropped = ($prune[$dependency] | Where-Object { $file.StartsWith($_) }) -or ($exclude | Where-Object { $file -match $_ })
        if (-not $dropped) { $files.Add("external/$dependency/$file") }
    }
}

Step "stage $stage ($($files.Count) files)" {
    foreach ($file in $files) {
        $target = Join-Path $stage $file
        $folder = Split-Path $target -Parent
        if (-not (Test-Path -LiteralPath $folder)) { New-Item -ItemType Directory -Force $folder | Out-Null }
        Copy-Item -LiteralPath $file -Destination $target
    }
    (Get-Content "$PSScriptRoot/SDK-README.txt.in" -Raw).Replace("@VERSION@", $version) |
        Set-Content -NoNewline (Join-Path $stage "SDK-README.txt")
    $global:LASTEXITCODE = 0
}

Step "verify" {
    $arguments = @("-NoProfile", "-File", "$PSScriptRoot/verify-sdk.ps1", "-Path", $stage)
    if ($NoConfigure) { $arguments += "-NoConfigure" }
    pwsh @arguments
}

$zip = Join-Path $OutDir "$name-src.zip"
if (Test-Path $zip) { Remove-Item $zip }
$commitTime = (git log -1 --date=rfc --format=%cd).Trim() # the form cmake -E tar --mtime parses
Step "zip $zip" {
    Push-Location $OutDir
    try { & $cmake -E tar cf (Split-Path $zip -Leaf) --format=zip "--mtime=$commitTime" $name }
    finally { Pop-Location }
}

# Measurements: descriptive, not targets.
$staged = @(Get-ChildItem $stage -Recurse -File)
$mb = { param($bytes) "{0:N1} MB" -f ($bytes / 1MB) }
Write-Host ""
Write-Host "SDK  $((Resolve-Path $zip).Path)"
Write-Host ("  uncompressed       {0} in {1} files" -f (& $mb ($staged | Measure-Object Length -Sum).Sum), $staged.Count)
Write-Host ("  zip                {0}" -f (& $mb (Get-Item $zip).Length))
Write-Host ("  took               {0:N0} s" -f ((Get-Date) - $started).TotalSeconds)
