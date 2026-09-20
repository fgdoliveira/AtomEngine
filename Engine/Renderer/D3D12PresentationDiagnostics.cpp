#include "Renderer/D3D12PresentationDiagnostics.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <wrl/client.h>
#endif

namespace Atom
{
    namespace
    {
        std::string Sanitize(std::string value)
        {
            std::replace(value.begin(), value.end(), '\r', ' ');
            std::replace(value.begin(), value.end(), '\n', ' ');
            std::replace(value.begin(), value.end(), '"', '\'');
            return value;
        }

#if defined(_WIN32)
        std::string ToUtf8(const wchar_t* value)
        {
            if (!value || value[0] == L'\0')
            {
                return {};
            }

            const int required = WideCharToMultiByte(
                CP_UTF8,
                0,
                value,
                -1,
                nullptr,
                0,
                nullptr,
                nullptr
            );

            if (required <= 1)
            {
                return {};
            }

            std::string result(static_cast<size_t>(required), '\0');
            WideCharToMultiByte(
                CP_UTF8,
                0,
                value,
                -1,
                result.data(),
                required,
                nullptr,
                nullptr
            );
            result.pop_back();
            return result;
        }

        std::string HexResult(HRESULT result)
        {
            std::ostringstream text;
            text
                << "0x"
                << std::hex
                << std::uppercase
                << std::setw(8)
                << std::setfill('0')
                << static_cast<std::uint32_t>(result);
            return text.str();
        }

        std::string LuidText(const LUID& luid)
        {
            std::ostringstream text;
            text
                << std::hex
                << std::uppercase
                << static_cast<std::uint32_t>(luid.HighPart)
                << ':'
                << static_cast<std::uint32_t>(luid.LowPart);
            return text.str();
        }
#endif
    }

    struct D3D12PresentationDiagnostics::Implementation
    {
        SDL_Window* window = nullptr;
        std::string topologySignature;

#if defined(_WIN32)
        Microsoft::WRL::ComPtr<IDXGIInfoQueue> infoQueue;
        UINT64 nextDxgiMessage = 0;
#endif
    };

    D3D12PresentationDiagnostics::D3D12PresentationDiagnostics()
        : m_implementation(std::make_unique<Implementation>())
    {
    }

    D3D12PresentationDiagnostics::~D3D12PresentationDiagnostics() = default;

    void D3D12PresentationDiagnostics::Initialize(
        SDL_Window* window,
        const LogFunction& log
    )
    {
        m_implementation->window = window;
        m_implementation->topologySignature.clear();

#if defined(_WIN32)
        const HRESULT result = DXGIGetDebugInterface1(
            0,
            IID_PPV_ARGS(&m_implementation->infoQueue)
        );

        if (SUCCEEDED(result))
        {
            m_implementation->nextDxgiMessage =
                m_implementation->infoQueue
                    ->GetNumStoredMessagesAllowedByRetrievalFilters(
                        DXGI_DEBUG_ALL
                    );
            log(
                "dxgi_info_queue_ready baseline=" +
                std::to_string(m_implementation->nextDxgiMessage)
            );
        }
        else
        {
            log(
                "dxgi_info_queue_unavailable hresult=" + HexResult(result)
            );
        }
#else
        log("presentation_diagnostics_unavailable platform=non_windows");
#endif

        CaptureTopology("initial", true, log);
    }

    void D3D12PresentationDiagnostics::CaptureTopology(
        const char* stage,
        bool force,
        const LogFunction& log
    )
    {
#if defined(_WIN32)
        SDL_Window* const window = m_implementation->window;
        if (!window)
        {
            log(std::string("display_topology stage=") + stage + " window=null");
            return;
        }

        const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
        HWND const hwnd = static_cast<HWND>(SDL_GetPointerProperty(
            properties,
            SDL_PROP_WINDOW_WIN32_HWND_POINTER,
            nullptr
        ));

        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        SDL_GetWindowPosition(window, &x, &y);
        SDL_GetWindowSizeInPixels(window, &width, &height);
        const SDL_DisplayID display = SDL_GetDisplayForWindow(window);
        HMONITOR const monitor = hwnd
            ? MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST)
            : nullptr;

        std::ostringstream signature;
        signature
            << x << ',' << y << ',' << width << ',' << height
            << ',' << display
            << ',' << reinterpret_cast<std::uintptr_t>(monitor);

        if (!force && signature.str() == m_implementation->topologySignature)
        {
            return;
        }
        m_implementation->topologySignature = signature.str();

        RECT windowRect{};
        RECT clientRect{};
        const bool hasWindowRect = hwnd && GetWindowRect(hwnd, &windowRect);
        const bool hasClientRect = hwnd && GetClientRect(hwnd, &clientRect);

        MONITORINFOEXW monitorInfo{};
        monitorInfo.cbSize = sizeof(monitorInfo);
        const bool hasMonitorInfo = monitor && GetMonitorInfoW(
            monitor,
            &monitorInfo
        );

        std::ostringstream windowEntry;
        windowEntry
            << "display_topology stage=" << stage
            << " hwnd=0x" << std::hex << std::uppercase
            << reinterpret_cast<std::uintptr_t>(hwnd) << std::dec
            << " hwnd_valid=" << (hwnd && IsWindow(hwnd) ? "true" : "false")
            << " sdl_position=" << x << ',' << y
            << " pixel_size=" << width << 'x' << height
            << " display_id=" << display
            << " display_scale=" << SDL_GetWindowDisplayScale(window)
            << " dpi=" << (hwnd ? GetDpiForWindow(hwnd) : 0)
            << " monitor=0x" << std::hex << std::uppercase
            << reinterpret_cast<std::uintptr_t>(monitor) << std::dec;

        if (hasWindowRect)
        {
            windowEntry
                << " window_rect="
                << windowRect.left << ',' << windowRect.top << ','
                << windowRect.right << ',' << windowRect.bottom;
        }
        if (hasClientRect)
        {
            windowEntry
                << " client_rect="
                << clientRect.left << ',' << clientRect.top << ','
                << clientRect.right << ',' << clientRect.bottom;
        }
        if (hasMonitorInfo)
        {
            windowEntry
                << " monitor_device=\""
                << Sanitize(ToUtf8(monitorInfo.szDevice)) << '"'
                << " monitor_rect="
                << monitorInfo.rcMonitor.left << ','
                << monitorInfo.rcMonitor.top << ','
                << monitorInfo.rcMonitor.right << ','
                << monitorInfo.rcMonitor.bottom;
        }
        log(windowEntry.str());

        UINT32 pathCount = 0;
        UINT32 modeCount = 0;
        LONG queryResult = GetDisplayConfigBufferSizes(
            QDC_ONLY_ACTIVE_PATHS,
            &pathCount,
            &modeCount
        );

        if (queryResult != ERROR_SUCCESS)
        {
            log(
                std::string("display_config_sizes_failed stage=") + stage +
                " win32_error=" + std::to_string(queryResult)
            );
            return;
        }

        std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
        std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
        do
        {
            queryResult = QueryDisplayConfig(
                QDC_ONLY_ACTIVE_PATHS,
                &pathCount,
                paths.data(),
                &modeCount,
                modes.data(),
                nullptr
            );
            if (queryResult == ERROR_INSUFFICIENT_BUFFER)
            {
                GetDisplayConfigBufferSizes(
                    QDC_ONLY_ACTIVE_PATHS,
                    &pathCount,
                    &modeCount
                );
                paths.resize(pathCount);
                modes.resize(modeCount);
            }
        } while (queryResult == ERROR_INSUFFICIENT_BUFFER);

        if (queryResult != ERROR_SUCCESS)
        {
            log(
                std::string("display_config_query_failed stage=") + stage +
                " win32_error=" + std::to_string(queryResult)
            );
            return;
        }

        paths.resize(pathCount);
        modes.resize(modeCount);
        const std::string windowMonitor = hasMonitorInfo
            ? ToUtf8(monitorInfo.szDevice)
            : std::string{};

        for (size_t index = 0; index < paths.size(); ++index)
        {
            const DISPLAYCONFIG_PATH_INFO& path = paths[index];

            DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName{};
            sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
            sourceName.header.size = sizeof(sourceName);
            sourceName.header.adapterId = path.sourceInfo.adapterId;
            sourceName.header.id = path.sourceInfo.id;
            const LONG sourceResult = DisplayConfigGetDeviceInfo(
                &sourceName.header
            );

            DISPLAYCONFIG_TARGET_DEVICE_NAME targetName{};
            targetName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
            targetName.header.size = sizeof(targetName);
            targetName.header.adapterId = path.targetInfo.adapterId;
            targetName.header.id = path.targetInfo.id;
            const LONG targetResult = DisplayConfigGetDeviceInfo(
                &targetName.header
            );

            DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO advancedColor{};
            advancedColor.header.type =
                DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
            advancedColor.header.size = sizeof(advancedColor);
            advancedColor.header.adapterId = path.targetInfo.adapterId;
            advancedColor.header.id = path.targetInfo.id;
            const LONG advancedColorResult = DisplayConfigGetDeviceInfo(
                &advancedColor.header
            );

            const std::string source = sourceResult == ERROR_SUCCESS
                ? ToUtf8(sourceName.viewGdiDeviceName)
                : std::string{};
            const std::string friendly = targetResult == ERROR_SUCCESS
                ? ToUtf8(targetName.monitorFriendlyDeviceName)
                : std::string{};

            const DISPLAYCONFIG_MODE_INFO* sourceMode = nullptr;
            if (
                path.sourceInfo.modeInfoIdx !=
                    DISPLAYCONFIG_PATH_MODE_IDX_INVALID &&
                path.sourceInfo.modeInfoIdx < modes.size()
            )
            {
                sourceMode = &modes[path.sourceInfo.modeInfoIdx];
            }

            const DISPLAYCONFIG_MODE_INFO* targetMode = nullptr;
            if (
                path.targetInfo.modeInfoIdx !=
                    DISPLAYCONFIG_PATH_MODE_IDX_INVALID &&
                path.targetInfo.modeInfoIdx < modes.size()
            )
            {
                targetMode = &modes[path.targetInfo.modeInfoIdx];
            }

            std::ostringstream pathEntry;
            pathEntry
                << "display_path stage=" << stage
                << " index=" << index
                << " adapter_luid=" << LuidText(path.sourceInfo.adapterId)
                << " source_id=" << path.sourceInfo.id
                << " target_id=" << path.targetInfo.id
                << " source_name=\"" << Sanitize(source) << '"'
                << " target_name=\"" << Sanitize(friendly) << '"'
                << " is_window_monitor="
                << (!source.empty() && source == windowMonitor ? "true" : "false")
                << " output_technology="
                << static_cast<int>(path.targetInfo.outputTechnology)
                << " rotation=" << static_cast<int>(path.targetInfo.rotation)
                << " scaling=" << static_cast<int>(path.targetInfo.scaling)
                << " refresh=" << path.targetInfo.refreshRate.Numerator
                << '/' << path.targetInfo.refreshRate.Denominator
                << " target_available="
                << (path.targetInfo.targetAvailable ? "true" : "false")
                << " flags=" << path.flags
                << " source_query=" << sourceResult
                << " target_query=" << targetResult
                << " advanced_color_query=" << advancedColorResult;
            if (sourceMode && sourceMode->infoType == DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE)
            {
                pathEntry
                    << " desktop_position="
                    << sourceMode->sourceMode.position.x << ','
                    << sourceMode->sourceMode.position.y
                    << " source_mode="
                    << sourceMode->sourceMode.width << 'x'
                    << sourceMode->sourceMode.height;
            }
            if (targetMode && targetMode->infoType == DISPLAYCONFIG_MODE_INFO_TYPE_TARGET)
            {
                pathEntry
                    << " target_active="
                    << targetMode->targetMode.targetVideoSignalInfo.activeSize.cx
                    << 'x'
                    << targetMode->targetMode.targetVideoSignalInfo.activeSize.cy
                    << " target_total="
                    << targetMode->targetMode.targetVideoSignalInfo.totalSize.cx
                    << 'x'
                    << targetMode->targetMode.targetVideoSignalInfo.totalSize.cy;
            }
            if (advancedColorResult == ERROR_SUCCESS)
            {
                pathEntry
                    << " advanced_color_supported="
                    << (advancedColor.advancedColorSupported ? "true" : "false")
                    << " advanced_color_enabled="
                    << (advancedColor.advancedColorEnabled ? "true" : "false")
                    << " wide_color_enforced="
                    << (advancedColor.wideColorEnforced ? "true" : "false")
                    << " advanced_color_force_disabled="
                    << (advancedColor.advancedColorForceDisabled
                        ? "true"
                        : "false")
                    << " color_encoding="
                    << static_cast<int>(advancedColor.colorEncoding)
                    << " bits_per_color_channel="
                    << advancedColor.bitsPerColorChannel;
            }
            log(pathEntry.str());
        }
#else
        static_cast<void>(stage);
        static_cast<void>(force);
        static_cast<void>(log);
#endif
    }

    void D3D12PresentationDiagnostics::DrainDxgiMessages(
        const char* stage,
        const LogFunction& log
    )
    {
#if defined(_WIN32)
        if (!m_implementation->infoQueue)
        {
            return;
        }

        const UINT64 count = m_implementation->infoQueue
            ->GetNumStoredMessagesAllowedByRetrievalFilters(DXGI_DEBUG_ALL);
        if (count < m_implementation->nextDxgiMessage)
        {
            m_implementation->nextDxgiMessage = 0;
        }

        for (
            UINT64 index = m_implementation->nextDxgiMessage;
            index < count;
            ++index
        )
        {
            SIZE_T messageSize = 0;
            HRESULT result = m_implementation->infoQueue->GetMessage(
                DXGI_DEBUG_ALL,
                index,
                nullptr,
                &messageSize
            );
            if (FAILED(result))
            {
                log(
                    std::string("dxgi_message_size_failed stage=") + stage +
                    " index=" + std::to_string(index) +
                    " hresult=" + HexResult(result)
                );
                continue;
            }

            std::vector<std::uint8_t> storage(messageSize);
            auto* message = reinterpret_cast<DXGI_INFO_QUEUE_MESSAGE*>(
                storage.data()
            );
            result = m_implementation->infoQueue->GetMessage(
                DXGI_DEBUG_ALL,
                index,
                message,
                &messageSize
            );
            if (FAILED(result))
            {
                log(
                    std::string("dxgi_message_read_failed stage=") + stage +
                    " index=" + std::to_string(index) +
                    " hresult=" + HexResult(result)
                );
                continue;
            }

            log(
                std::string("dxgi_message stage=") + stage +
                " index=" + std::to_string(index) +
                " category=" + std::to_string(message->Category) +
                " severity=" + std::to_string(message->Severity) +
                " id=" + std::to_string(message->ID) +
                " description=\"" +
                Sanitize(message->pDescription ? message->pDescription : "") +
                "\""
            );
        }

        m_implementation->nextDxgiMessage = count;
#else
        static_cast<void>(stage);
        static_cast<void>(log);
#endif
    }

    void D3D12PresentationDiagnostics::Shutdown(const LogFunction& log)
    {
        DrainDxgiMessages("diagnostics_shutdown", log);
#if defined(_WIN32)
        m_implementation->infoQueue.Reset();
        m_implementation->nextDxgiMessage = 0;
#endif
        m_implementation->window = nullptr;
        m_implementation->topologySignature.clear();
    }
}
