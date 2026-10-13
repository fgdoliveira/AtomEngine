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

# M116: the hand-made scene goes through the same export.
$sceneFile = Join-Path $root "Apps/Samples/HelloAtom/Source/hello_room.blend"
$helloAssets = Join-Path $root "Apps/Samples/HelloAtom/Assets"
function Scene([string[]]$options) {
    $arguments = @("-b", "--factory-startup", $sceneFile, "--python", (Join-Path $root "Tools/Blender/export_scene.py"), "--") + $options
    $log = @(& $blender @arguments 2>&1 | ForEach-Object { "$_" })
    return @{ Log = $log; Exit = $LASTEXITCODE }
}
$roomFiles = @("Room/hello_room.glb", "Room/hello_room_col.glb", "Levels/hello_room.markers.json") |
    ForEach-Object { Get-Item (Join-Path $helloAssets $_) }
$roomBefore = @{}; foreach ($f in $roomFiles) { $roomBefore[$f.FullName] = (Get-FileHash $f.FullName -Algorithm MD5).Hash }
$run = Scene @("--level", "hello_room", "--out", $helloAssets, "--bake", "sky")
Expect ($run.Exit -eq 0) "scene: the hand-made room exports"
Expect (@($run.Log -match '^export: 0 written').Count -eq 1) "scene: unchanged .blend, 0 written"
$roomChanged = @($roomFiles | Where-Object { (Get-FileHash $_.FullName -Algorithm MD5).Hash -ne $roomBefore[$_.FullName] })
Expect ($roomChanged.Count -eq 0) "scene: its outputs byte-identical"

# Exported over a level the procedural build owns: refused, nothing written.
$before = Snapshot
$run = Scene @("--level", "windmill_field", "--out", (Join-Path $root "Apps/Games/Demo/Assets"))
$after3 = Snapshot
Expect ($run.Exit -ne 0 -and @($run.Log -match 'refusing to overwrite').Count -ge 1) "scene: a file another source generated is refused"
$changed = Changed $before $after3
Expect ($changed.Count -eq 0) ("scene: refused, nothing written" + $(if ($changed) { ": " + ($changed -join ", ") }))

# Every committed .blend is free of personal paths (clean_scene.py).
foreach ($blend in @(git -C $root ls-files "*.blend")) {
    $log = @(& $blender -b --factory-startup (Join-Path $root $blend) --python (Join-Path $root "Tools/Blender/clean_scene.py") -- --check 2>&1 | ForEach-Object { "$_" })
    Expect ($LASTEXITCODE -eq 0) ("$blend holds no personal path" + $(if ($LASTEXITCODE) { ": " + (($log -match 'PERSONAL') -join "; ") }))
}

if ($failures) { Write-Host "FAILED: $failures"; exit 1 }
Write-Host "PASS"
