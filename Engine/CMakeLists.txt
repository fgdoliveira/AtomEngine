add_library(AtomEngine STATIC)

target_sources(
    AtomEngine
    PRIVATE
        Core/Application.cpp
        Platform/Window.cpp

    PUBLIC
        Core/Application.h
        Platform/Window.h
)

target_include_directories(
    AtomEngine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    AtomEngine
    PUBLIC
        SDL3::SDL3
)

target_compile_features(
    AtomEngine
    PUBLIC
        cxx_std_20
)