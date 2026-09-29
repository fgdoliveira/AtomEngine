#include "Renderer/RenderTexture.h"

#include "Renderer/Renderer.h"

namespace Atom
{
    RenderTexture::~RenderTexture()
    {
        if (m_owner)
        {
            m_owner->UnregisterRenderTexture(this);
        }
        m_canvas.Shutdown();
    }

    bool RenderTexture::Render(SDL_GPUCommandBuffer* commandBuffer)
    {
        if (!m_canvas.HasContent())
        {
            return false;
        }
        // The canvas draws over what the target holds; every frame's first
        // rectangle covers it all, so nothing old shows through.
        const bool ok = m_canvas.Upload(commandBuffer)
            && m_canvas.Render(commandBuffer, m_texture->GetGPUTexture(), GetWidth(), GetHeight());
        m_canvas.EndFrame();
        return ok;
    }
}
