#pragma once

#include "Environment/EnvironmentController.h"
#include "Environment/EnvironmentPresets.h"
#include "Level/LevelManager.h"
#include "Level/ModelCache.h"
#include "Level/SoundLibrary.h"
#include "Level/ViewToggles.h"
#include "Platform/AssetRoots.h"
#include "Scene/Camera.h"

#include <memory>
#include <string>
#include <vector>

namespace Atom
{
    class AudioSystem;
    class Renderer;
}

namespace AtomFramework
{
    // A level shown without its game (v0.0.19): what the workspace's Scene
    // Viewport draws. Levels are data (ADR-012), so any project's levels
    // load here - the Demo's street, the shore - with the environment
    // presets, the sky picture and the view switches the games' F10 had.
    // Game behaviour (riding, dialogue, the player) doesn't run, by design.
    // No ImGui: the workspace presents it; a test could drive it.
    class ScenePreview
    {
    public:
        ScenePreview(Atom::Renderer& renderer, Atom::AudioSystem& audio);
        ~ScenePreview();

        ScenePreview(const ScenePreview&) = delete;
        ScenePreview& operator=(const ScenePreview&) = delete;

        // Where files are found: the project's assets first, then the
        // shared content - as an app's payload lays them out. Unloads the
        // level and reads the environment presets again.
        void SetAssetRoots(std::vector<std::string> roots);
        const AssetRoots& Assets() const { return m_assets; }

        // "" when loaded; otherwise why not. The camera starts at the
        // level's first spawn, at eye height.
        std::string LoadLevel(const std::string& name);
        std::string Reload();
        void Unload();
        Level* GetLevel() { return m_levels ? m_levels->GetLevel() : nullptr; }
        const Level* GetLevel() const { return m_levels ? m_levels->GetLevel() : nullptr; }

        // Each frame: the environment blends, the level animates, and the
        // lighting and the camera go to the renderer with the level's draws.
        void Update(float deltaSeconds);
        void Submit();

        // The environment: "" is the level's own light, else a preset.
        std::vector<std::string> Presets() const;
        const std::string& EnvironmentName() const { return m_environmentName; }
        bool SetEnvironment(const std::string& name, float seconds);
        // After the level's own lighting was edited: shows it again under
        // the preset in use.
        void RefreshEnvironment();
        EnvironmentController& Environment() { return m_environment; }

        // The sky pictures offered (Sky/*.png in the roots); "" restores the level's.
        std::vector<std::string> SkyImages() const;
        bool LoadSkyImage(const std::string& file);

        ViewToggles& View() { return m_view; }
        Atom::Camera& Camera() { return m_camera; }
        // Puts the camera back from `center` so a sphere of `radius` fills the view.
        void Frame(const glm::vec3& center, float radius);

    private:
        class NoSounds final : public SoundLibrary
        {
        public:
            Atom::SoundHandle GetSound(std::string_view /*name*/) const override { return nullptr; }
        };

        Atom::Renderer& m_renderer;
        Atom::AudioSystem& m_audio;
        AssetRoots m_assets;
        NoSounds m_sounds;
        std::unique_ptr<ModelCache> m_models;     // declared before the levels: outlives them
        std::unique_ptr<LevelManager> m_levels;
        EnvironmentPresets m_presets;
        EnvironmentController m_environment;
        std::string m_environmentName;
        ViewToggles m_view;
        Atom::Camera m_camera;
    };
}
