#pragma once

#include "Renderer/Texture.h"
#include "UI/UIRenderer.h"

#include <cstdint>
#include <memory>

namespace Atom
{
    class Renderer;

    // Render-to-texture (M27): a colour target the renderer draws into
    // before the scene, usable afterwards as any material's texture. Its
    // canvas is the same immediate-mode 2D batcher as the UI overlay, at the
    // target's fixed virtual resolution (e.g. 320x240): whatever is drawn
    // into it during a frame lands on the texture that frame. Sampled with
    // nearest filtering, so the pixels stay crisp on a screen up close.
    //
    // Created by Renderer::CreateRenderTexture; unregisters itself when
    // destroyed. Must be destroyed before the renderer.
    class RenderTexture
    {
    public:
        ~RenderTexture();

        RenderTexture(const RenderTexture&) = delete;
        RenderTexture& operator=(const RenderTexture&) = delete;

        UIRenderer& GetCanvas() { return m_canvas; }
        const Texture& GetTexture() const { return *m_texture; }
        std::uint32_t GetWidth() const { return m_texture->GetWidth(); }
        std::uint32_t GetHeight() const { return m_texture->GetHeight(); }

    private:
        friend class Renderer;
        RenderTexture() = default;

        // Renderer-facing: copy the canvas to the GPU and draw it, before
        // the scene pass samples the texture. False if nothing was drawn.
        bool Render(SDL_GPUCommandBuffer* commandBuffer);

        Renderer* m_owner = nullptr;
        std::unique_ptr<Texture> m_texture;
        UIRenderer m_canvas;
    };
}
