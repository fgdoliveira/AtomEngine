#include "Preview/ScenePreview.h"

#include "Core/Log.h"
#include "Renderer/Renderer.h"

#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

namespace AtomFramework
{
    namespace
    {
        constexpr float EyeHeight = 1.6f;
    }

    ScenePreview::ScenePreview(Atom::Renderer& renderer, Atom::AudioSystem& audio)
        : m_renderer(renderer), m_audio(audio)
    {
        m_camera.farPlane = 400.0f; // a preview looks further than a player walking
    }

    ScenePreview::~ScenePreview()
    {
        Unload();
    }

    void ScenePreview::SetAssetRoots(std::vector<std::string> roots)
    {
        Unload();
        m_assets = AssetRoots(std::move(roots));
        m_models = std::make_unique<ModelCache>();
        m_levels = std::make_unique<LevelManager>(
            Level::Services{ m_renderer, m_audio, m_sounds, m_assets, *m_models, ScreenFactory{} });
        m_levels->onLoaded = [this](Level& level, const SpawnPoint& spawn) {
            m_camera.SetPosition(spawn.position + glm::vec3{ 0.0f, EyeHeight, 0.0f });
            m_camera.SetRotation(glm::radians(spawn.yawDegrees), 0.0f);
            m_environmentName.clear();
            m_environment.Reset(m_presets.Resolve(&level, ""));
        };
        m_levels->onReloaded = [this](Level&) { RefreshEnvironment(); };
        m_presets = EnvironmentPresets{};
        for (const std::string& problem : m_presets.Load(m_assets.Resolve("Environments")))
        {
            ATOM_LOG_WARN("Preview") << "Environments/" << problem << '\n';
        }
        m_view = ViewToggles{};
    }

    std::string ScenePreview::LoadLevel(const std::string& name)
    {
        if (!m_levels)
        {
            return "No project's assets to load from.";
        }
        if (!m_levels->Load(name, ""))
        {
            return "Level '" + name + "' didn't load - the log says why (Validate checks it).";
        }
        return {};
    }

    std::string ScenePreview::Reload()
    {
        return m_levels && m_levels->GetLevel() ? m_levels->Reload() : std::string("No level loaded.");
    }

    void ScenePreview::Unload()
    {
        // The level's GPU resources (and the sky picture it may borrow) go
        // first, then the models they shared.
        m_view.skyImage.reset();
        m_view.skyImageName.clear();
        m_levels.reset();
        m_models.reset();
    }

    void ScenePreview::Update(float deltaSeconds)
    {
        m_environment.Update(deltaSeconds);
        Level* level = GetLevel();
        if (level)
        {
            m_levels->Update(deltaSeconds);
            level->Update(deltaSeconds, m_camera.GetPosition());
        }
        // Every frame: an environment blend changes it continuously, and
        // the panels edit it live.
        m_renderer.SetLighting(SceneLightingFor(level, m_environment.Current(), m_view));
    }

    void ScenePreview::Submit()
    {
        m_renderer.SetCamera(m_camera.GetViewMatrix(), m_camera.verticalFov, m_camera.nearPlane, m_camera.farPlane);
        if (const Level* level = GetLevel())
        {
            level->Submit(m_renderer, m_camera.GetPosition());
        }
    }

    std::vector<std::string> ScenePreview::Presets() const
    {
        return m_presets.Offered(GetLevel());
    }

    bool ScenePreview::SetEnvironment(const std::string& name, float seconds)
    {
        const std::string preset = name == "level" ? std::string{} : name;
        if (!preset.empty() && !m_presets.Has(preset))
        {
            return false;
        }
        m_environmentName = preset;
        m_environment.SwitchTo(m_presets.Resolve(GetLevel(), preset), seconds);
        return true;
    }

    void ScenePreview::RefreshEnvironment()
    {
        m_environment.Reset(m_presets.Resolve(GetLevel(), m_environmentName));
    }

    std::vector<std::string> ScenePreview::SkyImages() const
    {
        return m_assets.List("Sky", ".png");
    }

    bool ScenePreview::LoadSkyImage(const std::string& file)
    {
        return AtomFramework::LoadSkyImage(m_renderer, m_assets.Resolve("Sky/" + file), file, m_view);
    }

    void ScenePreview::Frame(const glm::vec3& center, float radius)
    {
        // Back along the current view direction, far enough that the sphere
        // fits the vertical field of view.
        const float distance = std::max(radius, 0.25f) / std::sin(m_camera.verticalFov * 0.5f);
        m_camera.SetPosition(center - m_camera.GetForward() * distance);
    }
}
