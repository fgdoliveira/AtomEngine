add_library(AtomEngine STATIC)

target_sources(
    AtomEngine
    PRIVATE
        Core/Application.cpp
        Core/Time.cpp
        Platform/Input.cpp
        Platform/Window.cpp
        Renderer/Renderer.cpp

    PUBLIC
        Core/Application.h
        Core/Time.h
        Platform/Input.h
        Platform/Window.h
        Renderer/Renderer.h
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
        glm::glm
)

target_compile_definitions(
    AtomEngine
    PUBLIC
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_RADIANS
)

target_compile_features(
    AtomEngine
    PUBLIC
        cxx_std_20
)
