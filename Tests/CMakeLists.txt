# Unit tests for pure logic (no window, no GPU). Run with ctest or directly.
add_executable(
    AtomTests
    TestMain.cpp
    SlotMapTests.cpp
    RaycastTests.cpp
    InteractionTests.cpp
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
