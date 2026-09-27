#include "Probe.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace PresentationProbe
{
    namespace
    {
        using Microsoft::WRL::ComPtr;

        constexpr UINT BufferCount = 2;
        constexpr DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        constexpr wchar_t WindowClassName[] = L"AtomPresentationProbeWindow";

        bool replacingWindow = false;

        std::string Hex(HRESULT result)
        {
            std::ostringstream text;
            text
                << "0x" << std::hex << std::uppercase << std::setw(8)
                << std::setfill('0') << static_cast<std::uint32_t>(result);
            return text.str();
        }

        LRESULT CALLBACK WindowProcedure(
            HWND window,
            UINT message,
            WPARAM wParam,
            LPARAM lParam
        )
        {
            if (message == WM_CLOSE)
            {
                DestroyWindow(window);
                return 0;
            }
            if (message == WM_DESTROY && !replacingWindow)
            {
                PostQuitMessage(0);
                return 0;
            }
            return DefWindowProcW(window, message, wParam, lParam);
        }

        class D3D12Probe
        {
        public:
            D3D12Probe(const Options& options, Log& log)
                : m_options(options), m_log(log)
            {
            }

            int Run()
            {
                if (!Initialize())
                {
                    Shutdown();
                    return 1;
                }

                bool running = true;
                std::uint64_t renderedFrames = 0;
                int exitCode = 0;
                while (running)
                {
                    MSG message{};
                    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
                    {
                        if (message.message == WM_QUIT)
                        {
                            running = false;
                            break;
                        }
                        TranslateMessage(&message);
                        DispatchMessageW(&message);
                    }
                    if (!running)
                    {
                        break;
                    }

                    const HRESULT result = Render();
                    if (FAILED(result))
                    {
                        const HRESULT removed = m_device
                            ? m_device->GetDeviceRemovedReason()
                            : E_POINTER;
                        m_log.Write(
                            "render_failure hresult=" + Hex(result) +
                            " device_removed_reason=" + Hex(removed)
                        );

                        if (
                            m_options.tryNewWindowAfterFailure &&
                            !m_newWindowAttempted
                        )
                        {
                            m_newWindowAttempted = true;
                            if (TryFreshWindow())
                            {
                                m_validatingNewWindow = true;
                                continue;
                            }
                        }

                        m_log.Write("terminal_failure=true");
                        exitCode = 1;
                        break;
                    }

                    if (m_validatingNewWindow)
                    {
                        m_log.Write(
                            "fresh_hwnd_frame_presented validation_succeeded=true"
                        );
                        m_validatingNewWindow = false;
                    }
                    ++renderedFrames;
                    if (m_options.maxFrames && renderedFrames >= m_options.maxFrames)
                    {
                        running = false;
                    }
                }

                Shutdown();
                return exitCode;
            }

        private:
            bool Initialize()
            {
#ifndef NDEBUG
                ComPtr<ID3D12Debug> debugController;
                if (SUCCEEDED(D3D12GetDebugInterface(
                    IID_PPV_ARGS(&debugController)
                )))
                {
                    debugController->EnableDebugLayer();
                    m_factoryFlags = DXGI_CREATE_FACTORY_DEBUG;
                    m_log.Write("d3d12_debug_layer_enabled=true");
                }
#endif

                HRESULT result = CreateDXGIFactory2(
                    m_factoryFlags,
                    IID_PPV_ARGS(&m_factory)
                );
                if (FAILED(result))
                {
                    m_log.Write("factory_create_failed hresult=" + Hex(result));
                    return false;
                }

                ComPtr<IDXGIAdapter1> adapter;
                for (UINT index = 0; ; ++index)
                {
                    result = m_factory->EnumAdapterByGpuPreference(
                        index,
                        DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                        IID_PPV_ARGS(&adapter)
                    );
                    if (result == DXGI_ERROR_NOT_FOUND)
                    {
                        break;
                    }
                    if (FAILED(result))
                    {
                        continue;
                    }

                    DXGI_ADAPTER_DESC1 description{};
                    adapter->GetDesc1(&description);
                    std::ostringstream candidate;
                    candidate
                        << "adapter_candidate preference_index=" << index
                        << " luid=" << std::hex << std::uppercase
                        << static_cast<std::uint32_t>(
                            description.AdapterLuid.HighPart
                        )
                        << ':'
                        << static_cast<std::uint32_t>(
                            description.AdapterLuid.LowPart
                        );
                    m_log.Write(candidate.str());

                    const bool requested =
                        m_options.adapterIndex ==
                            (std::numeric_limits<std::uint32_t>::max)() ||
                        m_options.adapterIndex == index;
                    if (requested &&
                        !(description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) &&
                        SUCCEEDED(D3D12CreateDevice(
                            adapter.Get(),
                            D3D_FEATURE_LEVEL_11_0,
                            __uuidof(ID3D12Device),
                            nullptr
                        )))
                    {
                        break;
                    }
                    adapter.Reset();
                }

                if (!adapter)
                {
                    m_log.Write("hardware_adapter_not_found=true");
                    return false;
                }

                DXGI_ADAPTER_DESC1 adapterDescription{};
                adapter->GetDesc1(&adapterDescription);
                std::ostringstream adapterEntry;
                adapterEntry
                    << "adapter_selected luid=" << std::hex << std::uppercase
                    << static_cast<std::uint32_t>(adapterDescription.AdapterLuid.HighPart)
                    << ':'
                    << static_cast<std::uint32_t>(adapterDescription.AdapterLuid.LowPart);
                m_log.Write(adapterEntry.str());

                result = D3D12CreateDevice(
                    adapter.Get(),
                    D3D_FEATURE_LEVEL_11_0,
                    IID_PPV_ARGS(&m_device)
                );
                if (FAILED(result))
                {
                    m_log.Write("device_create_failed hresult=" + Hex(result));
                    return false;
                }

                D3D12_COMMAND_QUEUE_DESC queueDescription{};
                queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
                result = m_device->CreateCommandQueue(
                    &queueDescription,
                    IID_PPV_ARGS(&m_queue)
                );
                if (FAILED(result))
                {
                    m_log.Write("queue_create_failed hresult=" + Hex(result));
                    return false;
                }

                for (auto& allocator : m_allocators)
                {
                    result = m_device->CreateCommandAllocator(
                        D3D12_COMMAND_LIST_TYPE_DIRECT,
                        IID_PPV_ARGS(&allocator)
                    );
                    if (FAILED(result))
                    {
                        m_log.Write(
                            "command_allocator_create_failed hresult=" +
                            Hex(result)
                        );
                        return false;
                    }
                }

                result = m_device->CreateCommandList(
                    0,
                    D3D12_COMMAND_LIST_TYPE_DIRECT,
                    m_allocators[0].Get(),
                    nullptr,
                    IID_PPV_ARGS(&m_commandList)
                );
                if (FAILED(result))
                {
                    m_log.Write(
                        "command_list_create_failed hresult=" + Hex(result)
                    );
                    return false;
                }
                result = m_commandList->Close();
                if (FAILED(result))
                {
                    m_log.Write(
                        "command_list_initial_close_failed hresult=" +
                        Hex(result)
                    );
                    return false;
                }

                result = m_device->CreateFence(
                    0,
                    D3D12_FENCE_FLAG_NONE,
                    IID_PPV_ARGS(&m_fence)
                );
                if (FAILED(result))
                {
                    m_log.Write("fence_create_failed hresult=" + Hex(result));
                    return false;
                }
                m_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
                if (!m_fenceEvent)
                {
                    m_log.Write(
                        "fence_event_create_failed win32_error=" +
                        std::to_string(GetLastError())
                    );
                    return false;
                }

                WNDCLASSEXW windowClass{};
                windowClass.cbSize = sizeof(windowClass);
                windowClass.hInstance = GetModuleHandleW(nullptr);
                windowClass.lpfnWndProc = WindowProcedure;
                windowClass.lpszClassName = WindowClassName;
                windowClass.hCursor = LoadCursorW(
                    nullptr,
                    MAKEINTRESOURCEW(32512)
                );
                if (!RegisterClassExW(&windowClass) &&
                    GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
                {
                    m_log.Write(
                        "window_class_register_failed win32_error=" +
                        std::to_string(GetLastError())
                    );
                    return false;
                }

                m_window = CreateProbeWindow(
                    m_options.initialX.value_or(CW_USEDEFAULT),
                    m_options.initialY.value_or(CW_USEDEFAULT),
                    1280,
                    720
                );
                if (!m_window)
                {
                    return false;
                }

                if (!CreateSwapchainAndTargets("initial"))
                {
                    return false;
                }

                m_log.Write(
                    "initialization_succeeded format=R8G8B8A8_UNORM "
                    "buffers=2 swap_effect=flip_discard scaling=none vsync=true"
                );
                return true;
            }

            HWND CreateProbeWindow(int x, int y, int width, int height)
            {
                RECT rectangle{0, 0, width, height};
                AdjustWindowRect(&rectangle, WS_OVERLAPPEDWINDOW, FALSE);
                HWND window = CreateWindowExW(
                    0,
                    WindowClassName,
                    L"Raw D3D12 Presentation Probe",
                    WS_OVERLAPPEDWINDOW,
                    x,
                    y,
                    rectangle.right - rectangle.left,
                    rectangle.bottom - rectangle.top,
                    nullptr,
                    nullptr,
                    GetModuleHandleW(nullptr),
                    nullptr
                );
                if (!window)
                {
                    m_log.Write(
                        "window_create_failed win32_error=" +
                        std::to_string(GetLastError())
                    );
                    return nullptr;
                }
                ShowWindow(window, SW_SHOW);
                UpdateWindow(window);
                return window;
            }

            bool CreateSwapchainAndTargets(const char* stage)
            {
                BOOL allowTearing = FALSE;
                ComPtr<IDXGIFactory5> factory5;
                if (SUCCEEDED(m_factory.As(&factory5)))
                {
                    factory5->CheckFeatureSupport(
                        DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                        &allowTearing,
                        sizeof(allowTearing)
                    );
                }
                m_swapchainFlags = allowTearing
                    ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
                    : 0;

                DXGI_SWAP_CHAIN_DESC1 description{};
                description.Format = BackBufferFormat;
                description.SampleDesc.Count = 1;
                description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                description.BufferCount = BufferCount;
                description.Scaling = DXGI_SCALING_NONE;
                description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
                description.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
                description.Flags = m_swapchainFlags;

                DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen{};
                fullscreen.Windowed = TRUE;

                ComPtr<IDXGISwapChain1> swapchain;
                const HRESULT result = m_factory->CreateSwapChainForHwnd(
                    m_queue.Get(),
                    m_window,
                    &description,
                    &fullscreen,
                    nullptr,
                    &swapchain
                );
                m_log.Write(
                    std::string(stage) +
                    " create_swapchain_for_hwnd hresult=" + Hex(result) +
                    " allow_tearing=" + (allowTearing ? "true" : "false")
                );
                if (FAILED(result) || FAILED(swapchain.As(&m_swapchain)))
                {
                    return false;
                }

                m_factory->MakeWindowAssociation(
                    m_window,
                    DXGI_MWA_NO_ALT_ENTER
                );

                D3D12_DESCRIPTOR_HEAP_DESC heapDescription{};
                heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
                heapDescription.NumDescriptors = BufferCount;
                HRESULT heapResult = m_device->CreateDescriptorHeap(
                    &heapDescription,
                    IID_PPV_ARGS(&m_rtvHeap)
                );
                if (FAILED(heapResult))
                {
                    m_log.Write(
                        std::string(stage) +
                        " rtv_heap_create_failed hresult=" + Hex(heapResult)
                    );
                    return false;
                }
                m_rtvIncrement = m_device->GetDescriptorHandleIncrementSize(
                    D3D12_DESCRIPTOR_HEAP_TYPE_RTV
                );

                RECT client{};
                GetClientRect(m_window, &client);
                m_width = static_cast<UINT>(client.right - client.left);
                m_height = static_cast<UINT>(client.bottom - client.top);
                return CreateRenderTargets(stage);
            }

            bool CreateRenderTargets(const char* stage)
            {
                D3D12_CPU_DESCRIPTOR_HANDLE handle =
                    m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
                for (UINT index = 0; index < BufferCount; ++index)
                {
                    const HRESULT result = m_swapchain->GetBuffer(
                        index,
                        IID_PPV_ARGS(&m_backBuffers[index])
                    );
                    if (FAILED(result))
                    {
                        m_log.Write(
                            std::string(stage) + " get_buffer index=" +
                            std::to_string(index) + " hresult=" + Hex(result)
                        );
                        return false;
                    }
                    m_device->CreateRenderTargetView(
                        m_backBuffers[index].Get(),
                        nullptr,
                        handle
                    );
                    handle.ptr += m_rtvIncrement;
                }
                return true;
            }

            HRESULT Render()
            {
                RECT client{};
                GetClientRect(m_window, &client);
                const UINT width = static_cast<UINT>(client.right - client.left);
                const UINT height = static_cast<UINT>(client.bottom - client.top);
                if (width == 0 || height == 0)
                {
                    Sleep(10);
                    return S_OK;
                }

                if (width != m_width || height != m_height)
                {
                    HRESULT result = WaitForGpu();
                    if (FAILED(result))
                    {
                        return result;
                    }
                    for (auto& buffer : m_backBuffers)
                    {
                        buffer.Reset();
                    }
                    result = m_swapchain->ResizeBuffers(
                        BufferCount,
                        width,
                        height,
                        BackBufferFormat,
                        m_swapchainFlags
                    );
                    m_log.Write(
                        "resize_buffers size=" + std::to_string(width) + 'x' +
                        std::to_string(height) + " hresult=" + Hex(result)
                    );
                    if (FAILED(result))
                    {
                        return result;
                    }
                    m_width = width;
                    m_height = height;
                    if (!CreateRenderTargets("resize"))
                    {
                        return E_FAIL;
                    }
                }

                const UINT index = m_swapchain->GetCurrentBackBufferIndex();
                HRESULT result = m_allocators[index]->Reset();
                if (FAILED(result))
                {
                    return result;
                }
                result = m_commandList->Reset(m_allocators[index].Get(), nullptr);
                if (FAILED(result))
                {
                    return result;
                }

                D3D12_RESOURCE_BARRIER toTarget{};
                toTarget.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                toTarget.Transition.pResource = m_backBuffers[index].Get();
                toTarget.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
                toTarget.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
                toTarget.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                m_commandList->ResourceBarrier(1, &toTarget);

                D3D12_CPU_DESCRIPTOR_HANDLE handle =
                    m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
                handle.ptr += static_cast<SIZE_T>(index) * m_rtvIncrement;
                constexpr float color[] = {0.1f, 0.15f, 0.2f, 1.0f};
                m_commandList->ClearRenderTargetView(handle, color, 0, nullptr);

                std::swap(
                    toTarget.Transition.StateBefore,
                    toTarget.Transition.StateAfter
                );
                m_commandList->ResourceBarrier(1, &toTarget);
                result = m_commandList->Close();
                if (FAILED(result))
                {
                    return result;
                }

                ID3D12CommandList* lists[] = {m_commandList.Get()};
                m_queue->ExecuteCommandLists(1, lists);
                result = m_swapchain->Present(1, 0);
                if (FAILED(result))
                {
                    m_log.Write("present hresult=" + Hex(result));
                    return result;
                }
                return WaitForGpu();
            }

            HRESULT WaitForGpu()
            {
                const UINT64 value = ++m_fenceValue;
                HRESULT result = m_queue->Signal(m_fence.Get(), value);
                if (FAILED(result))
                {
                    return result;
                }
                if (m_fence->GetCompletedValue() < value)
                {
                    result = m_fence->SetEventOnCompletion(value, m_fenceEvent);
                    if (FAILED(result))
                    {
                        return result;
                    }
                    const DWORD waitResult = WaitForSingleObject(
                        m_fenceEvent,
                        10000
                    );
                    if (waitResult != WAIT_OBJECT_0)
                    {
                        return waitResult == WAIT_TIMEOUT
                            ? HRESULT_FROM_WIN32(ERROR_TIMEOUT)
                            : HRESULT_FROM_WIN32(GetLastError());
                    }
                }
                return S_OK;
            }

            bool TryFreshWindow()
            {
                RECT oldRectangle{};
                GetWindowRect(m_window, &oldRectangle);
                RECT oldClient{};
                GetClientRect(m_window, &oldClient);
                const int width = oldClient.right - oldClient.left;
                const int height = oldClient.bottom - oldClient.top;
                m_log.Write(
                    "fresh_hwnd_attempt_started x=" +
                    std::to_string(oldRectangle.left) + " y=" +
                    std::to_string(oldRectangle.top) + " size=" +
                    std::to_string(width) + 'x' + std::to_string(height)
                );

                const HRESULT waitResult = WaitForGpu();
                if (FAILED(waitResult))
                {
                    m_log.Write(
                        "fresh_hwnd_gpu_wait_failed hresult=" + Hex(waitResult)
                    );
                    return false;
                }

                for (auto& buffer : m_backBuffers)
                {
                    buffer.Reset();
                }
                m_rtvHeap.Reset();
                m_swapchain.Reset();

                replacingWindow = true;
                DestroyWindow(m_window);
                replacingWindow = false;
                m_window = CreateProbeWindow(
                    oldRectangle.left,
                    oldRectangle.top,
                    width,
                    height
                );
                if (!m_window)
                {
                    return false;
                }

                const bool created = CreateSwapchainAndTargets("fresh_hwnd");
                m_log.Write(
                    std::string("fresh_hwnd_attempt_") +
                    (created ? "succeeded" : "failed")
                );
                return created;
            }

            void Shutdown()
            {
                if (m_queue && m_fence && m_fenceEvent && m_device &&
                    SUCCEEDED(m_device->GetDeviceRemovedReason()))
                {
                    WaitForGpu();
                }
                for (auto& buffer : m_backBuffers)
                {
                    buffer.Reset();
                }
                m_swapchain.Reset();
                m_rtvHeap.Reset();
                m_commandList.Reset();
                for (auto& allocator : m_allocators)
                {
                    allocator.Reset();
                }
                m_fence.Reset();
                m_queue.Reset();
                m_device.Reset();
                m_factory.Reset();
                if (m_fenceEvent)
                {
                    CloseHandle(m_fenceEvent);
                    m_fenceEvent = nullptr;
                }
                if (m_window)
                {
                    replacingWindow = true;
                    DestroyWindow(m_window);
                    replacingWindow = false;
                    m_window = nullptr;
                }
                UnregisterClassW(WindowClassName, GetModuleHandleW(nullptr));
                m_log.Write("session_end api=d3d12");
            }

            const Options& m_options;
            Log& m_log;
            UINT m_factoryFlags = 0;
            UINT m_swapchainFlags = 0;
            UINT m_rtvIncrement = 0;
            UINT m_width = 0;
            UINT m_height = 0;
            UINT64 m_fenceValue = 0;
            bool m_newWindowAttempted = false;
            bool m_validatingNewWindow = false;
            HWND m_window = nullptr;
            HANDLE m_fenceEvent = nullptr;
            ComPtr<IDXGIFactory6> m_factory;
            ComPtr<ID3D12Device> m_device;
            ComPtr<ID3D12CommandQueue> m_queue;
            ComPtr<IDXGISwapChain3> m_swapchain;
            ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
            std::array<ComPtr<ID3D12Resource>, BufferCount> m_backBuffers;
            std::array<ComPtr<ID3D12CommandAllocator>, BufferCount> m_allocators;
            ComPtr<ID3D12GraphicsCommandList> m_commandList;
            ComPtr<ID3D12Fence> m_fence;
        };
    }

    int RunD3D12(const Options& options)
    {
        Log log(options.logPath);
        log.Write(
            "session_start api=d3d12 new_window_after_failure=" +
            std::string(options.tryNewWindowAfterFailure ? "true" : "false")
        );
        D3D12Probe probe(options, log);
        return probe.Run();
    }
}
