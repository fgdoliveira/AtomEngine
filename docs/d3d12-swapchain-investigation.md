# D3D12 Swapchain Resize Investigation

> Private AtomEngine diagnostic notes. This document is not intended for an SDL bug report, patch, pull request, or other SDL contribution.

## Investigation status

The supervised Orca investigation completed successfully under run `run_73ac9a49d448`. Three read-only workers independently audited HRESULT provenance, back-buffer ownership, and Microsoft DXGI/D3D12 contracts. No SDL files were changed.

The investigation identified the repeated retry state from source inspection, but the initial failing API call is not yet proven by a raw runtime trace. A later `SDL_GetError()` snapshot is insufficient to satisfy that proof requirement.

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

This explains the retry loop, but it does not by itself identify the condition that caused the first resize failure.

### Why a resize error can display `0x00000000`

The strongest source-level explanation for a message labelled `Could not resize swapchain buffers` with error code zero is:

1. `IDXGISwapChain::ResizeBuffers` returns `DXGI_ERROR_DEVICE_REMOVED`.
2. `D3D12_INTERNAL_SetError` replaces that HRESULT with `ID3D12Device::GetDeviceRemovedReason()`.
3. The secondary call returns `S_OK`.
4. SDL formats the replacement value as `0x00000000` instead of preserving the original `ResizeBuffers` HRESULT.

Microsoft explicitly documents that `GetDeviceRemovedReason()` returns `S_OK` when the D3D12 device does not report itself removed:

- [ID3D12Device::GetDeviceRemovedReason](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-getdeviceremovedreason)

Relevant source locations:

- `external/SDL/src/gpu/d3d12/SDL_gpu_d3d12.c:6941`: `ResizeBuffers`
- `external/SDL/src/gpu/d3d12/SDL_gpu_d3d12.c:6948`: resize HRESULT check
- `external/SDL/src/gpu/d3d12/SDL_gpu_d3d12.c:1256`: D3D12 error helper
- `external/SDL/src/gpu/d3d12/SDL_gpu_d3d12.c:1267`: replacement of `DXGI_ERROR_DEVICE_REMOVED`
- `Engine/Renderer/Renderer.cpp:90`: AtomEngine acquisition boundary
- `Engine/Renderer/Renderer.cpp:102`: silent cancel-and-retry behavior

The exact failing call remains unconfirmed until the original HRESULT is captured at the call site before this translation occurs.

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

## Ranked hypotheses

1. **Original `ResizeBuffers` result was `DXGI_ERROR_DEVICE_REMOVED`.** SDL replaced it with an `S_OK` device-removal reason before formatting the message. This is the only direct path in the inspected source that combines the resize label with `0x00000000`.
2. **An outstanding back-buffer reference made `ResizeBuffers` fail.** Possible sources include a previously cancelled acquired frame, an abandoned command buffer, or an early submission failure. The debug layer and InfoQueue should identify this state.
3. **The displayed error was stale or came from a different binary.** SDL errors are thread-local last-error state and successful calls do not automatically clear them. The exact loaded SDL revision and complete timestamped log are required.

## Unsupported assumptions

Current evidence does not support:

- Claiming either `GetBuffer` call is failing; those paths use different error labels.
- Treating the stale `GetDesc1` checks as the source of the resize-labelled failure.
- Wrapping the plain resize return with a generic error.
- Recreating the swapchain merely because the window crossed monitors.
- Treating monitor movement or minimization alone as device loss.
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

## Selected recovery strategy

Use classified, resize-first recovery:

1. Capture the original HRESULT at the exact failing call.
2. After successful `ResizeBuffers`, rebuild all back-buffer texture wrappers, descriptors, and indices.
3. For an InfoQueue-confirmed outstanding-reference or invalid-state failure, correct the ownership state and retry once.
4. For `DXGI_ERROR_DEVICE_REMOVED` or `DXGI_ERROR_DEVICE_RESET`, capture the separate removal reason and DRED data, then rebuild the D3D12 device and all dependent resources. Do not loop on swapchain resize.
5. Fully recreate the swapchain only when a creation-only invariant changes or diagnosed evidence shows that resize cannot restore a valid chain.
6. Do not recreate solely because the window changed monitors.

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
  |     -> Rebuild the device and all dependent resources.
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

## Instrumentation checklist

A human diagnostic build should capture the following before any error translation:

- Monotonic event, frame, and call sequence plus thread identifier.
- Exact call name and raw numeric and symbolic HRESULT for:
  - `ResizeBuffers`
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

The retry loop is explained by a reproducible state transition, but the initiating failure remains unproven. The next decisive step is to capture the original `ResizeBuffers` HRESULT before SDL's error helper changes it, alongside the separate device-removal reason and debug-layer output. Until that evidence exists, swapchain recreation based solely on monitor movement is not justified.
