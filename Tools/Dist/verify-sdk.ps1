# Checks a staged source SDK (v0.0.20, M131; Tools/Dist/sdk.ps1 runs it):
#
#   pwsh Tools/Dist/verify-sdk.ps1 -Path Dist/AtomSDK-v<version> [-NoConfigure]
#
#   present   what building the engine, its tools and a game needs, and
#             every licence that travels with it;
#   absent    what is development-only: other apps, the engine's tests and
#             scripts, Blender sources, build output, CI, agent notes;
#   configure unless -NoConfigure: the SDK on its own, in a fresh folder
#             outside it, makes the engine, the tools, the workspace and
#             HelloAtom - and nothing that isn't there.
param(
    [Parameter(Mandatory)][string]$Path,
    [switch]$NoConfigure
)
$ErrorActionPreference = "Stop"
. "$PSScriptRoot/../Dev/common.ps1"
$sdk = (Resolve-Path $Path).Path
$failures = 0
function Fail([string]$why) { Write-Host "  FAIL $why" -ForegroundColor Red; $script:failures++ }
function Ok([string]$what) { Write-Host "  ok   $what" }

Write-Host "== present"
$required = @(
    "CMakeLists.txt", "SDK-README.txt", "LICENSE.txt", "README.md", "CHANGELOG.md",
    "Engine/Core/Application.h", "Framework/Level/LevelData.h", "Authoring/Preview/ScenePreview.h",
    "Project/Project/ProjectCreator.h", "WorkspaceUI/ImGuiHost.h", "Shaders/CMakeLists.txt",
    "cmake/AtomApps.cmake", "cmake/AtomProject.cmake", "cmake/AtomTesting.cmake",
    "Tools/Workspace/Main.cpp", "Tools/ProjectTool/Main.cpp", "Tools/LevelCheck/Main.cpp",
    "Apps/Samples/HelloAtom/Main.cpp", "Apps/Samples/HelloAtom/HelloAtom.atomproject.json",
    "Content/Kit", "Content/Environments", "docs/Getting-Started.md", "docs/Architecture.md",
    "external/SDL/CMakeLists.txt", "external/glm/CMakeLists.txt", "external/json/CMakeLists.txt",
    "external/imgui/imgui.cpp", "external/imgui/backends/imgui_impl_sdlgpu3.cpp",
    "external/cgltf/cgltf.h", "external/stb/stb_image.h", "external/doctest/doctest/doctest.h"
)
foreach ($item in $required) {
    if (Test-Path -LiteralPath (Join-Path $sdk $item)) { Ok $item } else { Fail "missing $item" }
}
# Every licence that travels with the code and content.
$licences = @(
    "LICENSE.txt", "external/SDL/LICENSE.txt", "external/glm/copying.txt", "external/json/LICENSE.MIT",
    "external/cgltf/LICENSE", "external/stb/LICENSE", "external/imgui/LICENSE.txt", "external/doctest/LICENSE.txt",
    "Engine/Debug/Fonts/LICENSE.txt", "Content/Fonts/OFL.txt"
)
foreach ($licence in $licences) {
    if (Test-Path -LiteralPath (Join-Path $sdk $licence)) { Ok "licence $licence" } else { Fail "missing licence $licence" }
}

Write-Host "== absent"
$forbidden = @(
    @{ Pattern = '^Apps/(Showcase|Games|Benchmarks)/'; Why = "an app other than HelloAtom" },
    @{ Pattern = '^Tests/'; Why = "the engine's own tests" },
    @{ Pattern = '^Tools/(Blender|Dev|Perf|Dist|Docs|Machines|PresentationProbe)/'; Why = "development tooling" },
    @{ Pattern = '\.blend$'; Why = "a Blender source" },
    @{ Pattern = '\.(pdb|exe|dll|lib|obj|ilk|zip)$'; Why = "build output" },
    @{ Pattern = '^(build|build-.*|out|NoTrack|Dist)/'; Why = "a build or local folder" }, # the repository's top level: SDL has a src/hidapi/dist/ of its own
    @{ Pattern = '(^|/)\.github/'; Why = "CI configuration" },
    @{ Pattern = '^(CLAUDE\.md|\.gitmodules|\.gitignore)$'; Why = "repository-only file" },
    @{ Pattern = '^external/SDL/(test|examples|docs|VisualC|Xcode|android-project)/'; Why = "SDL's tests, samples or IDE projects" },
    @{ Pattern = '^external/(json/tests|glm/test|stb/tests|imgui/examples|doctest/examples)/'; Why = "a dependency's tests or samples" }
)
$found = 0
foreach ($file in Get-ChildItem $sdk -Recurse -File) {
    $relative = [System.IO.Path]::GetRelativePath($sdk, $file.FullName).Replace("\", "/")
    foreach ($rule in $forbidden) {
        if ($relative -cmatch $rule.Pattern) { Fail "$($rule.Why): $relative"; $found++; break } # case matters: paths are exact
    }
}
if ($found -eq 0) { Ok "no development-only material" }

if (-not $NoConfigure -and $failures -eq 0) {
    Write-Host "== configure (the SDK on its own, outside it)"
    $cmake = Find-DevTool "cmake"
    $build = Join-Path ([System.IO.Path]::GetTempPath()) "AtomSDK-verify-build"
    if (Test-Path $build) { Remove-Item $build -Recurse -Force }
    $out = & $cmake -S $sdk -B $build 2>&1
    if ($LASTEXITCODE -ne 0) {
        Fail "the SDK didn't configure:"
        $out | Select-Object -Last 20 | ForEach-Object { Write-Host "    $_" }
    }
    else {
        foreach ($target in "AtomEngine", "AtomFramework", "AtomWorkspace", "AtomLevelCheck", "AtomProjectTool", "HelloAtom") {
            if (Get-ChildItem $build -Recurse -Filter "$target.vcxproj" -ErrorAction SilentlyContinue) { Ok "target $target" } else { Fail "no target $target" }
        }
        foreach ($target in "Demo", "Showcase", "AtomEngineTests") {
            if (Get-ChildItem $build -Recurse -Filter "$target.vcxproj" -ErrorAction SilentlyContinue) { Fail "a target that isn't in the SDK: $target" }
        }
    }
    Remove-Item $build -Recurse -Force -ErrorAction SilentlyContinue
}

if ($failures -gt 0) { Write-Host "FAILED: $failures problem(s) in $sdk" -ForegroundColor Red; exit 1 }
Write-Host "PASS: $sdk"
exit 0
