# The Showcase (v0.0.14): every engine feature in one scene, built only from
# the engine and the framework - the engine's front door, and the default
# startup project. A showcase, not a product: it has no install rules and
# is never packaged (the pavilion's character has no recorded licence).

# What the unit tests reach (built without the game too, as CI does): the
# synth booth's sequencer.
add_library(
    ShowcaseLib
    STATIC
    BoothSynth.cpp
    BoothSynth.h
    Features/Features.cpp
    Features/Features.h
)
target_include_directories(ShowcaseLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ShowcaseLib PUBLIC AtomEngine)

# The Showcase's CPU tests (M105): the booth, the lens's features, the
# pavilion's skinned character, its levels.
if(ATOM_BUILD_TESTS)
    atom_add_tests(
        ShowcaseTests
        OWNER showcase
        ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/Assets"
        SOURCES
            Tests/AssetOwnershipTests.cpp
            Tests/BoothSynthTests.cpp
            Tests/LevelContentTests.cpp
            Tests/ShowcaseFeatureTests.cpp
            Tests/SkinningTests.cpp
        LIBS ShowcaseLib AtomAuthoring nlohmann_json::nlohmann_json # AtomAuthoring: the level checks
    )
endif()

if(NOT ATOM_BUILD_GAME)
    return() # the libraries only: no executable, no shaders
endif()

add_executable(
    Showcase
    Main.cpp
    ShowcaseApp.cpp
    ShowcaseApp.h
    ShowcaseLab.cpp
    ShowcaseLens.cpp
    ShowcaseMenus.cpp
)
target_link_libraries(
    Showcase
    PRIVATE
        ShowcaseLib
        AtomEngine
        AtomFramework
)
target_compile_definitions(Showcase PRIVATE ATOM_VERSION="${PROJECT_VERSION}") # M90: the benchmark file
if(ATOM_DISTRIBUTION)
    set_target_properties(Showcase PROPERTIES WIN32_EXECUTABLE ON)
endif()

# Its own folder, bin/<config>/Showcase/ (M108, cmake/AtomApps.cmake): its
# assets in Assets/Showcase/, looked up first, and the shared pieces it
# uses (Content/) in Assets/<folder>.
set(SHOWCASE_PAYLOAD "${CMAKE_CURRENT_SOURCE_DIR}/Assets" "Assets/Showcase")
foreach(folder IN ITEMS Environments First Fonts Kit Sky)
    list(APPEND SHOWCASE_PAYLOAD "${CMAKE_SOURCE_DIR}/Content/${folder}" "Assets/${folder}")
endforeach()
atom_app_payload(Showcase COPY ${SHOWCASE_PAYLOAD})

set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Showcase)

# The Showcase's in-game scenarios (M107, cmake/AtomTesting.cmake).
if(ATOM_BUILD_TESTS)
    atom_add_scenarios(Showcase OWNER showcase DIR "${CMAKE_CURRENT_SOURCE_DIR}/Scenarios" SMOKE showcase_lens)
endif()
