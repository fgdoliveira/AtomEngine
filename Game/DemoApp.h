#pragma once

#include "Core/Application.h"

namespace AtomGame
{
    class DemoApp final : public Atom::Application
    {
    protected:
        bool OnInitialize() override;
        void OnUpdate(float deltaSeconds) override;
    };
}
