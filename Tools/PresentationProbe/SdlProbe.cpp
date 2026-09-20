#include "Probe.h"

#include <SDL3/SDL.h>

#include <sstream>
#include <string>

namespace PresentationProbe
{
    namespace
    {
        SDL_Window* CreateWindowAt(int x, int y, int width, int height)
        {
            SDL_Window* window = SDL_CreateWindow(
                "SDL GPU Presentation Probe",
                width,
                height,
                SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN
            );
            if (window)
            {
                SDL_SetWindowPosition(window, x, y);
                SDL_ShowWindow(window);
            }
            return window;
        }

        bool ClaimSdrVsync(
            SDL_GPUDevice* device,
            SDL_Window* window,
            Log& log,
            const char* stage
        )
        {
            if (!SDL_ClaimWindowForGPUDevice(device, window))
            {
                log.Write(
                    std::string(stage) + " claim_failed error=\"" +
                    SDL_GetError() + "\""
                );
                return false;
            }

            if (!SDL_SetGPUSwapchainParameters(
                device,
                window,
                SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                SDL_GPU_PRESENTMODE_VSYNC
            ))
            {
                log.Write(
                    std::string(stage) + " parameter_set_failed error=\"" +
                    SDL_GetError() + "\""
                );
                SDL_ReleaseWindowFromGPUDevice(device, window);
                return false;
            }

            log.Write(std::string(stage) + " claim_succeeded sdr=true vsync=true");
            return true;
        }
    }

    int RunSdl(const Options& options)
    {
        Log log(options.logPath);
        log.Write(
            "session_start api=sdl new_window_after_failure=" +
            std::string(options.tryNewWindowAfterFailure ? "true" : "false")
        );

        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            log.Write(std::string("sdl_init_failed error=\"") + SDL_GetError() + '"');
            return 1;
        }

        SDL_Window* window = CreateWindowAt(
            options.initialX.value_or(SDL_WINDOWPOS_CENTERED),
            options.initialY.value_or(SDL_WINDOWPOS_CENTERED),
            1280,
            720
        );
        if (!window)
        {
            log.Write(std::string("window_create_failed error=\"") + SDL_GetError() + '"');
            SDL_Quit();
            return 1;
        }

#ifndef NDEBUG
        constexpr bool debug = true;
#else
        constexpr bool debug = false;
#endif
        const SDL_PropertiesID deviceProperties = SDL_CreateProperties();
        SDL_SetStringProperty(
            deviceProperties,
            SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING,
            "direct3d12"
        );
        SDL_SetBooleanProperty(
            deviceProperties,
            SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN,
            true
        );
        SDL_SetBooleanProperty(
            deviceProperties,
            SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN,
            debug
        );
        SDL_SetBooleanProperty(
            deviceProperties,
            SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOLEAN,
            options.preferLowPower
        );
        SDL_GPUDevice* device = SDL_CreateGPUDeviceWithProperties(
            deviceProperties
        );
        SDL_DestroyProperties(deviceProperties);
        if (!device)
        {
            log.Write(std::string("device_create_failed error=\"") + SDL_GetError() + '"');
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }

        log.Write(
            std::string("device_created backend=\"") +
            SDL_GetGPUDeviceDriver(device) + "\" adapter=\"" +
            SDL_GetStringProperty(
                SDL_GetGPUDeviceProperties(device),
                SDL_PROP_GPU_DEVICE_NAME_STRING,
                "unavailable"
            ) + "\" prefer_low_power=" +
            (options.preferLowPower ? "true" : "false")
        );
        bool claimed = ClaimSdrVsync(device, window, log, "initial_window");
        if (!claimed)
        {
            SDL_DestroyGPUDevice(device);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }

        bool running = true;
        bool newWindowAttempted = false;
        bool validatingNewWindow = false;
        std::uint64_t renderedFrames = 0;
        int exitCode = 0;

        while (running)
        {
            SDL_Event event{};
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT)
                {
                    running = false;
                }
            }
            if (!running)
            {
                break;
            }

            SDL_GPUCommandBuffer* commandBuffer =
                SDL_AcquireGPUCommandBuffer(device);
            if (!commandBuffer)
            {
                log.Write(
                    std::string("command_buffer_acquire_failed error=\"") +
                    SDL_GetError() + '"'
                );
                exitCode = 1;
                break;
            }

            SDL_GPUTexture* texture = nullptr;
            if (!SDL_WaitAndAcquireGPUSwapchainTexture(
                commandBuffer,
                window,
                &texture,
                nullptr,
                nullptr
            ))
            {
                const std::string error = SDL_GetError();
                log.Write("swapchain_acquire_failed error=\"" + error + "\"");
                if (!SDL_CancelGPUCommandBuffer(commandBuffer))
                {
                    log.Write(
                        std::string("command_buffer_cancel_failed error=\"") +
                        SDL_GetError() + '"'
                    );
                    exitCode = 1;
                    break;
                }

                if (!options.tryNewWindowAfterFailure || newWindowAttempted)
                {
                    log.Write("terminal_failure fresh_hwnd_not_available=true");
                    exitCode = 1;
                    break;
                }

                newWindowAttempted = true;
                int x = 0;
                int y = 0;
                int width = 1280;
                int height = 720;
                SDL_GetWindowPosition(window, &x, &y);
                SDL_GetWindowSize(window, &width, &height);
                log.Write(
                    "fresh_hwnd_attempt_started x=" + std::to_string(x) +
                    " y=" + std::to_string(y) +
                    " size=" + std::to_string(width) + 'x' +
                    std::to_string(height)
                );

                SDL_ReleaseWindowFromGPUDevice(device, window);
                claimed = false;
                SDL_DestroyWindow(window);
                window = CreateWindowAt(x, y, width, height);
                if (!window || !ClaimSdrVsync(
                    device,
                    window,
                    log,
                    "fresh_hwnd"
                ))
                {
                    log.Write("fresh_hwnd_attempt_failed terminal_failure=true");
                    exitCode = 1;
                    break;
                }
                claimed = true;
                validatingNewWindow = true;
                continue;
            }

            if (!texture)
            {
                if (!SDL_SubmitGPUCommandBuffer(commandBuffer))
                {
                    log.Write(
                        std::string("empty_submit_failed error=\"") +
                        SDL_GetError() + '"'
                    );
                    exitCode = 1;
                    break;
                }
                ++renderedFrames;
                if (options.maxFrames && renderedFrames >= options.maxFrames)
                {
                    running = false;
                }
                continue;
            }

            SDL_GPUColorTargetInfo target{};
            target.texture = texture;
            target.clear_color = SDL_FColor{0.1f, 0.15f, 0.2f, 1.0f};
            target.load_op = SDL_GPU_LOADOP_CLEAR;
            target.store_op = SDL_GPU_STOREOP_STORE;
            SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(
                commandBuffer,
                &target,
                1,
                nullptr
            );
            if (!pass)
            {
                log.Write(
                    std::string("render_pass_failed error=\"") +
                    SDL_GetError() + '"'
                );
                SDL_SubmitGPUCommandBuffer(commandBuffer);
                exitCode = 1;
                break;
            }
            SDL_EndGPURenderPass(pass);

            if (!SDL_SubmitGPUCommandBuffer(commandBuffer))
            {
                log.Write(
                    std::string("submit_failed error=\"") + SDL_GetError() + '"'
                );
                exitCode = 1;
                break;
            }

            if (validatingNewWindow)
            {
                log.Write("fresh_hwnd_frame_submitted validation_succeeded=true");
                validatingNewWindow = false;
            }
            ++renderedFrames;
            if (options.maxFrames && renderedFrames >= options.maxFrames)
            {
                running = false;
            }
        }

        if (claimed && window)
        {
            SDL_ReleaseWindowFromGPUDevice(device, window);
        }
        SDL_DestroyGPUDevice(device);
        if (window)
        {
            SDL_DestroyWindow(window);
        }
        SDL_Quit();
        log.Write("session_end exit_code=" + std::to_string(exitCode));
        return exitCode;
    }
}
