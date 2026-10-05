// SwapchainMatrix: can each adapter create, and present, a swapchain for a
// window on this screen? D3D11 and D3D12, flip and bitblt models, 2 or 3
// buffers - no SDL, no engine. The minimal reproduction for ADR-006: on
// the development laptop (muxless Optimus, HP), every RTX 4060 line fails
// with 0x887A0005 (DXGI_ERROR_DEVICE_REMOVED) and every Iris Xe line works.
//
//   SwapchainMatrix.exe          (LUID_ONLY=1: just list the adapters and
//                                 their LUIDs, as GPU performance counters
//                                 name them)
#include <windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdio>

using Microsoft::WRL::ComPtr;

static HWND MakeWindow()
{
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"swaptest";
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowW(L"swaptest", L"swaptest", WS_OVERLAPPEDWINDOW, 100, 100, 640, 360, nullptr, nullptr,
                              wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    return hwnd;
}

static DXGI_SWAP_CHAIN_DESC1 Desc(DXGI_FORMAT format, UINT buffers, DXGI_SWAP_EFFECT effect)
{
    DXGI_SWAP_CHAIN_DESC1 d{};
    d.Width = 640;
    d.Height = 360;
    d.Format = format;
    d.SampleDesc.Count = 1;
    d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    d.BufferCount = buffers;
    d.SwapEffect = effect;
    return d;
}

int main()
{
    ComPtr<IDXGIFactory6> factory;
    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)))) { std::puts("no factory"); return 1; }

    const DXGI_GPU_PREFERENCE prefs[] = { DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, DXGI_GPU_PREFERENCE_MINIMUM_POWER };
    for (DXGI_GPU_PREFERENCE pref : prefs)
    {
        ComPtr<IDXGIAdapter1> adapter;
        factory->EnumAdapterByGpuPreference(0, pref, IID_PPV_ARGS(&adapter));
        DXGI_ADAPTER_DESC1 ad{};
        adapter->GetDesc1(&ad);
        std::printf("=== %ls luid 0x%08lX_0x%08lX\n", ad.Description, static_cast<unsigned long>(ad.AdapterLuid.HighPart),
                    static_cast<unsigned long>(ad.AdapterLuid.LowPart));
        if (GetEnvironmentVariableW(L"LUID_ONLY", nullptr, 0)) continue;

        struct Variant { const char* name; DXGI_FORMAT format; UINT buffers; DXGI_SWAP_EFFECT effect; };
        const Variant variants[] = {
            { "BGRA8 x2 flip_discard", DXGI_FORMAT_B8G8R8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD },
            { "BGRA8 x3 flip_discard", DXGI_FORMAT_B8G8R8A8_UNORM, 3, DXGI_SWAP_EFFECT_FLIP_DISCARD },
            { "RGBA8 x2 flip_sequential", DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL },
            // The legacy bitblt model (D3D11 only; D3D12 requires flip).
            { "RGBA8 x1 blt_discard", DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD },
            { "RGBA8 x1 blt_sequential", DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_SEQUENTIAL },
        };

        // D3D11: the swapchain is created on the device.
        {
            ComPtr<ID3D11Device> device;
            HRESULT hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, nullptr, 0,
                                           D3D11_SDK_VERSION, &device, nullptr, nullptr);
            std::printf("d3d11 device: 0x%08lX\n", static_cast<unsigned long>(hr));
            for (const Variant& v : variants)
            {
                if (!device) break;
                HWND hwnd = MakeWindow();
                ComPtr<IDXGISwapChain1> swap;
                const DXGI_SWAP_CHAIN_DESC1 d = Desc(v.format, v.buffers, v.effect);
                hr = factory->CreateSwapChainForHwnd(device.Get(), hwnd, &d, nullptr, nullptr, &swap);
                std::printf("  d3d11 %-26s 0x%08lX\n", v.name, static_cast<unsigned long>(hr));
                if (swap) { hr = swap->Present(0, 0); std::printf("    present 0x%08lX\n", static_cast<unsigned long>(hr)); }
                swap.Reset();
                DestroyWindow(hwnd);
            }
        }
        // D3D12: the swapchain is created on the command queue.
        {
            ComPtr<ID3D12Device> device;
            HRESULT hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
            std::printf("d3d12 device: 0x%08lX\n", static_cast<unsigned long>(hr));
            for (const Variant& v : variants)
            {
                if (!device) break;
                D3D12_COMMAND_QUEUE_DESC qd{};
                ComPtr<ID3D12CommandQueue> queue;
                device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));
                HWND hwnd = MakeWindow();
                ComPtr<IDXGISwapChain1> swap;
                const DXGI_SWAP_CHAIN_DESC1 d = Desc(v.format, v.buffers, v.effect);
                hr = factory->CreateSwapChainForHwnd(queue.Get(), hwnd, &d, nullptr, nullptr, &swap);
                std::printf("  d3d12 %-26s 0x%08lX", v.name, static_cast<unsigned long>(hr));
                if (FAILED(hr)) std::printf("  removed-reason 0x%08lX", static_cast<unsigned long>(device->GetDeviceRemovedReason()));
                std::printf("\n");
                swap.Reset();
                DestroyWindow(hwnd);
                if (FAILED(device->GetDeviceRemovedReason())) { std::puts("  (device removed: stopping d3d12 variants)"); break; }
            }
        }
    }
    return 0;
}
