#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <SDL3/SDL_gpu.h>

namespace Atom
{
    class Font;
    class Texture;

    // Immediate-mode 2D overlay drawn on top of the final image: solid
    // rectangles and text, in window pixels with the origin top-left.
    // Colours are sRGB (as picked in any paint program), alpha straight.
    // Everything drawn is cleared after each frame.
    class UIRenderer
    {
    public:
        bool Initialize(SDL_GPUDevice* device, SDL_GPUTextureFormat targetFormat);
        void Shutdown();

        // Size of the last presented frame; use it to lay out the next.
        glm::vec2 GetScreenSize() const { return m_screenSize; }

        void DrawRect(glm::vec2 position, glm::vec2 size, glm::vec4 color);

        // A whole texture stretched over a rectangle (M29), tinted by `color`
        // (its alpha fades it). Pixel-art textures (render targets) are
        // sampled with nearest filtering, so integer scales stay crisp.
        void DrawImage(const Texture& texture, glm::vec2 position, glm::vec2 size,
                       glm::vec4 color = glm::vec4{ 1.0f });

        // `position` is the top-left of the first line. '\n' starts a line.
        void DrawText(
            const Font& font,
            std::string_view utf8,
            glm::vec2 position,
            glm::vec4 color,
            float scale = 1.0f
        );

        glm::vec2 MeasureText(const Font& font, std::string_view utf8, float scale = 1.0f) const;

        // Inserts line breaks at spaces so no line exceeds `maxWidth`.
        std::string WrapText(
            const Font& font,
            std::string_view utf8,
            float maxWidth,
            float scale = 1.0f
        ) const;

        // Renderer-facing: copy this frame's vertices to the GPU (outside
        // any render pass), then draw them into the swapchain image.
        bool Upload(SDL_GPUCommandBuffer* commandBuffer);
        bool Render(
            SDL_GPUCommandBuffer* commandBuffer,
            SDL_GPUTexture* target,
            std::uint32_t width,
            std::uint32_t height
        );
        void EndFrame();

        bool HasContent() const { return !m_vertices.empty(); }

    private:
        struct Vertex
        {
            glm::vec2 position;
            glm::vec2 uv;
            glm::vec4 color; // linear
        };

        struct Batch
        {
            const Texture* texture;
            std::uint32_t firstVertex;
            std::uint32_t vertexCount;
        };

        void PushQuad(
            const Texture* texture,
            glm::vec2 min,
            glm::vec2 max,
            glm::vec2 uvMin,
            glm::vec2 uvMax,
            glm::vec4 linearColor
        );

        SDL_GPUDevice* m_device = nullptr;
        SDL_GPUGraphicsPipeline* m_pipeline = nullptr;
        SDL_GPUSampler* m_sampler = nullptr;
        SDL_GPUSampler* m_pixelSampler = nullptr; // nearest, for pixel art
        SDL_GPUBuffer* m_vertexBuffer = nullptr;
        SDL_GPUTransferBuffer* m_transferBuffer = nullptr;
        std::unique_ptr<Texture> m_white;

        std::vector<Vertex> m_vertices;
        std::vector<Batch> m_batches;
        std::uint32_t m_uploadedVertices = 0;
        glm::vec2 m_screenSize{ 1280.0f, 720.0f };
    };
}
