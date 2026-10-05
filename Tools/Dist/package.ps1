# Make the Windows distribution (M68): one command, from source to ZIP.
#
#   pwsh Tools/Dist/package.ps1 [-NoSmoke] [-BuildDir build-dist] [-OutDir Dist]
#
#   1. builds AtomGame in Release in its own folder (build-dist/), with
#      -DATOM_DISTRIBUTION=ON: the windowed program players get (M67). The
#      development build (build/) is left alone;
#   2. installs it into a fresh staging folder, Dist/AtomGame/, with the
#      CMake install rules (Game/CMakeLists.txt: the one definition of the
#      package);
#   3. verifies the staged files (verify.ps1);
#   4. unless -NoSmoke (a machine without a GPU, CI): copies the package to
#      a temporary folder and starts it from another working directory -
#      `--no-settings --diagnostics` must report, so nothing depends on
#      the repository or the current directory;
#   5. zips it as Dist/AtomGame-v<version>-win64.zip, every entry stamped
#      with the commit's time. The same revision gives the same payload
#      (every file byte for byte); the ZIP container itself isn't
#      bit-identical between runs (the writer's own metadata);
#   6. prints the measurements.
param(
    [switch]$NoSmoke,
    [string]$BuildDir = "build-dist",
    [string]$OutDir = "Dist"
)
$ErrorActionPreference = "Stop"
$started = Get-Date
$root = Resolve-Path (Join-Path $PSScriptRoot "../..")
Set-Location $root
. "$PSScriptRoot/../Dev/common.ps1"
$cmake = Find-DevTool "cmake"
if (-not $cmake) { throw "cmake not found: run Tools/Dev/doctor.ps1" }

function Step([string]$what, [scriptblock]$action) {
    Write-Host "== $what"
    & $action
    if ($LASTEXITCODE -ne 0) { Write-Host "FAILED: $what (exit $LASTEXITCODE)" -ForegroundColor Red; exit 1 }
}

Step "configure $BuildDir (distribution, Release)" {
    & $cmake -B $BuildDir -S . -DATOM_DISTRIBUTION=ON -DATOM_BUILD_TESTS=OFF | Select-Object -Last 1
}
Step "build AtomGame" { & $cmake --build $BuildDir --config Release --target AtomGame | Select-Object -Last 1 }

$stage = Join-Path $OutDir "AtomGame"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
Step "stage $stage" { & $cmake --install $BuildDir --config Release --prefix $stage | Out-Null }
Step "verify" { pwsh -NoProfile -File "$PSScriptRoot/verify.ps1" -Path $stage }

$payload = @{}
foreach ($line in Get-Content (Join-Path $stage ".payload")) { if ($line -match "^(\w+)=(.*)$") { $payload[$Matches[1]] = $Matches[2] } }
Remove-Item (Join-Path $stage ".payload") # the check's input, not part of the game

if (-not $NoSmoke) {
    Write-Host "== smoke: run from outside the repository"
    $smoke = Join-Path ([System.IO.Path]::GetTempPath()) "AtomGameDist"
    if (Test-Path $smoke) { Remove-Item $smoke -Recurse -Force }
    Copy-Item $stage $smoke -Recurse
    $report = Join-Path $smoke "report.txt"
    $out = Join-Path ([System.IO.Path]::GetTempPath()) "AtomGameDist.out.txt"
    # Started with another working directory; a windowed program, so its
    # output is captured through redirected handles (RunLog keeps them).
    $process = Start-Process (Join-Path $smoke "AtomGame.exe") -ArgumentList "--no-settings", "--diagnostics", "`"$report`"" `
        -WorkingDirectory ([System.IO.Path]::GetTempPath()) -RedirectStandardOutput $out -PassThru -Wait
    if ($process.ExitCode -ne 0 -or -not (Test-Path $report)) {
        Write-Host "FAILED: the staged game didn't start outside the repository (exit $($process.ExitCode)); see $out" -ForegroundColor Red
        exit 1
    }
    Write-Host ("   ok: {0}" -f ((Get-Content $report | Select-String "gpu.adapter:").Line))
    Remove-Item $smoke -Recurse -Force
}

$zip = Join-Path $OutDir "AtomGame-v$($payload.version)-win64.zip"
if (Test-Path $zip) { Remove-Item $zip }
$commitTime = (git log -1 --date=rfc --format=%cd).Trim() # the form cmake -E tar --mtime parses
Step "zip $zip" {
    Push-Location $OutDir
    try { & $cmake -E tar cf (Split-Path $zip -Leaf) --format=zip "--mtime=$commitTime" AtomGame }
    finally { Pop-Location }
}

# Measurements: descriptive, not targets.
$files = @(Get-ChildItem $stage -Recurse -File)
$mb = { param($bytes) "{0:N1} MB" -f ($bytes / 1MB) }
Write-Host ""
Write-Host "Package  $((Resolve-Path $zip).Path)"
Write-Host ("  AtomGame.exe       {0}" -f (& $mb (Get-Item (Join-Path $stage "AtomGame.exe")).Length))
Write-Host ("  uncompressed       {0} in {1} files ({2} DLL)" -f (& $mb ($files | Measure-Object Length -Sum).Sum), $files.Count,
    @($files | Where-Object Extension -eq ".dll").Count)
Write-Host ("  zip                {0}" -f (& $mb (Get-Item $zip).Length))
Write-Host ("  took               {0:N0} s" -f ((Get-Date) - $started).TotalSeconds)
