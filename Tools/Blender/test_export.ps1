# The incremental export's guarantees, checked against the real Blender
# build (v0.0.17, M113). Needs Blender, so CTest registers it only where
# CMake found one (label "blender"); CI has none.
#
#   pwsh Tools/Blender/test_export.ps1
#
# 1. A marker-only export of one level makes no model, bakes nothing and
#    writes nothing new (its sources are unchanged).
# 2. A one-unit export reads and writes only that unit's outputs: no other
#    output's bytes or timestamp change.
# 3. Unchanged sources rewrite nothing ("0 written").
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "../..")
. (Join-Path $root "Tools/Dev/common.ps1")
$blender = Find-Blender
if (-not $blender) { Write-Host "SKIP: no Blender"; exit 0 }

$outputs = @("Content", "Apps/Games/Demo/Assets", "Apps/Showcase/Assets") | ForEach-Object {
    Get-ChildItem (Join-Path $root $_) -Recurse -File -Include *.glb, *.png, *.json
}
function Snapshot {
    $map = @{}
    foreach ($file in $outputs) {
        $item = Get-Item $file.FullName
        $map[$item.FullName] = "{0}|{1}" -f $item.LastWriteTimeUtc.Ticks, (Get-FileHash $item.FullName -Algorithm MD5).Hash
    }
    return $map
}
function Run([string[]]$options) {
    $arguments = @("-b", "--factory-startup", "-P", (Join-Path $root "Tools/Blender/build_assets.py"), "--") + $options
    $log = @(& $blender @arguments 2>&1 | ForEach-Object { "$_" })
    if ($LASTEXITCODE -ne 0) { $log | Select-Object -Last 20 | Write-Host; throw "build_assets $options failed" }
    return $log
}
$failures = 0
function Expect([bool]$condition, [string]$what) {
    if ($condition) { Write-Host "ok   $what" }
    else { Write-Host "FAIL $what" -ForegroundColor Red; $script:failures++ }
}
function Changed($before, $after) {
    @($before.Keys | Where-Object { $before[$_] -ne $after[$_] } | ForEach-Object { $_.Substring($root.Path.Length + 1) })
}

$before = Snapshot
$log = Run @("--only", "fields", "--what", "markers")
$after = Snapshot
Expect (-not ($log -match '\.glb')) "markers only: no model exported or compared"
Expect (-not ($log -match '^bake:|^lightmap:')) "markers only: nothing baked"
Expect (@($log -match 'windmill_field\.markers\.json').Count -eq 1) "markers only: the level's markers written or confirmed"
$changed = Changed $before $after
Expect ($changed.Count -eq 0) ("markers only: no output touched" + $(if ($changed) { ": " + ($changed -join ", ") }))

$log = Run @("--only", "shrine")
$after2 = Snapshot
$changed = Changed $after $after2
Expect ($changed.Count -eq 0) ("one unit, unchanged sources: no output touched" + $(if ($changed) { ": " + ($changed -join ", ") }))
Expect (@($log -match '^export: 0 written').Count -eq 1) "one unit, unchanged sources: 0 written"
Expect (-not ($log -match 'Content[/\\]Kit|Street[/\\]|Night[/\\]|Fields[/\\]')) "one unit: no other unit's output read"

if ($failures) { Write-Host "FAILED: $failures"; exit 1 }
Write-Host "PASS"
