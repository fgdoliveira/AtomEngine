#pragma once

#include <memory>
#include <span>
#include <string>
#include <vector>

namespace Atom
{
    class Font;
    class Renderer;
    class UIRenderer;

    // The F1 overlay (M82; drawn by the engine itself since v0.0.19): a
    // corner panel of text lines every app shares - the engine adds its
    // lines first each frame, the app appends its own. A readout, not a
    // window: it never takes input. Drawn with the engine's UI renderer and
    // its own embedded font (ProggyClean), so it needs no asset and no
    // tool library: the development tools live in the workspace now.
    class PerfOverlay
    {
    public:
        PerfOverlay();
        ~PerfOverlay();
        PerfOverlay(const PerfOverlay&) = delete;
        PerfOverlay& operator=(const PerfOverlay&) = delete;

        bool Initialize(Renderer& renderer);
        void Shutdown(); // before the renderer: its font is a GPU texture

        bool IsInitialized() const { return m_font != nullptr; }
        bool IsOverlayVisible() const { return IsInitialized() && m_visible; }
        void SetOverlayVisible(bool visible) { m_visible = visible; }
        void Toggle() { m_visible = !m_visible; }

        // Cleared at the start of each frame.
        void BeginFrame() { m_lines.clear(); }
        void AddOverlayLine(std::string line) { m_lines.push_back(std::move(line)); }
        const std::vector<std::string>& Lines() const { return m_lines; }

        // Onto this frame's UI, over whatever the app drew.
        void Draw(UIRenderer& ui) const;

        // The embedded TrueType bytes (generated at configure time).
        static std::span<const unsigned char> FontData();

    private:
        std::unique_ptr<Font> m_font;
        bool m_visible = false;
        std::vector<std::string> m_lines;
    };
}
