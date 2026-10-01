# Compiles HLSL to DXIL offline with dxc. Shaders are named
# <Name>.<stage>.hlsl where stage is vert or frag, and land next to the
# executable in shaders/<Name>.<stage>.dxil.

set(ATOM_WINDOWS_SDK_BIN "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/bin")
file(GLOB ATOM_WINDOWS_SDK_VERSIONS LIST_DIRECTORIES true "${ATOM_WINDOWS_SDK_BIN}/10.*")
list(SORT ATOM_WINDOWS_SDK_VERSIONS COMPARE NATURAL ORDER DESCENDING)
list(TRANSFORM ATOM_WINDOWS_SDK_VERSIONS APPEND "/x64")

find_program(
    ATOM_DXC
    NAMES dxc
    HINTS
        $ENV{VULKAN_SDK}/Bin
        ${ATOM_WINDOWS_SDK_VERSIONS}
    REQUIRED
)
message(STATUS "AtomEngine shader compiler: ${ATOM_DXC}")

set(ATOM_SHADER_SOURCES
    Basic.vert.hlsl
    Basic.frag.hlsl
    Fullscreen.vert.hlsl
    Post.frag.hlsl
    GlowBright.frag.hlsl
    GlowBlur.frag.hlsl
    Sky.vert.hlsl
    Sky.frag.hlsl
    Halo.frag.hlsl
    Shadow.vert.hlsl
    Shadow.frag.hlsl
    Skinned.vert.hlsl
    ShadowSkinned.vert.hlsl
    Particle.vert.hlsl
    Particle.frag.hlsl
    UI.vert.hlsl
    UI.frag.hlsl
)

# Headers every shader may include; a change recompiles all shaders.
set(ATOM_SHADER_INCLUDES
    Common.hlsli
    Sway.hlsli
    Skinning.hlsli
)
list(TRANSFORM ATOM_SHADER_INCLUDES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/" OUTPUT_VARIABLE ATOM_SHADER_INCLUDE_PATHS)

get_property(ATOM_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(ATOM_MULTI_CONFIG)
    set(ATOM_SHADER_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/shaders")
else()
    set(ATOM_SHADER_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")
endif()
set(ATOM_SHADER_OUTPUTS)

foreach(source IN LISTS ATOM_SHADER_SOURCES)
    string(REGEX REPLACE "\.hlsl$" "" name "${source}")

    if(name MATCHES "\.vert$")
        set(profile vs_6_0)
    elseif(name MATCHES "\.frag$")
        set(profile ps_6_0)
    else()
        message(FATAL_ERROR "Unknown shader stage for ${source}")
    endif()

    set(output "${ATOM_SHADER_OUTPUT_DIR}/${name}.dxil")

    add_custom_command(
        OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ATOM_SHADER_OUTPUT_DIR}"
        COMMAND "${ATOM_DXC}"
            -nologo -T ${profile} -E main
            $<$<CONFIG:Debug>:-Zi> $<$<CONFIG:Debug>:-Qembed_debug> $<$<CONFIG:Debug>:-Od>
            -Fo "${output}"
            "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${source}" ${ATOM_SHADER_INCLUDE_PATHS}
        COMMENT "Compiling shader ${source}"
        VERBATIM
    )

    list(APPEND ATOM_SHADER_OUTPUTS "${output}")
endforeach()

# Listed for IDE browsing only; keep Visual Studio's built-in FXC off them.
set_source_files_properties(
    ${ATOM_SHADER_SOURCES}
    ${ATOM_SHADER_INCLUDES}
    PROPERTIES
        VS_TOOL_OVERRIDE "None"
)

add_custom_target(
    AtomShaders
    DEPENDS ${ATOM_SHADER_OUTPUTS}
    SOURCES ${ATOM_SHADER_SOURCES} ${ATOM_SHADER_INCLUDES}
)
