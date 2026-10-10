#include "Testing/BasicTestHooks.h"

#include "Character/PlayerController.h"
#include "Level/Level.h"
#include "Level/LevelManager.h"
#include "Level/ViewToggles.h"
#include "World/GameWorld.h"

#include "Audio/AudioSystem.h"
#include "Physics/CollisionWorld.h"
#include "Renderer/Renderer.h"
#include "Scene/Camera.h"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <cmath>
#include <iostream>

namespace AtomFramework
{
    const Entity* BasicTestHooks::Find(const std::string& name) const
    {
        const Level* level = m_view.levels ? m_view.levels->GetLevel() : nullptr;
        const Entity* found = nullptr;
        if (level)
        {
            level->GetWorld().ForEach([&](EntityId, const Entity& entity) {
                if (!found && entity.name == name)
                {
                    found = &entity;
                }
            });
        }
        return found;
    }

    bool BasicTestHooks::TeleportTo(const std::string& name, float distance)
    {
        const Entity* entity = Find(name);
        if (!entity)
        {
            return false;
        }
        // `distance` metres from it, on the side the player is on, on the floor.
        glm::vec3 away = m_view.player->GetFeetPosition() - entity->position;
        away.y = 0.0f;
        const float length = glm::length(away);
        away = length > 0.01f ? away / length : glm::vec3{ 0.0f, 0.0f, 1.0f };
        glm::vec3 feet = entity->position + away * distance;
        if (const Level* level = m_view.levels->GetLevel())
        {
            feet.y = level->GetCollision().FindFloor({ feet.x, entity->position.y + 3.0f, feet.z }, 20.0f).value_or(0.0f);
        }
        m_view.player->Teleport(feet, *m_view.camera);
        return Face(name);
    }

    void BasicTestHooks::Teleport(const glm::vec3& feet, float yawDegrees)
    {
        m_view.player->Teleport(feet, *m_view.camera);
        m_view.camera->SetRotation(glm::radians(yawDegrees), 0.0f);
    }

    bool BasicTestHooks::Face(const std::string& name)
    {
        const Entity* entity = Find(name);
        if (!entity)
        {
            return false;
        }
        const glm::vec3 offset = entity->position + glm::vec3{ 0.0f, 1.2f, 0.0f } - m_view.camera->GetPosition();
        m_view.camera->SetRotation(std::atan2(offset.x, -offset.z),
                                   std::atan2(offset.y, std::sqrt(offset.x * offset.x + offset.z * offset.z)));
        return true;
    }

    std::string BasicTestHooks::LevelName() const
    {
        const Level* level = m_view.levels ? m_view.levels->GetLevel() : nullptr;
        return level ? level->GetName() : std::string{};
    }

    std::string BasicTestHooks::ModeName() const
    {
        return m_view.levels && m_view.levels->IsTransitioning() ? "transitioning" : "exploring";
    }

    std::size_t BasicTestHooks::VoiceCount() const
    {
        return m_view.audio ? m_view.audio->GetLoopingVoiceCount() : 0;
    }

    std::optional<float> BasicTestHooks::AnimationTime(const std::string& name) const
    {
        const Entity* entity = Find(name);
        return entity && entity->animated ? std::optional<float>(entity->animated->time) : std::nullopt;
    }

    bool BasicTestHooks::AnimationPlaying(const std::string& name) const
    {
        const Entity* entity = Find(name);
        return entity && entity->animated && entity->animated->playing;
    }

    std::string BasicTestHooks::ReloadLevel()
    {
        return m_view.levels->Reload();
    }

    void BasicTestHooks::RequestLevel(const std::string& level, const std::string& spawn)
    {
        m_view.levels->RequestChange(level, spawn);
    }

    glm::vec3 BasicTestHooks::FeetPosition() const
    {
        return m_view.player->GetFeetPosition();
    }

    void BasicTestHooks::Log(const std::string& text)
    {
        std::cout << "[test] " << text << '\n';
    }

    std::string BasicTestHooks::Capture(const std::string& stem, bool includeUi)
    {
        const std::string path = m_view.outputRoot + "out/img/" + stem + ".png";
        m_view.renderer->RequestCapture(path, includeUi);
        return path;
    }

    bool BasicTestHooks::CapturePending() const
    {
        return m_view.renderer->IsCapturePending();
    }

    bool BasicTestHooks::Set(const std::string& what, const std::string& value)
    {
        // The view switches every app shares (msaa, scale, fog, fov, shadows...).
        ViewToggles fallback;
        return ApplyViewSwitch(*m_view.renderer, m_view.toggles ? *m_view.toggles : fallback, m_view.camera,
                               nullptr, what, value);
    }

    std::optional<float> BasicTestHooks::Stat(const std::string& name) const
    {
        const Atom::FrameStats& stats = m_view.renderer->GetLastFrameStats();
        if (name == "draws") return static_cast<float>(stats.drawn);
        if (name == "shadow_draws") return static_cast<float>(stats.shadowDrawn);
        return std::nullopt;
    }
}
