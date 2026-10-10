#pragma once

#include <glm/vec3.hpp>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace AtomFramework
{
    // Action sequences (M26): timed steps written as data in the level file
    // ("sequences"), started by an interaction. While one runs the player is
    // frozen and nothing else can be started; the bus arriving at a stop is
    // the first one.
    //
    //   wait         seconds
    //   message      text
    //   setFlag      flag
    //   show / hide  entity (a hidden entity isn't drawn, nor what rides on it)
    //   playSound    sound [entity: follows it] [gain] [loop]
    //   playAnimation entity clip
    //   moveEntity   entity to seconds (eases out, like a vehicle braking)
    //   changeLevel  level [spawn] - always the last step
    struct SequenceStep
    {
        enum class Type
        {
            Wait,
            Message,
            SetFlag,
            Show,
            Hide,
            PlaySound,
            PlayAnimation,
            MoveEntity,
            ChangeLevel,
        };

        Type type = Type::Wait;
        float seconds = 0.0f;
        std::string text;   // message; flag; sound; level
        std::string entity;
        std::string clip;   // animation; spawn for changeLevel
        glm::vec3 to{ 0.0f };
        float gain = 1.0f;
        bool loop = false;
    };

    using Sequence = std::vector<SequenceStep>;

    // What a sequence can do to the game, supplied by its owner.
    struct SequenceHooks
    {
        std::function<void(const std::string& text)> message;
        std::function<void(const std::string& flag)> setFlag;
        std::function<void(const std::string& entity, bool visible)> setVisible;
        std::function<void(const std::string& sound, const std::string& entity, float gain, bool loop)> playSound;
        std::function<bool(const std::string& entity, const std::string& clip)> playAnimation;
        std::function<std::optional<glm::vec3>(const std::string& entity)> entityPosition;
        std::function<void(const std::string& entity, const glm::vec3& position)> moveEntity;
        std::function<void(const std::string& level, const std::string& spawn)> changeLevel;
    };

    class SequenceRunner
    {
    public:
        // False if one is already running (a sequence can't start twice).
        bool Start(const Sequence& sequence, const std::string& name);
        void Stop();
        bool IsRunning() const { return m_running; }
        const std::string& GetName() const { return m_name; }

        // Runs instant steps until one takes time; advances that one.
        void Update(float deltaSeconds, const SequenceHooks& hooks);

    private:
        Sequence m_steps;
        std::string m_name;
        std::size_t m_index = 0;
        float m_elapsed = 0.0f;       // in the current step
        bool m_stepStarted = false;
        glm::vec3 m_moveFrom{ 0.0f };
        bool m_running = false;
    };
}
