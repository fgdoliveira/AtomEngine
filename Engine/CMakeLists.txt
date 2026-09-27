add_library(AtomEngine STATIC)

target_sources(
    AtomEngine
    PRIVATE
        Assets/Model.cpp
        Assets/ThirdParty.cpp
        Core/Application.cpp
        Core/Time.cpp
        Physics/CollisionWorld.cpp
        Platform/Input.cpp
        Platform/Window.cpp
        Renderer/Mesh.cpp
        Renderer/RenderTargets.cpp
        Renderer/Renderer.cpp
        Renderer/Shader.cpp
        Renderer/Texture.cpp
        Scene/Camera.cpp

    PUBLIC
        Assets/Model.h
        Core/Application.h
        Core/Time.h
        Physics/CollisionWorld.h
        Platform/Input.h
        Platform/Window.h
        Renderer/Material.h
        Renderer/Mesh.h
        Renderer/RenderSettings.h
        Renderer/RenderTargets.h
        Renderer/Renderer.h
        Renderer/Shader.h
        Renderer/Texture.h
        Scene/Camera.h
)

target_include_directories(
    AtomEngine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/external/cgltf
        ${CMAKE_SOURCE_DIR}/external/stb
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
