#include "World/LiveEffects.h"

#include <algorithm>
#include <cmath>

namespace AtomGame
{
    float FlickerFactor(float time, const glm::vec3& seed, float amount)
    {
        if (amount <= 0.0f)
        {
            return 1.0f;
        }
        // Three incommensurate sines: their product only rarely peaks, so
        // the drops are short and never fall into a regular rhythm.
        const float phase = seed.x * 13.1f + seed.y * 5.7f + seed.z * 7.3f;
        const float t = time * 7.0f + phase;
        const float wobble = std::sin(t) * std::sin(t * 2.3f + 1.0f) * std::sin(t * 0.37f);
        return wobble > 0.55f ? 1.0f - amount : 1.0f;
    }

    MoverState EvaluateMover(float travelSeconds, float waitSeconds, float time)
    {
        travelSeconds = std::max(travelSeconds, 0.001f);
        waitSeconds = std::max(waitSeconds, 0.0f);
        const float leg = waitSeconds + travelSeconds;
        const float phase = std::fmod(std::max(time, 0.0f), 2.0f * leg);

        if (phase < waitSeconds)
        {
            return { 0.0f, false };
        }
        if (phase < leg)
        {
            return { (phase - waitSeconds) / travelSeconds, true };
        }
        if (phase < leg + waitSeconds)
        {
            return { 1.0f, false };
        }
        return { 1.0f - (phase - leg - waitSeconds) / travelSeconds, true };
    }

    float StepTowards(float current, float target, float deltaSeconds, float seconds)
    {
        if (seconds <= 0.0f)
        {
            return target;
        }
        const float step = deltaSeconds / seconds;
        return current < target ? std::min(current + step, target) : std::max(current - step, target);
    }
}
