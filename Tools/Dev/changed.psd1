# What `check.ps1 -Level changed` runs for each changed file (M81; by
# owner since v0.0.16, M107). Rules are tried in order; the first whose
# Pattern (a regex on the repository-relative path, forward slashes)
# matches decides the file.
#   Tests      owners whose CPU tests to build and run (Unit.<owner>,
#              Content.<owner>, and since M112 LevelCheck.<owner> - their
#              levels through AtomLevelCheck): engine, framework, content,
#              demo, drift, showcase, hello, arena
#   Smoke      $true: every app's smoke scenario (ctest -L smoke) - what an
#              engine or framework change owes every app
#   Scenarios  scenarios by <owner>.<name> (Scenario.<owner>.<name> in ctest)
#   Build      $false: nothing to build for this file (docs, scripts)
#   Package    a game whose package to stage and verify (-NoSmoke)
# Only what a rule names is built: a demo change compiles no engine or
# framework test. A file no rule matches is reported as unmapped and gets
# the Fallback - never silently nothing. Keep this table honest when you
# add a scenario, a test target or a folder.
@{
    # Owner -> its CPU test target; app owner -> the executable its scenarios run.
    TestTargets = @{ engine = 'AtomEngineTests'; framework = 'AtomFrameworkTests'; content = 'AtomContentTests'
                     demo = 'DemoTests'; drift = 'DriftTests'; showcase = 'ShowcaseTests'; arena = 'ArenaTests' }
    Apps = @{ demo = 'Demo'; drift = 'Drift'; showcase = 'Showcase'; hello = 'HelloAtom'; arena = 'AtomArena' }

    Rules = @(
        # Docs and repository text: nothing to build. The CI workflow tests itself.
        @{ Pattern = '^(docs/|NoTrack/|audits/)|\.md$|^CHANGELOG|^LICENSE|^\.gitignore$|^\.gitattributes$|^\.github/'; Build = $false }

        # DRIFT
        @{ Pattern = '^Apps/Games/Drift/(CMakeLists\.txt|README\.txt\.in)$'; Tests = @('drift'); Scenarios = @('drift.drift_smoke', 'drift.drift_diagnostics'); Package = 'Drift' } # M83: its package
        @{ Pattern = '^Apps/Games/Drift/Tests/'; Tests = @('drift') }
        @{ Pattern = '^Apps/Games/Drift/'; Tests = @('drift'); Scenarios = @('drift.drift_fly', 'drift.drift_diagnostics') }

        # The Showcase (v0.0.14): a scenario script runs itself; its tests; anything else, the tour.
        @{ Pattern = '^Apps/Showcase/Scenarios/(?<scenario>[^/]+)\.atomtest$'; Scenarios = @('showcase.$scenario') }
        @{ Pattern = '^Apps/Showcase/Tests/'; Tests = @('showcase') }
        @{ Pattern = '^Apps/Showcase/(ShowcaseLab.cpp|Assets/(Lab|ThirdParty)/)'; Tests = @('showcase'); Scenarios = @('showcase.showcase_lab', 'showcase.showcase_tour') }
        @{ Pattern = '^Apps/Showcase/'; Tests = @('showcase'); Scenarios = @('showcase.showcase_tour', 'showcase.showcase_lens', 'showcase.showcase_benchmark') }

        # The samples (M91, M93); ATOM ARENA's simulation has its CPU tests.
        @{ Pattern = '^Apps/Samples/HelloAtom/'; Tests = @('hello'); Scenarios = @('hello.hello_atom') }
        @{ Pattern = '^Apps/Benchmarks/AtomArena/Scenarios/(?<scenario>[^/]+)\.atomtest$'; Scenarios = @('arena.$scenario') }
        @{ Pattern = '^Apps/Benchmarks/AtomArena/Tests/'; Tests = @('arena') }
        @{ Pattern = '^(Apps/Benchmarks/AtomArena/|Tools/Perf/arena\.ps1)'; Tests = @('arena'); Scenarios = @('arena.arena_smoke') }

        # The engine: its tests and the framework's (built on it), and every
        # app's smoke run. Where a subsystem has an app's checks of its own,
        # those too.
        @{ Pattern = '^(Engine/Core/(Trace|Profile)|Tests/Engine/TraceTests)'; Tests = @('engine'); Scenarios = @('arena.arena_smoke') } # M96: the tracer
        @{ Pattern = '^Engine/Assets/'; Tests = @('engine', 'framework', 'content', 'demo', 'showcase'); Smoke = $true; Scenarios = @('showcase.showcase_lab') } # the apps' content tests load models
        @{ Pattern = '^Engine/Physics/'; Tests = @('engine', 'framework', 'demo'); Smoke = $true } # the demo's collision meshes
        @{ Pattern = '^Engine/Audio/'; Tests = @('engine', 'framework', 'drift', 'showcase'); Smoke = $true } # DRIFT's music, the booth's synth
        @{ Pattern = '^(Engine/Renderer/|Shaders/)'; Tests = @('engine', 'framework'); Smoke = $true; Scenarios = @('demo.quality_tiers') }
        @{ Pattern = '^Engine/Debug/'; Tests = @('engine', 'framework'); Smoke = $true; Scenarios = @('demo.devtools', 'showcase.showcase_benchmark') }

        # The framework (M85: the world layer; M89: the UI every app shares), by system.
        @{ Pattern = '^Framework/Schemas/'; Tests = @('framework', 'content', 'demo', 'showcase', 'arena') } # the content tests check files against them
        @{ Pattern = '^Framework/Level/'; Tests = @('framework', 'content', 'demo', 'showcase', 'hello', 'arena'); Smoke = $true; Scenarios = @('demo.hot_reload') }
        @{ Pattern = '^Framework/Environment/'; Tests = @('framework', 'content'); Smoke = $true; Scenarios = @('demo.environment') }
        @{ Pattern = '^Framework/Character/'; Tests = @('framework'); Smoke = $true; Scenarios = @('showcase.showcase_lab') }
        @{ Pattern = '^Framework/Testing/'; Tests = @('framework'); Smoke = $true; Scenarios = @('demo.diagnostics', 'demo.devtools') }
        @{ Pattern = '^Framework/(Debug|UI|Settings)/'; Tests = @('framework'); Smoke = $true; Scenarios = @('demo.devtools', 'showcase.showcase_benchmark') }
        @{ Pattern = '^(Engine/|Framework/)CMakeLists\.txt$'; Tests = @('engine', 'framework', 'content', 'demo', 'drift', 'showcase', 'hello', 'arena'); Smoke = $true; Package = 'Demo' }
        @{ Pattern = '^Engine/'; Tests = @('engine', 'framework'); Smoke = $true }
        @{ Pattern = '^Framework/'; Tests = @('framework'); Smoke = $true }

        # The demo (v0.0.14: Apps/Games/Demo), its package
        @{ Pattern = '^Apps/Games/Demo/(CMakeLists\.txt|README\.txt\.in)$'; Tests = @('demo'); Scenarios = @('demo.street_keeper', 'demo.diagnostics'); Package = 'Demo' }
        @{ Pattern = '^Apps/Games/Demo/Scenarios/(?<scenario>[^/]+)\.atomtest$'; Scenarios = @('demo.$scenario') }
        @{ Pattern = '^Apps/Games/Demo/Tests/'; Tests = @('demo') }

        # The demo's content (before its code: first match wins). Its content tests check it; the levels load it.
        @{ Pattern = '^Apps/Games/Demo/Assets/(Levels|Environments)/'; Tests = @('demo'); Scenarios = @('demo.levels_roundtrip', 'demo.hot_reload', 'demo.environment') }
        @{ Pattern = '^Apps/Games/Demo/Assets/(Machines|Pachinko)/'; Tests = @('demo'); Scenarios = @('demo.pachinko_session') }
        @{ Pattern = '^Apps/Games/Demo/Assets/'; Tests = @('demo'); Scenarios = @('demo.street_keeper', 'demo.levels_roundtrip') }
        # Shared content: kit pieces, sky, fonts - every app that resolves it.
        @{ Pattern = '^Content/'; Tests = @('content', 'demo', 'showcase', 'arena'); Smoke = $true; Scenarios = @('demo.night_street') }

        # The demo, by system
        @{ Pattern = '^Apps/Games/Demo/Pachinko/'; Tests = @('demo'); Scenarios = @('demo.pachinko_session') }
        @{ Pattern = '^Apps/Games/Demo/Input/'; Tests = @('demo'); Scenarios = @('demo.street_keeper', 'demo.pachinko_session') }
        @{ Pattern = '^Apps/Games/Demo/Dialogue/'; Tests = @('demo'); Scenarios = @('demo.street_keeper') }
        @{ Pattern = '^Apps/Games/Demo/Flashlight'; Tests = @('demo'); Scenarios = @('demo.flashlight', 'demo.passage') }
        @{ Pattern = '^Apps/Games/Demo/DemoAppDevTools'; Tests = @('demo'); Scenarios = @('demo.devtools') }
        @{ Pattern = '^Apps/Games/Demo/DemoAppCalibration'; Tests = @('demo'); Scenarios = @('demo.calibration', 'demo.quality_tiers') }
        @{ Pattern = '^Apps/Games/Demo/'; Tests = @('demo'); Scenarios = @('demo.street_keeper', 'demo.levels_roundtrip') }

        # The tests themselves, by owner; the shared support and helpers: every owner.
        @{ Pattern = '^Tests/Engine/'; Tests = @('engine') }
        @{ Pattern = '^Tests/Framework/'; Tests = @('framework') }
        @{ Pattern = '^Tests/Content/'; Tests = @('content') }
        # M112: the level check - its tool and every app's levels through it.
        @{ Pattern = '^Tools/LevelCheck/'; Tests = @('framework', 'content', 'demo', 'showcase', 'hello', 'arena') }
        @{ Pattern = '^(Tests/|cmake/)'; Tests = @('engine', 'framework', 'content', 'demo', 'drift', 'showcase', 'hello', 'arena'); Smoke = $true }

        # Packaging and the build: stage and verify a package.
        @{ Pattern = '^Tools/Dist/'; Package = 'Demo' }
        @{ Pattern = '(^|/)CMakeLists\.txt$'; Tests = @('engine', 'framework', 'content', 'demo', 'drift', 'showcase', 'hello', 'arena'); Smoke = $true; Package = 'Demo' }

        # Tools that don't ship or build: nothing to build (a script is checked by running it).
        @{ Pattern = '^Tools/(Dev|Perf|Blender|Docs|Machines|PresentationProbe)/'; Build = $false }
        @{ Pattern = '^(CLAUDE|README)'; Build = $false }
    )

    # For unmapped files: every owner's tests, the smoke runs and the broad rendering set.
    Fallback = @{ Tests = @('engine', 'framework', 'content', 'demo', 'drift', 'showcase', 'hello', 'arena'); Smoke = $true
                  Scenarios = @('demo.night_street', 'showcase.showcase_tour') }
}
