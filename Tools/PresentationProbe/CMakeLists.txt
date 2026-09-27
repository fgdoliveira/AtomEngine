add_executable(
    PresentationProbe
    Main.cpp
    Probe.h
    SdlProbe.cpp
    D3D12Probe.cpp
)

target_link_libraries(
    PresentationProbe
    PRIVATE
        SDL3::SDL3
        d3d12
        dxgi
        dxguid
        user32
)

target_compile_features(PresentationProbe PRIVATE cxx_std_20)

set_target_properties(
    PresentationProbe
    PROPERTIES
        FOLDER "Diagnostic Tools"
)
