#include "Interaction/Sequence.h"

#include <algorithm>
#include <iostream>

namespace AtomFramework
{
    bool SequenceRunner::Start(const Sequence& sequence, const std::string& name)
    {
        if (m_running)
        {
            return false;
        }
        m_steps = sequence;
        m_name = name;
        m_index = 0;
        m_elapsed = 0.0f;
        m_stepStarted = false;
        m_running = !m_steps.empty();
        std::cout << "Sequence '" << name << "' started (" << m_steps.size() << " steps)\n";
        return m_running;
    }

    void SequenceRunner::Stop()
    {
        m_running = false;
        m_steps.clear();
    }

    void SequenceRunner::Update(float deltaSeconds, const SequenceHooks& hooks)
    {
        using Type = SequenceStep::Type;
        float budget = deltaSeconds;
        while (m_running)
        {
            if (m_index >= m_steps.size())
            {
                std::cout << "Sequence '" << m_name << "' finished\n";
                Stop();
                return;
            }
            const SequenceStep& step = m_steps[m_index];
            const bool firstFrame = !m_stepStarted;
            m_stepStarted = true;

            switch (step.type)
            {
            case Type::Wait:
            case Type::MoveEntity:
            {
                if (firstFrame && step.type == Type::MoveEntity)
                {
                    m_moveFrom = hooks.entityPosition ? hooks.entityPosition(step.entity).value_or(step.to) : step.to;
                }
                m_elapsed += budget;
                budget = 0.0f;
                const float t = step.seconds > 0.0f ? std::min(m_elapsed / step.seconds, 1.0f) : 1.0f;
                if (step.type == Type::MoveEntity && hooks.moveEntity)
                {
                    // Ease out (cubic): fast at first, braking to a stop.
                    const float eased = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
                    hooks.moveEntity(step.entity, m_moveFrom + (step.to - m_moveFrom) * eased);
                }
                if (t < 1.0f)
                {
                    return; // this step takes more time
                }
                // Time left over carries into the next step.
                budget = m_elapsed - step.seconds;
                break;
            }
            case Type::Message:
                if (hooks.message) hooks.message(step.text);
                break;
            case Type::SetFlag:
                if (hooks.setFlag) hooks.setFlag(step.text);
                break;
            case Type::Show:
            case Type::Hide:
                if (hooks.setVisible) hooks.setVisible(step.entity, step.type == Type::Show);
                break;
            case Type::PlaySound:
                if (hooks.playSound) hooks.playSound(step.text, step.entity, step.gain, step.loop);
                break;
            case Type::PlayAnimation:
                if (hooks.playAnimation && !hooks.playAnimation(step.entity, step.clip))
                {
                    std::cerr << "Sequence '" << m_name << "': cannot play '" << step.clip << "' on '" << step.entity << "'\n";
                }
                break;
            case Type::ChangeLevel:
                if (hooks.changeLevel) hooks.changeLevel(step.text, step.clip);
                break;
            }
            ++m_index;
            m_elapsed = 0.0f;
            m_stepStarted = false;
        }
    }
}
