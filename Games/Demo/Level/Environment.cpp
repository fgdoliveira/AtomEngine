#include "Level/Environment.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>

namespace Demo
{
    namespace
    {
        glm::vec3 Direction(const glm::vec3& a, const glm::vec3& b, float t)
        {
            // Mix the unit vectors, then renormalise: shorter in the middle,
            // but always a direction. Opposite directions have no middle.
            const glm::vec3 mixed = glm::mix(glm::normalize(a), glm::normalize(b), t);
            const float length = glm::length(mixed);
            if (length < 1e-4f)
            {
                return t < 0.5f ? a : b;
            }
            return mixed / length;
        }
    }

    EnvironmentState Blend(const EnvironmentState& a, const EnvironmentState& b, float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        const EnvironmentState& nearer = t < 0.5f ? a : b;

        EnvironmentState result;
        result.sunDirection = Direction(a.sunDirection, b.sunDirection, t);
        result.sunColor = glm::mix(a.sunColor, b.sunColor, t);
        result.skyColor = glm::mix(a.skyColor, b.skyColor, t);
        result.groundColor = glm::mix(a.groundColor, b.groundColor, t);
        result.fogColor = glm::mix(a.fogColor, b.fogColor, t);

        if (a.fogDensity && b.fogDensity)
        {
            result.fogDensity = glm::mix(*a.fogDensity, *b.fogDensity, t);
        }
        else
        {
            result.fogDensity = nearer.fogDensity;
        }

        if (a.sky && b.sky)
        {
            SkyGradient sky;
            sky.zenith = glm::mix(a.sky->zenith, b.sky->zenith, t);
            sky.horizon = glm::mix(a.sky->horizon, b.sky->horizon, t);
            sky.sunSize = glm::mix(a.sky->sunSize, b.sky->sunSize, t);
            sky.sunGlow = glm::mix(a.sky->sunGlow, b.sky->sunGlow, t);
            result.sky = sky;
        }
        else
        {
            result.sky = nearer.sky;
        }
        result.water.shallow = glm::mix(a.water.shallow, b.water.shallow, t);
        result.water.deep = glm::mix(a.water.deep, b.water.deep, t);
        result.water.skyReflection = glm::mix(a.water.skyReflection, b.water.skyReflection, t);
        result.water.ripple = glm::mix(a.water.ripple, b.water.ripple, t);
        result.water.glint = glm::mix(a.water.glint, b.water.glint, t);
        result.water.reflection = nearer.water.reflection;
        result.rain = glm::mix(a.rain, b.rain, t);
        result.wind = glm::mix(a.wind, b.wind, t);
        return result;
    }
}
