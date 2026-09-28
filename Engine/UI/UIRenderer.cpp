#include "UI/UIRenderer.h"

#include "Renderer/Shader.h"
#include "Renderer/Texture.h"
#include "UI/Font.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iostream>

namespace Atom
{
    namespace
    {
        constexpr std::uint32_t MaxVertices = 6 * 8192;

        // Decodes one UTF-8 sequence starting at `index`, advancing it.
        // Malformed bytes decode as '?'.
        std::uint32_t NextCodepoint(std::string_view text, std::size_t& index)
        {
            const auto byte = [&](std::size_t i) {
                return static_cast<unsigned char>(text[i]);
            };
            const unsigned char lead = byte(index);
            int extra = 0;
            std::uint32_t codepoint = 0;
            if (lead < 0x80) { codepoint = lead; }
            else if ((lead & 0xE0) == 0xC0) { codepoint = lead & 0x1F; extra = 1; }
            else if ((lead & 0xF0) == 0xE0) { codepoint = lead & 0x0F; extra = 2; }
            else if ((lead & 0xF8) == 0xF0) { codepoint = lead & 0x07; extra = 3; }
            else { ++index; return '?'; }

            ++index;
            for (int i = 0; i < extra; ++i)
            {
                if (index >= text.size() || (byte(index) & 0xC0) != 0x80)
                {
                    return '?';
                }
                codepoint = (codepoint << 6) | (byte(index) & 0x3F);
                ++index;
            }
            return codepoint;
        }

        float SrgbToLinear(float c)
        {
            return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
        }

        glm::vec4 ToLinear(glm::vec4 srgb)
        {
            return { SrgbToLinear(srgb.r), SrgbToLinear(srgb.g), SrgbToLinear(srgb.b), srgb.a };
        }
    }

    bool UIRenderer::Initialize(SDL_GPUDevice* device, SDL_GPUTextureFormat targetFormat)
    {
        m_device = device;

        SDL_GPUShader* vertexShader = LoadShader(
            device, "UI.vert", SDL_GPU_SHADERSTAGE_VERTEX,
            ShaderResources{ .uniformBuffers = 1 });
        SDL_GPUShader* fragmentShader = LoadShader(
            device, "UI.frag", SDL_GPU_SHADERSTAGE_FRAGMENT,
            ShaderResources{ .samplers = 1 });
        if (!vertexShader || !fragmentShader)
        {
            if (vertexShader) { SDL_ReleaseGPUShader(device, vertexShader); }
            if (fragmentShader) { SDL_ReleaseGPUShader(device, fragmentShader); }
            return false;
        }

        SDL_GPUVertexBufferDescription vertexBuffer{};
        vertexBuffer.pitch = sizeof(Vertex);
        vertexBuffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexAttribute attributes[3]{};
        attributes[0] = { 0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Vertex, position) };
        attributes[1] = { 1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Vertex, uv) };
        attributes[2] = { 2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(Vertex, color) };

        SDL_GPUColorTargetDescription colorTarget{};
        colorTarget.format = targetFormat;
        colorTarget.blend_state.enable_blend = true;
        colorTarget.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        colorTarget.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        colorTarget.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        colorTarget.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        colorTarget.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        colorTarget.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

        SDL_GPUGraphicsPipelineCreateInfo createInfo{};
        createInfo.vertex_shader = vertexShader;
        createInfo.fragment_shader = fragmentShader;
        createInfo.vertex_input_state.vertex_buffer_descriptions = &vertexBuffer;
        createInfo.vertex_input_state.num_vertex_buffers = 1;
        createInfo.vertex_input_state.vertex_attributes = attributes;
        createInfo.vertex_input_state.num_vertex_attributes = 3;
        createInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        createInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        createInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        createInfo.target_info.color_target_descriptions = &colorTarget;
        createInfo.target_info.num_color_targets = 1;

        m_pipeline = SDL_CreateGPUGraphicsPipeline(device, &createInfo);
        SDL_ReleaseGPUShader(device, vertexShader);
        SDL_ReleaseGPUShader(device, fragmentShader);

        SDL_GPUSamplerCreateInfo samplerInfo{};
        samplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
        samplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
        samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.max_lod = 1000.0f;
        m_sampler = SDL_CreateGPUSampler(device, &samplerInfo);

        SDL_GPUBufferCreateInfo bufferInfo{};
        bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bufferInfo.size = MaxVertices * sizeof(Vertex);
        m_vertexBuffer = SDL_CreateGPUBuffer(device, &bufferInfo);

        SDL_GPUTransferBufferCreateInfo transferInfo{};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = MaxVertices * sizeof(Vertex);
        m_transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferInfo);

        // Solid rectangles sample a white texel so one shader does both.
        constexpr std::uint8_t white[4] = { 255, 255, 255, 255 };
        m_white = Texture::Create(device, 1, 1, white, false);

        if (!m_pipeline || !m_sampler || !m_vertexBuffer || !m_transferBuffer || !m_white)
        {
            std::cerr << "Failed to create UI renderer: " << SDL_GetError() << '\n';
            return false;
        }
        return true;
    }

    void UIRenderer::Shutdown()
    {
        if (m_device)
        {
            m_white.reset();
            if (m_pipeline) { SDL_ReleaseGPUGraphicsPipeline(m_device, m_pipeline); }
            if (m_sampler) { SDL_ReleaseGPUSampler(m_device, m_sampler); }
            if (m_vertexBuffer) { SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer); }
            if (m_transferBuffer) { SDL_ReleaseGPUTransferBuffer(m_device, m_transferBuffer); }
        }
        m_pipeline = nullptr;
        m_sampler = nullptr;
        m_vertexBuffer = nullptr;
        m_transferBuffer = nullptr;
        m_device = nullptr;
        EndFrame();
    }

    void UIRenderer::PushQuad(
        const Texture* texture,
        glm::vec2 min,
        glm::vec2 max,
        glm::vec2 uvMin,
        glm::vec2 uvMax,
        glm::vec4 linearColor
    )
    {
        if (m_vertices.size() + 6 > MaxVertices)
        {
            return;
        }

        // Consecutive quads with the same texture share one draw call.
        if (m_batches.empty() || m_batches.back().texture != texture)
        {
            m_batches.push_back(Batch{
                texture, static_cast<std::uint32_t>(m_vertices.size()), 0 });
        }

        const Vertex topLeft{ min, uvMin, linearColor };
        const Vertex topRight{ { max.x, min.y }, { uvMax.x, uvMin.y }, linearColor };
        const Vertex bottomLeft{ { min.x, max.y }, { uvMin.x, uvMax.y }, linearColor };
        const Vertex bottomRight{ max, uvMax, linearColor };
        m_vertices.insert(m_vertices.end(),
            { topLeft, topRight, bottomLeft, bottomLeft, topRight, bottomRight });
        m_batches.back().vertexCount += 6;
    }

    void UIRenderer::DrawRect(glm::vec2 position, glm::vec2 size, glm::vec4 color)
    {
        PushQuad(m_white.get(), position, position + size,
            glm::vec2{ 0.5f }, glm::vec2{ 0.5f }, ToLinear(color));
    }

    void UIRenderer::DrawText(
        const Font& font,
        std::string_view utf8,
        glm::vec2 position,
        glm::vec4 color,
        float scale
    )
    {
        const glm::vec4 linear = ToLinear(color);
        const float lineHeight = font.GetLineHeight() * scale;
        glm::vec2 pen{ position.x, position.y + font.GetAscent() * scale };
        std::uint32_t previous = 0;

        for (std::size_t i = 0; i < utf8.size();)
        {
            const std::uint32_t codepoint = NextCodepoint(utf8, i);
            if (codepoint == '\n')
            {
                pen = { position.x, pen.y + lineHeight };
                previous = 0;
                continue;
            }

            const Font::Glyph* glyph = font.FindGlyph(codepoint);
            if (!glyph)
            {
                glyph = font.FindGlyph('?');
                if (!glyph) { continue; }
            }

            if (previous)
            {
                pen.x += font.GetKerning(previous, codepoint) * scale;
            }

            // Snap the pen to whole pixels vertically; horizontal positions
            // benefit from the atlas's 2x oversampling.
            const glm::vec2 origin{ pen.x, std::round(pen.y) };
            PushQuad(
                &font.GetAtlas(),
                origin + glm::vec2{ glyph->x0, glyph->y0 } * scale,
                origin + glm::vec2{ glyph->x1, glyph->y1 } * scale,
                { glyph->u0, glyph->v0 },
                { glyph->u1, glyph->v1 },
                linear
            );

            pen.x += glyph->advance * scale;
            previous = codepoint;
        }
    }

    glm::vec2 UIRenderer::MeasureText(const Font& font, std::string_view utf8, float scale) const
    {
        float width = 0.0f;
        float lineWidth = 0.0f;
        int lines = 1;
        std::uint32_t previous = 0;

        for (std::size_t i = 0; i < utf8.size();)
        {
            const std::uint32_t codepoint = NextCodepoint(utf8, i);
            if (codepoint == '\n')
            {
                width = std::max(width, lineWidth);
                lineWidth = 0.0f;
                ++lines;
                previous = 0;
                continue;
            }
            const Font::Glyph* glyph = font.FindGlyph(codepoint);
            if (!glyph) { glyph = font.FindGlyph('?'); }
            if (!glyph) { continue; }
            if (previous) { lineWidth += font.GetKerning(previous, codepoint) * scale; }
            lineWidth += glyph->advance * scale;
            previous = codepoint;
        }

        width = std::max(width, lineWidth);
        return { width, lines * font.GetLineHeight() * scale };
    }

    std::string UIRenderer::WrapText(
        const Font& font,
        std::string_view utf8,
        float maxWidth,
        float scale
    ) const
    {
        std::string result;
        std::string line;

        std::size_t start = 0;
        while (start <= utf8.size())
        {
            // Next word, keeping explicit newlines as hard breaks.
            std::size_t end = utf8.find_first_of(" \n", start);
            if (end == std::string_view::npos) { end = utf8.size(); }
            const std::string_view word = utf8.substr(start, end - start);

            const std::string candidate = line.empty()
                ? std::string(word)
                : line + ' ' + std::string(word);
            if (!line.empty() && MeasureText(font, candidate, scale).x > maxWidth)
            {
                result += line + '\n';
                line = std::string(word);
            }
            else
            {
                line = candidate;
            }

            if (end < utf8.size() && utf8[end] == '\n')
            {
                result += line + '\n';
                line.clear();
            }
            start = end + 1;
        }

        return result + line;
    }

    bool UIRenderer::Upload(SDL_GPUCommandBuffer* commandBuffer)
    {
        m_uploadedVertices = 0;
        if (m_vertices.empty())
        {
            return true;
        }

        const Uint32 bytes = static_cast<Uint32>(m_vertices.size() * sizeof(Vertex));
        void* mapped = SDL_MapGPUTransferBuffer(m_device, m_transferBuffer, true);
        if (!mapped)
        {
            std::cerr << "Failed to map UI transfer buffer: " << SDL_GetError() << '\n';
            return false;
        }
        std::memcpy(mapped, m_vertices.data(), bytes);
        SDL_UnmapGPUTransferBuffer(m_device, m_transferBuffer);

        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commandBuffer);
        SDL_GPUTransferBufferLocation source{ m_transferBuffer, 0 };
        SDL_GPUBufferRegion destination{ m_vertexBuffer, 0, bytes };
        SDL_UploadToGPUBuffer(copyPass, &source, &destination, true);
        SDL_EndGPUCopyPass(copyPass);

        m_uploadedVertices = static_cast<std::uint32_t>(m_vertices.size());
        return true;
    }

    bool UIRenderer::Render(
        SDL_GPUCommandBuffer* commandBuffer,
        SDL_GPUTexture* target,
        std::uint32_t width,
        std::uint32_t height
    )
    {
        m_screenSize = { static_cast<float>(width), static_cast<float>(height) };
        if (m_uploadedVertices == 0)
        {
            return true;
        }

        // Draw over the finished frame: keep what the post pass wrote.
        SDL_GPUColorTargetInfo colorTarget{};
        colorTarget.texture = target;
        colorTarget.load_op = SDL_GPU_LOADOP_LOAD;
        colorTarget.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass* renderPass =
            SDL_BeginGPURenderPass(commandBuffer, &colorTarget, 1, nullptr);
        if (!renderPass)
        {
            std::cerr << "Failed to begin UI render pass: " << SDL_GetError() << '\n';
            return false;
        }

        SDL_BindGPUGraphicsPipeline(renderPass, m_pipeline);
        const glm::vec4 viewport{ m_screenSize, 0.0f, 0.0f };
        SDL_PushGPUVertexUniformData(commandBuffer, 0, &viewport, sizeof(viewport));

        const SDL_GPUBufferBinding vertices{ m_vertexBuffer, 0 };
        SDL_BindGPUVertexBuffers(renderPass, 0, &vertices, 1);

        for (const Batch& batch : m_batches)
        {
            if (batch.firstVertex >= m_uploadedVertices)
            {
                break;
            }
            const SDL_GPUTextureSamplerBinding binding{
                batch.texture->GetGPUTexture(), m_sampler };
            SDL_BindGPUFragmentSamplers(renderPass, 0, &binding, 1);
            SDL_DrawGPUPrimitives(renderPass, batch.vertexCount, 1, batch.firstVertex, 0);
        }

        SDL_EndGPURenderPass(renderPass);
        return true;
    }

    void UIRenderer::EndFrame()
    {
        m_vertices.clear();
        m_batches.clear();
        m_uploadedVertices = 0;
    }
}
