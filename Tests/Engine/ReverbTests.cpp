#include "Audio/Reverb.h"

#include <doctest/doctest.h>

#include <cmath>

TEST_CASE("Reverb: silent when off, rings after a click, decays")
{
    Atom::Reverb off;
    off.Configure(0.0f, 1.0f, 0.5f, 48000);
    float l = 0.5f, r = 0.5f;
    off.Process(l, r);
    CHECK(l == 0.5f);

    Atom::Reverb hall;
    hall.Configure(0.4f, 1.5f, 0.7f, 48000);
    float energyEarly = 0.0f, energyLate = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        float left = i == 0 ? 1.0f : 0.0f;
        float right = left;
        hall.Process(left, right);
        const float e = left * left + right * right;
        if (i > 1000 && i < 5000) energyEarly += e;
        if (i > 40000) energyLate += e;
        REQUIRE(std::isfinite(left));
    }
    CHECK(energyEarly > 0.0f);           // a tail after the click
    CHECK(energyLate < energyEarly * 0.1f); // and it dies away
}
