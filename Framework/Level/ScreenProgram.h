#pragma once

#include "Level/LevelData.h"
#include "Platform/AssetRoots.h"

#include <functional>
#include <memory>
#include <string>

namespace Atom
{
    class UIRenderer;
}

namespace AtomFramework
{
    // What a live screen (M27) shows: a small program stepped at a fixed
    // 60 Hz that draws each frame into the screen's render texture, which
    // the screen material samples. The level owns the screen and its clock;
    // the app supplies the programs (v0.0.14, Level::Services::screens) -
    // the demo's are pachinko attract loops.
    class ScreenProgram
    {
    public:
        virtual ~ScreenProgram() = default;

        virtual int Width() const = 0;  // the render texture's size, in pixels
        virtual int Height() const = 0;
        virtual void Step() = 0;        // one 1/60 s tick
        virtual void Draw(Atom::UIRenderer& canvas) const = 0;
    };

    // Builds the program for one screen of a level file. Null, with `error`
    // set, when it can't (a missing playfield, say): the level then fails
    // to load, as for any missing file.
    using ScreenFactory = std::function<std::unique_ptr<ScreenProgram>(
        const ScreenData& screen, const AtomFramework::AssetRoots& assets, std::string& error)>;
}
