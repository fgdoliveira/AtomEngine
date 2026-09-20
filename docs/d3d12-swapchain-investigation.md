# D3D12 Swapchain Resize Investigation

> Private AtomEngine diagnostic notes. This document is not intended for an SDL bug report, patch, pull request, or other SDL contribution.

## Investigation status

The supervised Orca investigation completed successfully under run `run_73ac9a49d448`. Three read-only workers independently audited HRESULT provenance, back-buffer ownership, and Microsoft DXGI/D3D12 contracts. No SDL files were changed.

The investigation first identified the repeated retry state from source inspection. Debugger captures on 20 September 2026 then proved the initiating API failure at its call site, the failure of swapchain-only reconstruction, and the failure of complete SDL GPU-device reconstruction. No SDL files were changed during these captures.

## Diagnosis

The repeatable state transition is:

```text
pixel-size event
  -> needsSwapchainRecreate = true
  -> D3D12_INTERNAL_ResizeSwapchain fails
  -> needsSwapchainRecreate remains true
  -> AtomEngine cancels the empty command buffer and returns success
  -> the next frame attempts the same resize again
```

This explains the retry loop. The runtime capture now also identifies the condition that caused its first observed resize failure.

### Confirmed runtime capture

Execution was stopped on `SDL_gpu_d3d12.c:6948`, immediately after the `IDXGISwapChain::ResizeBuffers` call and before `D3D12_INTERNAL_SetError` changed `res`.

| Observed value | Debugger result |
|---|---|
| Original `ResizeBuffers` HRESULT | `0x887A0005` (`DXGI_ERROR_DEVICE_REMOVED`) |
| Immediate `ID3D12Device::GetDeviceRemovedReason()` | `S_OK` (`0x00000000`) |
| Stored `windowData` swapchain size | `1280 x 720` |
| Swapchain buffer count | `2` |
| `frameCounter` | `0` |
| `needsSwapchainRecreate` | `true` |

This satisfies the requirement to identify the exact failing call with its own HRESULT. It also proves that the later error text containing `0x00000000` did not contain the HRESULT returned by `ResizeBuffers`.

### Confirmed swapchain-only recovery failure

A later diagnostic run tested destruction and recreation of only the swapchain while retaining the same SDL/D3D12 GPU device. The persistent log and debugger established this sequence:

| Observed value | Result |
|---|---|
| Failure display | `Generic PnP Monitor`, display ID `2` |
| Failure window state | `1920 x 991`, display scale `1.25` |
| Old swapchain release | Completed |
| `IDXGIFactory4::CreateSwapChainForHwnd` HRESULT | `0x887A0005` (`DXGI_ERROR_DEVICE_REMOVED`) |
| Immediate `ID3D12Device::GetDeviceRemovedReason()` | `S_OK` (`0x00000000`) |
| Time from release completion to reclaim failure | Approximately `2.78` seconds |
| Failure handling | Clean terminal shutdown; no retry loop |

The original run began on `Acer KA240HQ`, display ID `1`, at `1280 x 720` and scale `1.0`. The failure occurred after the transition to the second display. These display facts describe the reproducible trigger; they do not by themselves establish the underlying driver or DXGI cause.

This disproves swapchain-only recovery for the captured state. After the old swapchain was released, the existing SDL GPU device's DXGI factory/command-queue path could not create a replacement swapchain, even though the D3D12 device-removal query continued to report `S_OK`.

### Confirmed full-device reconstruction failure

The next diagnostic replaced the complete SDL GPU device rather than retaining the existing device. Repeated persistent-log captures and a debugger stop at `SDL_gpu_d3d12.c:7072` established this sequence:

| Observed stage | Result |
|---|---|
| Failed frame command buffer | Cancelled successfully |
| Old window claim | Released successfully |
| Old SDL GPU device | Destroyed successfully |
| Replacement backend request | Explicit `direct3d12` with DXIL and Debug mode |
| Replacement SDL GPU device | Created successfully and reported `direct3d12` |
| Replacement `CreateSwapChainForHwnd` | `0x887A0005` (`DXGI_ERROR_DEVICE_REMOVED`) |
| Replacement `ID3D12Device::GetDeviceRemovedReason()` | `S_OK` (`0x00000000`) |
| Replacement device cleanup | Completed successfully |
| Application result | Clean terminal renderer shutdown and `session_end` |

The replacement call used a new SDL GPU device and its new D3D12/DXGI objects but the same process, HWND, and active display topology. Complete device reconstruction therefore does not restore the presentation path for this captured state. The experiment never reached a non-null replacement swapchain texture or a validated recovered frame.

The Visual Studio Output window showed no D3D12 validation-layer error at any captured failure point. It contained a `D3D11: Removing Device!` line and first-chance C++ exceptions. Process-module inspection also found NVIDIA's `nvspcap64.dll` capture component loaded. These are contextual observations only: their producer and causal relationship were not proven, and overlay isolation was declined for this investigation.

### Why a resize error can display `0x00000000`

The debugger confirmed the complete path behind both `Could not resize swapchain buffers` and `Could not create swapchain` messages with error code zero:

1. `IDXGISwapChain::ResizeBuffers` returns `DXGI_ERROR_DEVICE_REMOVED`.
2. `D3D12_INTERNAL_SetError` replaces that HRESULT with `ID3D12Device::GetDeviceRemovedReason()`.
3. The secondary call returns `S_OK`.
4. SDL formats the replacement value as `0x00000000` instead of preserving the original HRESULT.

Microsoft explicitly documents that `GetDeviceRemovedReason()` returns `S_OK` when the D3D12 device does not report itself removed:

- [ID3D12Device::GetDeviceRemovedReason](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-getdeviceremovedreason)

Relevant source locations:

- `external/SDL/src/gpu/d3d12/SDL_gpu_d3d12.c:6941`: `ResizeBuffers`
- `external/SDL/src/gpu/d3d12/SDL_gpu_d3d12.c:6948`: resize HRESULT check
- `external/SDL/src/gpu/d3d12/SDL_gpu_d3d12.c:7064`: `CreateSwapChainForHwnd`
- `external/SDL/src/gpu/d3d12/SDL_gpu_d3d12.c:7072`: creation HRESULT check
- `external/SDL/src/gpu/d3d12/SDL_gpu_d3d12.c:1256`: D3D12 error helper
- `external/SDL/src/gpu/d3d12/SDL_gpu_d3d12.c:1267`: replacement of `DXGI_ERROR_DEVICE_REMOVED`
- `Engine/Renderer/Renderer.cpp:278`: AtomEngine acquisition boundary
- `Engine/Renderer/Renderer.cpp:293`: command-buffer cancellation after acquisition failure
- `Engine/Renderer/Renderer.cpp:314`: old-window release in the full-device reconstruction experiment
- `Engine/Renderer/Renderer.cpp:319`: old-device destruction in the full-device reconstruction experiment

The zero code is therefore an error-reporting artifact. It is not evidence that `ResizeBuffers` succeeded.

## Call chain

```text
Atom::Renderer::Render
  Engine/Renderer/Renderer.cpp:67
    -> SDL_WaitAndAcquireGPUSwapchainTexture
       external/SDL/src/gpu/SDL_gpu.c:3337
      -> D3D12_WaitAndAcquireSwapchainTexture
         external/SDL/src/gpu/d3d12/SDL_gpu_d3d12.c:7741
        -> D3D12_INTERNAL_AcquireSwapchainTexture
           external/SDL/src/gpu/d3d12/SDL_gpu_d3d12.c:7612
          -> D3D12_INTERNAL_ResizeSwapchain, when recreation is pending
             external/SDL/src/gpu/d3d12/SDL_gpu_d3d12.c:6908
            -> IDXGISwapChain::ResizeBuffers
               external/SDL/src/gpu/d3d12/SDL_gpu_d3d12.c:6941
            -> D3D12_INTERNAL_InitializeSwapchainTexture for every buffer
               external/SDL/src/gpu/d3d12/SDL_gpu_d3d12.c:6951
              -> IDXGISwapChain::GetBuffer
                 external/SDL/src/gpu/d3d12/SDL_gpu_d3d12.c:6808
```

Every plain `return false` in this chain preserves the SDL error set by the nested failure. Wrapping the return at line 6958 with a new generic error would overwrite more useful detail.

## Confirmed defects

### Stale `res` checks

Both `IDXGISwapChain3_GetDesc1` calls fail to assign their HRESULT to `res`:

- `external/SDL/src/gpu/d3d12/SDL_gpu_d3d12.c:6962-6964`
- `external/SDL/src/gpu/d3d12/SDL_gpu_d3d12.c:7121-7122`

The subsequent macros therefore inspect an earlier HRESULT. These are real defects, but they do not explain an error specifically labelled `Could not resize swapchain buffers`.

### Acquired back-buffer lifetime

The initialization-time `GetBuffer` reference is temporary and is released on success and on each checked allocation failure.

The per-frame `GetBuffer` at `SDL_gpu_d3d12.c:7688` stores an owning reference in `activeTexture->resource`. The normal submit/present path releases it at line 8130, including when `Present` fails.

Cancellation after a successful acquisition is forbidden only by a debug-mode check in `external/SDL/src/gpu/SDL_gpu.c:3429-3447`. In a non-debug build, the backend cancellation path clears presentation bookkeeping without releasing the acquired resource. Several early submission failures can similarly bypass the normal release. Such a leaked reference can make later `ResizeBuffers` calls fail persistently.

This is a confirmed lifetime defect, but it is not proven as the trigger for the current incident. AtomEngine's cancellation at `Renderer.cpp:102` follows an acquisition failure, before a successful per-frame `GetBuffer` has been established.

### Partial resize initialization

If buffer `N` fails during resize-time initialization:

- Buffers `0..N-1` retain initialized texture wrappers and descriptors.
- Those wrappers do not retain back-buffer COM references because their temporary initialization references were released.
- Buffer `N` and later buffers remain null or zeroed, aside from smaller allocation/descriptor edge-case defects.
- `needsSwapchainRecreate` remains true.
- The next attempt's guarded cleanup releases the earlier wrappers before retrying.

Partial initialization therefore explains the retry state but is not, by itself, a persistent back-buffer-reference failure.

## Findings and remaining hypotheses

1. **Confirmed:** `ResizeBuffers` returned `DXGI_ERROR_DEVICE_REMOVED`, and SDL replaced that result with the immediately queried `S_OK` removal reason before formatting the message.
2. **Confirmed:** after releasing the old swapchain, `CreateSwapChainForHwnd` on the same SDL GPU device also returned `DXGI_ERROR_DEVICE_REMOVED`; its immediate removal-reason query also returned `S_OK`.
3. **Confirmed:** after destroying the old SDL GPU device, a fresh `direct3d12` SDL GPU device was created successfully, but its `CreateSwapChainForHwnd` call returned the same `DXGI_ERROR_DEVICE_REMOVED` while its new D3D12 device reported `S_OK`.
4. **Confirmed:** neither swapchain-only nor complete SDL GPU-device reconstruction can recover the captured state.
5. **Confirmed:** the bounded diagnostic cleans up every partial state and terminates without a retry loop or unhandled application crash.
6. **Still unproven:** why DXGI reports removal for presentation operations while both old and replacement D3D12 devices report healthy. The captures contained no D3D12 InfoQueue explanation.
7. **Still possible but less likely:** an outstanding direct or indirect old back-buffer reference contributed to the initiating resize failure. It cannot explain why a newly created device fails to create a swapchain on the retained HWND unless wider process, driver, or presentation state is involved.
8. **Rejected for these captures:** the displayed errors came only from stale SDL error state or different calls. The original HRESULTs were observed directly at their call-site checks.

## Unsupported assumptions

Current evidence does not support:

- Claiming either `GetBuffer` call is failing; those paths use different error labels.
- Treating the stale `GetDesc1` checks as the source of the resize-labelled failure.
- Wrapping the plain resize return with a generic error.
- Recreating the swapchain merely because the window crossed monitors.
- Treating monitor movement or minimization alone as device loss.
- Assuming `GetDeviceRemovedReason() == S_OK` means the existing presentation stack is reusable.
- Retrying an unknown or repeatable failure indefinitely.

## Back-buffer ownership invariants

Before `ResizeBuffers`:

1. No per-frame `GetBuffer` reference may remain in any `activeTexture->resource`.
2. No submitted or partially recorded command list may still reference a back buffer or its view.
3. GPU work using the old buffers must have completed.
4. Old SRV/RTV descriptors and wrapper allocations must be released.
5. After a successful resize, every buffer must be reacquired and its wrapper rebuilt.

Microsoft requires every direct and indirect back-buffer reference to be released before `ResizeBuffers`, followed by fresh `GetBuffer` queries:

- [IDXGISwapChain::ResizeBuffers](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-resizebuffers)
- [DXGI overview and window resizing](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/d3d10-graphics-programming-guide-dxgi)

## Recovery decision

Use classified, bounded failure handling:

1. On an ordinary successful `ResizeBuffers`, rebuild all back-buffer texture wrappers, descriptors, and indices.
2. For an InfoQueue-confirmed outstanding-reference or invalid-state failure, correct the ownership state and retry once.
3. For the captured special case—`ResizeBuffers == DXGI_ERROR_DEVICE_REMOVED` while the immediate D3D12 removal reason is `S_OK`—stop retrying the same swapchain.
4. Swapchain-only reconstruction is no longer a candidate for this incident: it was attempted once and `CreateSwapChainForHwnd` returned the same `DXGI_ERROR_DEVICE_REMOVED` result.
5. Complete SDL GPU-device reconstruction is also rejected for this incident: a fresh device could not create a swapchain for the retained HWND.
6. Do not add more in-process recovery tiers. Preserve the failure evidence and terminate rendering cleanly.
7. Preserve the original HRESULT, the separate removal reason, InfoQueue messages, DRED data, topology state, and recovery-stage results across every boundary.
8. Do not rebuild merely because the window changed monitors. The monitor transition is a reproducible trigger to investigate, not a sufficient recovery classification.

The full-device code remains temporarily on this private diagnostic branch so the failed experiment and its ordered cleanup can be reproduced. It is not a production recovery direction. Microsoft documents `S_OK` from `GetDeviceRemovedReason()` as meaning that the D3D12 device is not reporting itself removed, but the captures show that this does not guarantee a usable DXGI presentation path.

Microsoft's Advanced Color guidance warns that recreating a swapchain merely to refresh output information introduces temporary black frames:

- [High Dynamic Range and Wide Color Gamut](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range)

## Decision tree

```text
Resize or output-related event
  |
  +-- Minimized/occluded with no renderable client area?
  |     -> Skip rendering; retry after restore.
  |
  +-- ResizeBuffers succeeds?
  |     -> Rebuild every back-buffer wrapper and continue.
  |
  +-- DXGI_ERROR_DEVICE_REMOVED or DXGI_ERROR_DEVICE_RESET?
  |     -> Capture original HRESULT, removal reason, InfoQueue, and DRED.
  |     |
  |     +-- GetDeviceRemovedReason also failed?
  |     |     -> Rebuild the device and all dependent resources.
  |     |
  |     +-- GetDeviceRemovedReason returned S_OK?
  |           -> Do not retry ResizeBuffers on the same swapchain.
  |           -> The captured swapchain-only reconstruction also failed.
  |           -> The captured complete-device reconstruction also failed.
  |           -> Terminate cleanly and preserve presentation/topology evidence.
  |           -> Do not add another runtime recovery tier.
  |
  +-- Invalid call/outstanding reference confirmed by InfoQueue?
  |     -> Account for and release the reference or command-list ownership.
  |     -> Retry once after the state is corrected.
  |
  +-- Creation-only invariant changed or format/configuration cannot be
  |   expressed with ResizeBuffers plus SetColorSpace1?
  |     -> Fully recreate the swapchain.
  |
  +-- Unknown or repeatable HRESULT?
        -> Stop the blind retry loop and preserve diagnostics.
```

## Next controlled investigation

The next step is to identify which state shared across the failed replacement device prevents presentation. Keep all diagnostics private to AtomEngine or standalone private probes; do not modify SDL.

### Procedure

1. Retrieve and persist DXGI InfoQueue messages at initialization and every failure/reconstruction boundary.
2. Record the active Windows display path with `QueryDisplayConfig`: adapter LUID, source and target IDs, connector technology, scaling, rotation, refresh rate, availability, monitor name, DPI, Advanced Color state, HWND validity, and client/window rectangles.
3. Capture a short GPUView/WPR ETW trace beginning before the monitor crossing and ending immediately after the failure, plus matching `Display`, `nvlddmkm`, and `DxgKrnl` event-log entries.
4. Reproduce with a minimal private SDL GPU presentation probe and then a raw Win32/D3D12/DXGI probe using the same SDR, VSYNC, two-buffer flip-discard configuration.
5. In each probe, allow one diagnostic-only new-HWND test after failure. Do not promote window recreation into runtime recovery without separate evidence and validation.
6. Keep NVIDIA capture/overlay injection recorded as an unexcluded variable because isolation of that component is outside the chosen test constraints.

### Implemented private evidence capture

AtomEngine now opens the DXGI debug InfoQueue before creating the SDL GPU device and records only messages added after that baseline. It drains the queue at device creation, window claim, acquisition failure, device destruction, and shutdown boundaries.

The same private diagnostic records the Win32 HWND and validity, window and client rectangles, DPI, SDL display and pixel size, monitor handle and GDI name, and every active `QueryDisplayConfig` path. Each path includes its adapter LUID, source and target IDs, monitor name, connector technology, rotation, scaling, refresh rate, availability, and Advanced Color state. A snapshot is written at startup, after a successful claim, when the window's display geometry changes, at acquisition failure, and during shutdown.

This instrumentation is evidence collection only. It does not modify SDL, expose a public AtomEngine API, or add another recovery attempt.

### Decision gates

- Fresh HWND succeeds in the same process: investigate stale HWND/presentation binding and validate window recreation separately.
- Fresh HWND fails but a new process succeeds: classify the state as process-global runtime or driver state; clean restart remains the only reliable recovery.
- SDL probe fails while raw D3D12 succeeds: isolate the differing SDL presentation/event sequence without modifying or contributing generated material to SDL.
- Raw D3D12 also fails: classify the incident as Windows/NVIDIA/display-topology behavior and stop AtomEngine recovery work.
- DXGI diagnostics identify an invalid call, ownership error, or deterministic transient-topology condition: implement only the smallest AtomEngine-side prevention supported by that evidence.

### Fix acceptance threshold

Any candidate AtomEngine fix must prevent the original acquisition failure in five reproduced monitor-crossing episodes. After each crossing, render for 30 seconds, resize repeatedly, minimize and restore three times, cross monitors again, and shut down normally with no D3D12/DXGI error, live-object warning, corrupted frame, or retry loop.

## Instrumentation checklist

A human diagnostic build should capture the following before any error translation:

- Monotonic event, frame, and call sequence plus thread identifier.
- Exact call name and raw numeric and symbolic HRESULT for:
  - `ResizeBuffers`
  - `CreateSwapChainForHwnd`
  - initialization and per-frame `GetBuffer`
  - `GetDesc1`
  - `Present`
  - `CheckColorSpaceSupport`
  - `SetColorSpace1`
- `GetDeviceRemovedReason` as a separate value, without replacing the original HRESULT.
- Current back-buffer index and requested `GetBuffer` index.
- Physical client size, logical size, DPI, minimized state, and output identity.
- Complete `DXGI_SWAP_CHAIN_DESC1`, relevant fullscreen description, sync interval, present flags, requested color space, and support flags.
- Every acquired and released back-buffer reference.
- Cancellation, submission exit, fence, command-list cleanup, and resize-wait events.
- D3D12/DXGI debug-layer and InfoQueue messages with producer, category, severity, ID, and description.
- DRED breadcrumbs and page-fault data when removal is reported.
- The SDL error copied immediately when acquisition returns false, before cancellation or another SDL call.
- Debug/release configuration and the exact loaded SDL binary revision.

Useful diagnostic references:

- [D3D12 Debug Layer](https://learn.microsoft.com/en-us/windows/win32/direct3d12/understanding-the-d3d12-debug-layer)
- [ID3D12InfoQueue](https://learn.microsoft.com/en-us/windows/win32/api/d3d12sdklayers/nn-d3d12sdklayers-id3d12infoqueue)
- [Device Removed Extended Data](https://learn.microsoft.com/en-us/windows/win32/direct3d12/use-dred)
- [DXGI error codes](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-error)

## Manual validation matrix

Run each case in debug and release configurations. Exercise VSYNC, immediate, and mailbox modes where supported.

| Case | Procedure | Expected result | Required evidence |
|---|---|---|---|
| Same-monitor resize | Repeatedly drag-resize at fixed DPI | Release old references, one successful `ResizeBuffers` per settled size, rebuild all wrappers | HRESULT sequence, client pixels, swapchain description, buffer indices, InfoQueue |
| Cross-monitor DPI move | Move between displays with different DPI and pause while spanning them | If physical client pixels change, use the normal resize path; otherwise re-evaluate output/color support without recreation | `WM_DPICHANGED`, DPI, physical size, majority output, resize calls |
| Minimize and restore | Minimize, wait, then restore and resize | Skip rendering while minimized; restore through normal resize without an endless failure loop | Minimized state, client size, first restore HRESULTs |
| SDR/HDR transition | Move between SDR/HDR outputs and toggle Windows HDR | Requery output/color support; use `SetColorSpace1` when compatible; resize/rebuild for format change | Format, color space, support flags, output identity, HRESULTs |
| Refresh/VRR transition | Move between fixed-refresh and VRR displays; test tearing modes | Preserve the `ALLOW_TEARING` contract through resize and matching present flags | Swapchain flags, sync interval, present flags, output identity |
| Controlled device removal | Use a private diagnostic harness capable of controlled removal | Capture original failure and removal reason, then rebuild the whole device graph | Original HRESULT, separate removal reason, InfoQueue, DRED |
| Shutdown | Close after normal rendering, failed resize, and restore | Wait for GPU, release acquired buffers/fences/wrappers, then destroy swapchain and device | Debug-layer live-object report and ordered release trace |

## Current conclusion

The initiating failure and misleading error translation are proven at three presentation stages. `ResizeBuffers` returned `DXGI_ERROR_DEVICE_REMOVED`; swapchain-only reconstruction returned the same result from `CreateSwapChainForHwnd`; and a newly created SDL/D3D12 GPU device again returned the same result while claiming the retained HWND. Every immediate D3D12 device-removal query returned `S_OK`, which SDL substituted into its error message as `0x00000000`.

The evidence rejects continued resize retries, swapchain-only reconstruction, and complete SDL GPU-device reconstruction as runtime recovery strategies. The current diagnostic terminates cleanly after preserving the failure. Further work must identify whether the persistent state is bound to the HWND, process-wide DXGI state, SDL's presentation sequence, or the Windows/NVIDIA display-topology path before another fix is proposed.
