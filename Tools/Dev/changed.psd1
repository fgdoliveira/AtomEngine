# What `check.ps1 -Level changed` runs for each changed file (M81).
# Rules are tried in order; the first whose Pattern (a regex on the
# repository-relative path, forward slashes) matches decides the file.
#   Scenarios  scenario names (Scenario.<name> in ctest) to run
#   Build      $false: nothing to build for this file (docs)
#   Package    a game whose package to stage and verify (-NoSmoke)
# The unit and authoring tests always run when anything builds. A file no
# rule matches is reported as unmapped and gets the Fallback set - never
# silently nothing. Keep this table honest when you add a scenario.
@{
    Rules = @(
        # Docs and repository text: nothing to build. The CI workflow tests itself.
        @{ Pattern = '^(docs/|NoTrack/|audits/)|\.md$|^CHANGELOG|^LICENSE|^\.gitignore$|^\.gitattributes$|^\.github/'; Build = $false; Scenarios = @() }

        # DRIFT
        @{ Pattern = '^Games/Drift/(CMakeLists\.txt|README\.txt\.in)$'; Scenarios = @('drift_fly', 'drift_diagnostics'); Package = 'Drift' } # M83: its package
        @{ Pattern = '^Games/Drift/'; Scenarios = @('drift_fly', 'drift_diagnostics') }

        # The Showcase (v0.0.14): a scenario script runs itself; anything else, the tour.
        @{ Pattern = '^Showcase/Scenarios/(?<scenario>[^/]+)\.atomtest$'; Scenarios = @('$scenario') }
        @{ Pattern = '^Showcase/'; Scenarios = @('showcase_tour') }

        # The engine
        @{ Pattern = '^(Engine/Renderer/|Shaders/)'; Scenarios = @('first_render', 'lakeshore', 'night_street', 'drift_fly', 'showcase_tour') }
        @{ Pattern = '^Engine/Audio/'; Scenarios = @('drift_fly', 'pachinko_session') }
        @{ Pattern = '^Engine/Debug/'; Scenarios = @('devtools') }
        @{ Pattern = '^Engine/Assets/'; Scenarios = @('first_render', 'character_lab', 'drift_fly') }
        # The framework's world layer (M85: moved from the demo), by system - before the catch-all.
        @{ Pattern = '^Framework/Schemas/'; Scenarios = @() } # the authoring tests (always run) check it
        @{ Pattern = '^Framework/Level/'; Scenarios = @('levels_roundtrip', 'hot_reload', 'night_street', 'pachinko_session') }
        @{ Pattern = '^Framework/(Interaction|World)/'; Scenarios = @('street_keeper', 'levels_roundtrip', 'pachinko_session') }
        @{ Pattern = '^Framework/Environment/'; Scenarios = @('environment', 'lakeshore') }
        @{ Pattern = '^Framework/Character/'; Scenarios = @('character_lab', 'passage') }
        @{ Pattern = '^Framework/Testing/'; Scenarios = @('diagnostics', 'first_render', 'devtools') }
        @{ Pattern = '^(Engine/|Framework/)'; Scenarios = @('first_render', 'diagnostics', 'quality_tiers', 'gpu_fallback', 'drift_fly', 'drift_diagnostics', 'showcase_tour') }

        # The demo (v0.0.14: Games/Demo), its package
        @{ Pattern = '^Games/Demo/(CMakeLists\.txt|README\.txt\.in)$'; Scenarios = @('first_render', 'diagnostics'); Package = 'Demo' }

        # The demo's content (before its code: first match wins). The authoring tests check it; the levels load it.
        @{ Pattern = '^Games/Demo/Assets/(Levels|Environments)/'; Scenarios = @('levels_roundtrip', 'hot_reload', 'environment') }
        @{ Pattern = '^Games/Demo/Assets/(Machines|Pachinko)/'; Scenarios = @('pachinko_session') }
        @{ Pattern = '^Games/Demo/Assets/Lakeshore/'; Scenarios = @('lakeshore') }
        @{ Pattern = '^Games/Demo/Assets/(Lab|ThirdParty)/'; Scenarios = @('character_lab') }
        @{ Pattern = '^Games/Demo/Assets/'; Scenarios = @('first_render', 'levels_roundtrip') }
        # Shared content: kit pieces, sky, fonts.
        @{ Pattern = '^Content/'; Scenarios = @('first_render', 'levels_roundtrip', 'night_street') }

        # The demo, by system
        @{ Pattern = '^Games/Demo/Pachinko/'; Scenarios = @('pachinko_session') }
        @{ Pattern = '^Games/Demo/DemoAppLab'; Scenarios = @('character_lab') }
        @{ Pattern = '^Games/Demo/Input/'; Scenarios = @('street_keeper', 'pachinko_session', 'character_lab') }
        @{ Pattern = '^Games/Demo/Dialogue/'; Scenarios = @('street_keeper') }
        @{ Pattern = '^Games/Demo/Flashlight'; Scenarios = @('flashlight', 'passage') }
        @{ Pattern = '^Games/Demo/DemoAppDevTools'; Scenarios = @('devtools') }
        @{ Pattern = '^Games/Demo/DemoAppCalibration'; Scenarios = @('calibration', 'quality_tiers') }
        @{ Pattern = '^Games/Demo/'; Scenarios = @('first_render', 'levels_roundtrip') }

        # Tests: a scenario script runs itself; the rest are unit tests (always run).
        @{ Pattern = '^Tests/Scenarios/(?<scenario>[^/]+)\.atomtest$'; Scenarios = @('$scenario') }
        @{ Pattern = '^Tests/'; Scenarios = @() }

        # Packaging and the build: stage and verify a package.
        @{ Pattern = '^Tools/Dist/'; Scenarios = @(); Package = 'Demo' }
        @{ Pattern = '(^|/)CMakeLists\.txt$'; Scenarios = @(); Package = 'Demo' }

        # Tools that don't ship or build: nothing beyond the unit tests.
        @{ Pattern = '^Tools/(Dev|Perf|Blender|Docs|Machines|PresentationProbe)/'; Scenarios = @() }
        @{ Pattern = '^(CLAUDE|README)'; Build = $false; Scenarios = @() }
    )

    # For unmapped files: the broad rendering set.
    Fallback = @('first_render', 'lakeshore', 'night_street', 'drift_fly', 'showcase_tour')
}
