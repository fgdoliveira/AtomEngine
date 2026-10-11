#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Atom::Trace
{
    // A minimal CPU tracer (v0.0.15, M96; ADR-010): the profiling zones
    // (Core/Profile.h with -DATOM_TRACE=ON), recorded into a fixed buffer
    // in memory and written when the run ends:
    //   <prefix>.json          Chrome trace events - open in ui.perfetto.dev
    //   <prefix>-zones.csv     per zone: count, total, mean, min, max
    //   <prefix>-frames.txt    the slowest frames, each with its zones
    // Main thread only (the engine's zones are); no allocation while
    // recording once started; recording stops when the buffer is full.

    struct Event
    {
        const char* name;       // a string literal: never copied
        std::uint64_t startNs;
        std::uint64_t endNs;
        std::uint32_t frame;
        std::uint32_t depth;    // 0 at the outermost zone
    };

    // Starts recording with room for `capacity` zones (32 bytes each).
    void Start(std::size_t capacity = 1u << 20);
    // Stops and writes the three files; false if any couldn't be written.
    bool StopAndWrite(const std::string& prefix);
    bool IsRecording();
    void FrameMark();

    // What's been recorded (tests, and the writers).
    const std::vector<Event>& Events();
    std::uint32_t Frames();
    bool Overflowed();
    void Reset(); // stop and forget, without writing

    // Pure functions over events (unit-tested).
    struct ZoneStats
    {
        std::string name;
        std::uint64_t count = 0;
        std::uint64_t totalNs = 0;
        std::uint64_t minNs = 0;
        std::uint64_t maxNs = 0;
    };
    std::vector<ZoneStats> Summarise(const std::vector<Event>& events);
    std::string ChromeJson(const std::vector<Event>& events);
    // The `count` frames whose outermost zones took longest, with their zones.
    std::string SlowestFrames(const std::vector<Event>& events, std::size_t count);

    // The zone itself: RAII - it opens when made, closes when destroyed.
    class Scope
    {
    public:
        explicit Scope(const char* name);
        ~Scope();
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

    private:
        std::int64_t m_index = -1; // into the event buffer; -1: not recorded
    };
}
