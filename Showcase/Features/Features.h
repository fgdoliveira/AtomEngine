#pragma once

#include <glm/vec3.hpp>

#include <vector>

namespace Showcase
{
    // A capability of the engine, as the Showcase's lens presents it
    // (v0.0.14). The descriptor is plain data, checked by a unit test: its
    // source file must exist and its manual section must be a heading of
    // docs/AtomEngine-Tech-Manual.md. A new engine capability enters here:
    // it gets a callout, an A/B toggle and an entry in the catalog.
    struct FeatureInfo
    {
        const char* id;      // stable: scenarios and the catalog use it
        const char* title;   // what a player sees it as
        const char* system;  // what in the engine produces it
        const char* source;  // where to read it, from the repository root
        int manual;          // the tech manual's section (§)
        const char* entity;  // where the callout points: an entity, or
        glm::vec3 point;     //   this point if none ("" and no point: the whole view)
        bool anchored;
    };

    // In the order of the lens's keys (1-9, 0).
    const std::vector<FeatureInfo>& FeatureCatalog();
}
