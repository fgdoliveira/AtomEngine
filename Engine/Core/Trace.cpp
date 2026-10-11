#include "Core/Trace.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <map>

namespace Atom::Trace
{
    namespace
    {
        struct State
        {
            std::vector<Event> events;
            std::size_t capacity = 0;
            bool recording = false;
            bool overflowed = false;
            std::uint32_t frame = 0;
            std::uint32_t depth = 0;
            std::uint64_t originNs = 0;
        };

        State& Get()
        {
            static State state;
            return state;
        }

        std::uint64_t NowNs()
        {
            // steady_clock is QueryPerformanceCounter on Windows: ~100 ns
            // resolution, monotonic.
            return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                                  std::chrono::steady_clock::now().time_since_epoch())
                                                  .count());
        }
    }

    void Start(std::size_t capacity)
    {
        State& s = Get();
        s.events.clear();
        s.events.reserve(capacity); // the only allocation
        s.capacity = capacity;
        s.recording = true;
        s.overflowed = false;
        s.frame = 0;
        s.depth = 0;
        s.originNs = NowNs();
    }

    bool IsRecording() { return Get().recording; }
    const std::vector<Event>& Events() { return Get().events; }
    std::uint32_t Frames() { return Get().frame; }
    bool Overflowed() { return Get().overflowed; }

    void Reset()
    {
        State& s = Get();
        s.recording = false;
        s.events.clear();
        s.overflowed = false;
        s.frame = 0;
        s.depth = 0;
    }

    void FrameMark()
    {
        State& s = Get();
        if (s.recording)
        {
            ++s.frame;
        }
    }

    Scope::Scope(const char* name)
    {
        State& s = Get();
        if (!s.recording)
        {
            return;
        }
        if (s.events.size() >= s.capacity)
        {
            s.recording = false; // full: keep what's there, record no more
            s.overflowed = true;
            return;
        }
        m_index = static_cast<std::int64_t>(s.events.size());
        s.events.push_back({ name, NowNs() - s.originNs, 0, s.frame, s.depth });
        ++s.depth;
    }

    Scope::~Scope()
    {
        if (m_index < 0)
        {
            return;
        }
        State& s = Get();
        s.events[static_cast<std::size_t>(m_index)].endNs = NowNs() - s.originNs;
        --s.depth;
    }

    std::vector<ZoneStats> Summarise(const std::vector<Event>& events)
    {
        std::map<std::string, ZoneStats> byName;
        for (const Event& e : events)
        {
            if (e.endNs < e.startNs)
            {
                continue; // still open when the run ended
            }
            const std::uint64_t ns = e.endNs - e.startNs;
            ZoneStats& z = byName[e.name];
            if (z.count == 0)
            {
                z.name = e.name;
                z.minNs = ns;
            }
            ++z.count;
            z.totalNs += ns;
            z.minNs = std::min(z.minNs, ns);
            z.maxNs = std::max(z.maxNs, ns);
        }
        std::vector<ZoneStats> result;
        for (auto& [name, z] : byName)
        {
            result.push_back(z);
        }
        std::sort(result.begin(), result.end(), [](const ZoneStats& a, const ZoneStats& b) { return a.totalNs > b.totalNs; });
        return result;
    }

    std::string ChromeJson(const std::vector<Event>& events)
    {
        // The Trace Event Format's complete events ("ph":"X"), microseconds.
        std::string json = "{\"traceEvents\":[\n";
        char line[256];
        bool first = true;
        for (const Event& e : events)
        {
            if (e.endNs < e.startNs)
            {
                continue;
            }
            std::snprintf(line, sizeof(line),
                          "%s{\"name\":\"%s\",\"ph\":\"X\",\"ts\":%.3f,\"dur\":%.3f,\"pid\":1,\"tid\":1,\"args\":{\"frame\":%u}}",
                          first ? "" : ",\n", e.name, static_cast<double>(e.startNs) / 1000.0,
                          static_cast<double>(e.endNs - e.startNs) / 1000.0, e.frame);
            json += line;
            first = false;
        }
        json += "\n],\"displayTimeUnit\":\"ms\"}\n";
        return json;
    }

    std::string SlowestFrames(const std::vector<Event>& events, std::size_t count)
    {
        // A frame's length: the sum of its outermost zones.
        std::map<std::uint32_t, std::uint64_t> frameNs;
        for (const Event& e : events)
        {
            if (e.depth == 0 && e.endNs >= e.startNs)
            {
                frameNs[e.frame] += e.endNs - e.startNs;
            }
        }
        std::vector<std::pair<std::uint32_t, std::uint64_t>> frames(frameNs.begin(), frameNs.end());
        std::sort(frames.begin(), frames.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        frames.resize(std::min(frames.size(), count));
        std::string text;
        char line[256];
        for (const auto& [frame, ns] : frames)
        {
            std::snprintf(line, sizeof(line), "frame %u: %.3f ms in zones\n", frame, static_cast<double>(ns) / 1e6);
            text += line;
            for (const Event& e : events)
            {
                if (e.frame == frame && e.endNs >= e.startNs && (e.endNs - e.startNs) >= 10000) // >= 10 us
                {
                    std::snprintf(line, sizeof(line), "  %*s%-22s %9.3f ms\n", static_cast<int>(e.depth * 2), "", e.name,
                                  static_cast<double>(e.endNs - e.startNs) / 1e6);
                    text += line;
                }
            }
        }
        return text;
    }

    bool StopAndWrite(const std::string& prefix)
    {
        State& s = Get();
        s.recording = false;
        bool ok = true;
        {
            std::ofstream file(prefix + ".json", std::ios::binary);
            file << ChromeJson(s.events);
            ok = ok && static_cast<bool>(file);
        }
        {
            std::ofstream file(prefix + "-zones.csv", std::ios::binary);
            file << "name,total_ns,counts,mean_ns,min_ns,max_ns\n";
            for (const ZoneStats& z : Summarise(s.events))
            {
                file << z.name << ',' << z.totalNs << ',' << z.count << ',' << (z.count ? z.totalNs / z.count : 0) << ','
                     << z.minNs << ',' << z.maxNs << '\n';
            }
            ok = ok && static_cast<bool>(file);
        }
        {
            std::ofstream file(prefix + "-frames.txt", std::ios::binary);
            file << "frames recorded: " << s.frame << ", zones: " << s.events.size()
                 << (s.overflowed ? " (buffer full: recording stopped early)" : "") << "\n\n"
                 << SlowestFrames(s.events, 5);
            ok = ok && static_cast<bool>(file);
        }
        return ok;
    }
}
