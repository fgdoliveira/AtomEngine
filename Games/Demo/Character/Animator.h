#pragma once

#include "Assets/Skin.h" // ClipSample

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Demo
{
    // The animation state machine (M37), as data. Gameplay sets parameters
    // ("speed", "grounded"); the animator picks the state, crossfades
    // between states, and says which clips to sample at which times and
    // weights. It never touches a model: the clips are looked up once, by
    // name, when it is bound.
    //
    //   "animator": {
    //     "initial": "idle",
    //     "states": {
    //       "idle": { "clip": "Idle" },
    //       "move": { "blend": ["Walk", "Run"], "param": "speed", "range": [1.4, 4.0] },
    //       "jump": { "clip": "Jump", "loop": false, "next": "idle" }
    //     },
    //     "transitions": [
    //       { "from": "idle", "to": "move", "when": ["speed > 0.1"], "blend": 0.25 },
    //       { "from": "*", "to": "jump", "when": ["grounded == 0"], "blend": 0.1 }
    //     ]
    //   }

    struct AnimCondition
    {
        enum class Op { Less, LessEqual, Greater, GreaterEqual, Equal, NotEqual };
        std::string param;
        Op op = Op::Greater;
        float value = 0.0f;

        bool Holds(float actual) const;
    };

    // "speed > 0.1" -> condition; nullopt if malformed.
    std::optional<AnimCondition> ParseCondition(std::string_view text);

    struct AnimStateData
    {
        std::string clip;                   // a single clip, or...
        std::string blendFrom, blendTo;     // ...two, blended by a parameter
        std::string param;                  // the blend parameter
        float rangeLow = 0.0f;              // param here: all blendFrom
        float rangeHigh = 1.0f;             // param here: all blendTo
        bool loop = true;
        std::string next;                   // one-shot: where to go when it ends
        float nextBlend = 0.2f;
        std::string inPlace;                // M38: a joint pinned at rest (root motion off)
    };

    // Something that happens at a moment of a clip (M38): a foot touching
    // down, for a footstep sound.
    struct AnimEventData
    {
        std::string clip;
        std::string name;
        float time = 0.0f; // seconds into the clip
    };

    struct AnimTransitionData
    {
        std::string from; // a state, or "*" for any (but the target itself)
        std::string to;
        std::vector<AnimCondition> when; // all must hold
        float blend = 0.2f;              // crossfade seconds
    };

    struct AnimatorData
    {
        std::string initial;
        std::unordered_map<std::string, AnimStateData> states;
        std::vector<AnimTransitionData> transitions; // first match wins
        std::vector<AnimEventData> events;
    };

    // Walk and run cycles of different lengths, blended: both play at the
    // same *phase* (0..1 through their cycle), and the blended cycle lasts
    // in between, so the feet land together and don't slide (M37).
    struct SyncedBlend
    {
        float weight = 0.0f;   // 0 = all A, 1 = all B
        float duration = 0.0f; // of the blended cycle
    };
    SyncedBlend MakeSyncedBlend(float durationA, float durationB, float weight);

    class Animator
    {
    public:
        struct ClipInfo
        {
            int index = -1;
            float duration = 0.0f;
        };
        using ClipLookup = std::function<std::optional<ClipInfo>(std::string_view)>;
        using NodeLookup = std::function<int(std::string_view)>; // -1 if none

        // Resolves clip, state and joint names; on failure returns the problem.
        std::optional<std::string> Bind(const AnimatorData& data, const ClipLookup& lookup,
                                         const NodeLookup& nodes = {});

        // Events the current state passed through since the last call (a
        // state fading out fires none: its feet are leaving the ground).
        std::vector<std::string> TakeEvents();

        void SetParam(const std::string& name, float value) { m_params[name] = value; }
        float GetParam(const std::string& name) const;
        void Update(float deltaSeconds);

        // Jump straight to a state (no crossfade); false if unknown.
        bool ForceState(std::string_view name);
        const std::string& GetStateName() const;
        bool IsBlending() const { return m_fade > 0.0f; }

        // What to draw: the current state's clips, and while crossfading
        // the previous state's, weights summing to 1.
        std::vector<Atom::ClipSample> GetSamples() const;

        bool IsEnabled() const { return m_enabled; }
        void SetEnabled(bool on) { m_enabled = on; }

    private:
        struct State
        {
            std::string name;
            ClipInfo clip, from, to;
            bool isBlend = false;
            std::string param;
            float rangeLow = 0.0f, rangeHigh = 1.0f;
            bool loop = true;
            int next = -1;
            float nextBlend = 0.2f;
            int pinNode = -1;
        };
        struct Event
        {
            int clip = -1;
            float phase = 0.0f; // time / clip duration
            std::string name;
        };
        struct Transition
        {
            int from = -1; // -1 = any
            int to = -1;
            std::vector<AnimCondition> when;
            float blend = 0.2f;
        };
        // A state playing: where it is in its cycle.
        struct Playing
        {
            int state = -1;
            float phase = 0.0f; // 0..1
            bool finished = false; // one-shot reached its end
        };

        float BlendWeight(const State& state) const;
        float CycleSeconds(const State& state) const;
        void Advance(Playing& playing, float deltaSeconds) const;
        void AppendSamples(const Playing& playing, float weight, std::vector<Atom::ClipSample>& out) const;
        void Enter(int state, float blend);
        void FireEvents(const State& state, float from, float to);

        std::vector<State> m_states;
        std::vector<Transition> m_transitions;
        std::vector<Event> m_events;
        std::vector<std::string> m_fired;
        std::unordered_map<std::string, float> m_params;
        Playing m_current;
        Playing m_previous;
        float m_fade = 0.0f;     // previous state's weight, falling to 0
        float m_fadeRate = 0.0f; // per second
        bool m_enabled = true;
    };
}
