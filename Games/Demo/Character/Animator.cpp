#include "Character/Animator.h"

#include <algorithm>
#include <charconv>
#include <cmath>

namespace AtomGame
{
    bool AnimCondition::Holds(float actual) const
    {
        switch (op)
        {
        case Op::Less: return actual < value;
        case Op::LessEqual: return actual <= value;
        case Op::Greater: return actual > value;
        case Op::GreaterEqual: return actual >= value;
        case Op::Equal: return actual == value;
        case Op::NotEqual: return actual != value;
        }
        return false;
    }

    std::optional<AnimCondition> ParseCondition(std::string_view text)
    {
        // "param op value", separated by spaces.
        std::vector<std::string_view> words;
        while (!text.empty())
        {
            const std::size_t start = text.find_first_not_of(' ');
            if (start == std::string_view::npos)
            {
                break;
            }
            text.remove_prefix(start);
            const std::size_t end = std::min(text.find(' '), text.size());
            words.push_back(text.substr(0, end));
            text.remove_prefix(end);
        }
        if (words.size() != 3)
        {
            return std::nullopt;
        }
        AnimCondition condition;
        condition.param = std::string(words[0]);
        const std::string_view op = words[1];
        using Op = AnimCondition::Op;
        if (op == "<") condition.op = Op::Less;
        else if (op == "<=") condition.op = Op::LessEqual;
        else if (op == ">") condition.op = Op::Greater;
        else if (op == ">=") condition.op = Op::GreaterEqual;
        else if (op == "==") condition.op = Op::Equal;
        else if (op == "!=") condition.op = Op::NotEqual;
        else return std::nullopt;
        const char* end = words[2].data() + words[2].size();
        if (std::from_chars(words[2].data(), end, condition.value).ptr != end)
        {
            return std::nullopt;
        }
        return condition;
    }

    SyncedBlend MakeSyncedBlend(float durationA, float durationB, float weight)
    {
        weight = std::clamp(weight, 0.0f, 1.0f);
        return SyncedBlend{ weight, durationA + (durationB - durationA) * weight };
    }

    std::optional<std::string> Animator::Bind(const AnimatorData& data, const ClipLookup& lookup,
                                               const NodeLookup& nodes)
    {
        *this = Animator{};
        std::unordered_map<std::string, int> index;
        for (const auto& [name, state] : data.states)
        {
            index[name] = static_cast<int>(m_states.size());
            m_states.push_back(State{ name });
        }
        const auto clip = [&](const std::string& name, ClipInfo& out) -> std::optional<std::string> {
            const auto found = lookup(name);
            if (!found)
            {
                return "no clip '" + name + "'";
            }
            out = *found;
            return std::nullopt;
        };
        for (const auto& [name, source] : data.states)
        {
            State& state = m_states[index[name]];
            state.isBlend = source.clip.empty();
            if (state.isBlend)
            {
                if (source.blendFrom.empty() || source.blendTo.empty() || source.param.empty())
                {
                    return "state '" + name + "' needs a clip, or a blend with a param";
                }
                if (auto error = clip(source.blendFrom, state.from)) return error;
                if (auto error = clip(source.blendTo, state.to)) return error;
            }
            else if (auto error = clip(source.clip, state.clip))
            {
                return error;
            }
            state.param = source.param;
            state.rangeLow = source.rangeLow;
            state.rangeHigh = source.rangeHigh;
            state.loop = source.loop;
            state.nextBlend = source.nextBlend;
            if (!source.inPlace.empty())
            {
                state.pinNode = nodes ? nodes(source.inPlace) : -1;
                if (state.pinNode < 0)
                {
                    return "state '" + name + "': no joint '" + source.inPlace + "'";
                }
            }
            if (!source.next.empty())
            {
                const auto found = index.find(source.next);
                if (found == index.end())
                {
                    return "state '" + name + "': no next state '" + source.next + "'";
                }
                state.next = found->second;
            }
        }
        for (const AnimTransitionData& source : data.transitions)
        {
            Transition transition;
            transition.blend = source.blend;
            transition.when = source.when;
            if (source.from != "*")
            {
                const auto from = index.find(source.from);
                if (from == index.end())
                {
                    return "transition from unknown state '" + source.from + "'";
                }
                transition.from = from->second;
            }
            const auto to = index.find(source.to);
            if (to == index.end())
            {
                return "transition to unknown state '" + source.to + "'";
            }
            transition.to = to->second;
            m_transitions.push_back(std::move(transition));
        }
        for (const AnimEventData& source : data.events)
        {
            ClipInfo info;
            if (auto error = clip(source.clip, info))
            {
                return "event '" + source.name + "': " + *error;
            }
            const float phase = info.duration > 0.0f ? source.time / info.duration : 0.0f;
            if (phase < 0.0f || phase > 1.0f)
            {
                return "event '" + source.name + "' lies outside clip '" + source.clip + "'";
            }
            m_events.push_back(Event{ info.index, phase, source.name });
        }
        const auto initial = index.find(data.initial);
        if (initial == index.end())
        {
            return "no initial state '" + data.initial + "'";
        }
        m_current.state = initial->second;
        return std::nullopt;
    }

    float Animator::GetParam(const std::string& name) const
    {
        const auto found = m_params.find(name);
        return found != m_params.end() ? found->second : 0.0f;
    }

    float Animator::BlendWeight(const State& state) const
    {
        const float span = state.rangeHigh - state.rangeLow;
        const float value = GetParam(state.param);
        return span != 0.0f ? std::clamp((value - state.rangeLow) / span, 0.0f, 1.0f) : 0.0f;
    }

    float Animator::CycleSeconds(const State& state) const
    {
        return state.isBlend
            ? MakeSyncedBlend(state.from.duration, state.to.duration, BlendWeight(state)).duration
            : state.clip.duration;
    }

    void Animator::Advance(Playing& playing, float deltaSeconds) const
    {
        if (playing.state < 0)
        {
            return;
        }
        const State& state = m_states[playing.state];
        const float cycle = CycleSeconds(state);
        if (cycle <= 0.0f)
        {
            return;
        }
        playing.phase += deltaSeconds / cycle;
        if (playing.phase >= 1.0f)
        {
            if (state.loop)
            {
                playing.phase -= std::floor(playing.phase);
            }
            else
            {
                playing.phase = 1.0f;
                playing.finished = true;
            }
        }
    }

    void Animator::Enter(int state, float blend)
    {
        if (blend > 0.0f)
        {
            m_previous = m_current;
            m_fade = 1.0f;
            m_fadeRate = 1.0f / blend;
        }
        else
        {
            m_fade = 0.0f;
        }
        m_current = Playing{ state };
    }

    void Animator::Update(float deltaSeconds)
    {
        if (m_current.state < 0)
        {
            return;
        }
        const float before = m_current.phase;
        const bool wasFinished = m_current.finished;
        Advance(m_current, deltaSeconds);
        if (!wasFinished)
        {
            FireEvents(m_states[m_current.state], before, m_current.phase);
        }
        if (m_fade > 0.0f)
        {
            Advance(m_previous, deltaSeconds);
            m_fade = std::max(0.0f, m_fade - deltaSeconds * m_fadeRate);
        }

        // A one-shot that has ended moves on by itself.
        const State& current = m_states[m_current.state];
        if (m_current.finished && current.next >= 0)
        {
            Enter(current.next, current.nextBlend);
            return;
        }
        for (const Transition& transition : m_transitions)
        {
            if ((transition.from >= 0 && transition.from != m_current.state) || transition.to == m_current.state)
            {
                continue;
            }
            const bool holds = std::all_of(transition.when.begin(), transition.when.end(),
                [&](const AnimCondition& condition) { return condition.Holds(GetParam(condition.param)); });
            if (holds)
            {
                Enter(transition.to, transition.blend);
                return;
            }
        }
    }

    void Animator::FireEvents(const State& state, float from, float to)
    {
        // Blended walk/run: both cycles share the phase, so the clip with
        // more weight speaks for the pair (their feet land together).
        int clip = state.clip.index;
        if (state.isBlend)
        {
            clip = BlendWeight(state) < 0.5f ? state.from.index : state.to.index;
        }
        const bool wrapped = to < from; // looped past the end
        for (const Event& event : m_events)
        {
            if (event.clip != clip)
            {
                continue;
            }
            const bool crossed = wrapped
                ? (event.phase > from || event.phase <= to)
                : (event.phase > from && event.phase <= to);
            if (crossed)
            {
                m_fired.push_back(event.name);
            }
        }
    }

    std::vector<std::string> Animator::TakeEvents()
    {
        std::vector<std::string> fired;
        fired.swap(m_fired);
        return fired;
    }

    bool Animator::ForceState(std::string_view name)
    {
        for (std::size_t i = 0; i < m_states.size(); ++i)
        {
            if (m_states[i].name == name)
            {
                m_current = Playing{ static_cast<int>(i) };
                m_fade = 0.0f;
                return true;
            }
        }
        return false;
    }

    const std::string& Animator::GetStateName() const
    {
        static const std::string none;
        return m_current.state >= 0 ? m_states[m_current.state].name : none;
    }

    void Animator::AppendSamples(const Playing& playing, float weight, std::vector<Atom::ClipSample>& out) const
    {
        if (playing.state < 0 || weight <= 0.0f)
        {
            return;
        }
        const State& state = m_states[playing.state];
        if (!state.isBlend)
        {
            out.push_back({ state.clip.index, playing.phase * state.clip.duration, weight, state.pinNode });
            return;
        }
        // Same phase in both cycles: each clip at its own time for it.
        const float w = BlendWeight(state);
        out.push_back({ state.from.index, playing.phase * state.from.duration, weight * (1.0f - w), state.pinNode });
        out.push_back({ state.to.index, playing.phase * state.to.duration, weight * w, state.pinNode });
    }

    std::vector<Atom::ClipSample> Animator::GetSamples() const
    {
        std::vector<Atom::ClipSample> samples;
        AppendSamples(m_previous, m_fade, samples);
        AppendSamples(m_current, 1.0f - m_fade, samples);
        return samples;
    }
}
