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

        # The engine
        @{ Pattern = '^(Engine/Renderer/|Shaders/)'; Scenarios = @('first_render', 'lakeshore', 'night_street', 'drift_fly') }
        @{ Pattern = '^Engine/Audio/'; Scenarios = @('drift_fly', 'pachinko_session') }
        @{ Pattern = '^Engine/Debug/'; Scenarios = @('devtools') }
        @{ Pattern = '^Engine/Assets/'; Scenarios = @('first_render', 'character_lab', 'drift_fly') }
        @{ Pattern = '^(Engine/|Framework/)'; Scenarios = @('first_render', 'diagnostics', 'quality_tiers', 'gpu_fallback', 'drift_fly', 'drift_diagnostics') }

        # The demo, by system
        @{ Pattern = '^Game/Pachinko/'; Scenarios = @('pachinko_session') }
        @{ Pattern = '^Game/(Character/|DemoAppLab)'; Scenarios = @('character_lab') }
        @{ Pattern = '^Game/Environment/'; Scenarios = @('environment', 'lakeshore') }
        @{ Pattern = '^Game/(Dialogue|Interaction)/'; Scenarios = @('street_keeper') }
        @{ Pattern = '^Game/Flashlight'; Scenarios = @('flashlight', 'passage') }
        @{ Pattern = '^Game/DemoAppDevTools'; Scenarios = @('devtools') }
        @{ Pattern = '^Game/(DemoAppCalibration|Settings/)'; Scenarios = @('calibration', 'quality_tiers') }
        @{ Pattern = '^Game/Testing/'; Scenarios = @('diagnostics', 'first_render') }
        @{ Pattern = '^Game/Level/'; Scenarios = @('levels_roundtrip', 'hot_reload') }
        @{ Pattern = '^Game/'; Scenarios = @('first_render', 'levels_roundtrip') }

        # Content: the authoring tests check it; the levels load it.
        @{ Pattern = '^Assets/(Levels|Environments)/'; Scenarios = @('levels_roundtrip', 'hot_reload', 'environment') }
        @{ Pattern = '^Assets/(Machines|Pachinko)/'; Scenarios = @('pachinko_session') }
        @{ Pattern = '^Assets/Lakeshore/'; Scenarios = @('lakeshore') }
        @{ Pattern = '^Assets/(Lab|ThirdParty)/'; Scenarios = @('character_lab') }
        @{ Pattern = '^Assets/'; Scenarios = @('first_render', 'levels_roundtrip') }

        # Tests: a scenario script runs itself; the rest are unit tests (always run).
        @{ Pattern = '^Tests/Scenarios/(?<scenario>[^/]+)\.atomtest$'; Scenarios = @('$scenario') }
        @{ Pattern = '^Tests/'; Scenarios = @() }

        # Packaging and the build: stage and verify a package.
        @{ Pattern = '^Tools/Dist/'; Scenarios = @(); Package = 'AtomGame' }
        @{ Pattern = '(^|/)CMakeLists\.txt$'; Scenarios = @(); Package = 'AtomGame' }

        # Tools that don't ship or build: nothing beyond the unit tests.
        @{ Pattern = '^Tools/(Dev|Perf|Blender|Docs|Machines|PresentationProbe)/'; Scenarios = @() }
        @{ Pattern = '^(CLAUDE|README)'; Build = $false; Scenarios = @() }
    )

    # For unmapped files: the broad rendering set.
    Fallback = @('first_render', 'lakeshore', 'night_street', 'drift_fly')
}
