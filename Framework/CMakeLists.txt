# The game framework (M76): what every game made with AtomEngine shares and
# the engine itself doesn't own - the run's log, the settings model and
# command line, the calibration decision. Games link it; they don't share
# each other's code. Namespace AtomFramework.
add_library(
    AtomFramework
    STATIC
    Platform/RunLog.cpp
    Platform/RunLog.h
    Platform/AssetRoots.cpp
    Platform/AssetRoots.h
    Settings/GameSettings.cpp
    Settings/GameSettings.h
    Settings/Calibration.cpp
    Settings/Calibration.h
    Diagnostics/DiagnosticsReport.cpp
    Diagnostics/DiagnosticsReport.h
)

target_include_directories(
    AtomFramework
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# SDL for the pref path (RunLog); JSON only inside the settings parser.
target_link_libraries(
    AtomFramework
    PUBLIC
        SDL3::SDL3
        AtomEngine # M82: the developer switches, and the renderer the diagnostics report reads
    PRIVATE
        nlohmann_json::nlohmann_json
)
