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

# Every swapchain kind on every adapter, D3D11 and D3D12, no SDL: the
# hybrid-laptop reproduction behind ADR-006.
add_executable(SwapchainMatrix SwapchainMatrix.cpp)
target_link_libraries(SwapchainMatrix PRIVATE d3d11 d3d12 dxgi user32)
target_compile_features(SwapchainMatrix PRIVATE cxx_std_20)
set_target_properties(SwapchainMatrix PROPERTIES FOLDER "Diagnostic Tools")
