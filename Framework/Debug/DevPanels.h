#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace Atom
{
    class Camera;
    class DevTools;
    class Renderer;
}

namespace AtomFramework
{
    class Atmosphere;
    class EnvironmentController;
    class GameWorld;
    class LevelManager;
    struct ViewToggles;

    // What the shared panels look at and change. Everything but the
    // renderer and the tools is optional: a panel without its subject
    // isn't drawn (DRIFT has no level, so no Lighting or Level panel).
    struct DevContext
    {
        Atom::Renderer* renderer = nullptr;
        Atom::DevTools* tools = nullptr;
        Atom::Camera* camera = nullptr;
        ViewToggles* view = nullptr;
        Atmosphere* particles = nullptr;
        LevelManager* levels = nullptr;
        EnvironmentController* environment = nullptr;
        std::vector<std::string> presets;  // the environments offered here
        std::string environmentName;       // "" is the level's own light

        // The app's switches (TestHooks::Set): every widget applies through
        // it, so the panels and the scenarios stay one path. Without one,
        // the view switches apply directly (ApplyViewSwitch).
        std::function<bool(const std::string&, const std::string&)> set;
        // The app's "set environment <name> <seconds>".
        std::function<bool(const std::string&, float)> setEnvironment;
        // After a live edit of the level's light or the environment: the
        // app re-resolves its environment and reapplies the lighting.
        std::function<void()> refreshEnvironment;
        std::function<void()> applyLighting;
        // On/off switches the app adds to the Render panel (label, name, state).
        struct Toggle
        {
            const char* label;
            const char* what;
            bool on;
        };
        std::vector<Toggle> toggles;
    };

    // The developer tools' panels (v0.0.14, M89; the demo's since M41):
    // what every app shows under F10 - Frame, Render, Lighting,
    // Environment, Level - plus the panels an app adds. One layout and one
    // style, so F10 is the same tool in every app.
    //
    // ImGui, immediate mode: each frame the panels are described again from
    // the current state; a widget's return value says it changed.
    class DevPanels
    {
    public:
        // An app's own panel: drawn inside Begin/End under `name`, after
        // the shared ones. `column` places it: 0 left, 1 right, 2 centre.
        void Add(std::string name, int column, std::function<void()> draw);

        // Every frame (it records the frame time even while hidden); draws
        // when the tools are visible and an ImGui frame is running.
        void Draw(const DevContext& context, float deltaSeconds);

        // "set devtools_collapsed on|off": every panel, for one frame.
        void CollapseNext(bool collapsed) { m_collapse = collapsed; }
        // How many panels the last frame drew (the "devtools_panels" stat).
        int PanelsDrawn() const { return m_drawn; }

    private:
        void DrawFrame(const DevContext& context);
        void DrawRender(const DevContext& context);
        void DrawLighting(const DevContext& context);
        void DrawEnvironment(const DevContext& context);
        void DrawLevel(const DevContext& context);
        bool Begin(const char* name, int column, float width, float height);
        static bool Apply(const DevContext& context, const std::string& what, const std::string& value);

        struct Panel
        {
            std::string name;
            int column;
            std::function<void()> draw;
        };
        std::vector<Panel> m_panels;
        std::array<float, 240> m_frameHistory{}; // ms, a ring
        std::size_t m_frameHistoryNext = 0;
        std::optional<bool> m_collapse;
        float m_environmentSeconds = 3.0f;
        int m_drawn = 0;
        std::array<float, 3> m_columnY{}; // where the next panel in each column starts
    };
}
