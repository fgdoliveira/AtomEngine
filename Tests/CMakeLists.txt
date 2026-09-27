# Unit tests for pure logic (no window, no GPU). Run with ctest or directly.
add_executable(
    AtomTests
    TestMain.cpp
    SlotMapTests.cpp
    RaycastTests.cpp
    InteractionTests.cpp
    DialogueTests.cpp
    LevelTests.cpp
)

target_link_libraries(
    AtomTests
    PRIVATE
        AtomGameLib
)

target_include_directories(
    AtomTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/external/doctest
)

add_test(NAME AtomTests COMMAND AtomTests)

# Lets tests validate shipped content (e.g. Assets/Dialogue/*.json).
target_compile_definitions(
    AtomTests
    PRIVATE
        ATOM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
