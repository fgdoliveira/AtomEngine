#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Demo
{
    // A conversation as data. Content lives in Assets/Dialogue/*.json; C++
    // only knows how to walk it (DialogueRunner) and draw it (DialogueView).
    //
    // Flow: each node shows one line. A node with choices waits for one;
    // a node without choices continues to `next`. "end" (or no `next`)
    // closes the conversation. Flags gate choices (`requires`,
    // `requiresNot`) and record outcomes (`sets`).

    inline constexpr std::string_view DialogueEnd = "end";

    struct DialogueChoice
    {
        std::string text;
        std::string next;
        std::string requiredFlag;   // shown only if this flag is set ("requires")
        std::string forbiddenFlag;  // hidden if this flag is set ("requiresNot")
        std::string setsFlag;       // flag set when chosen ("sets")
    };

    struct DialogueNode
    {
        std::string id;
        std::string speaker;
        std::string text;
        std::string next;           // used when there are no choices
        std::string setsFlag;       // flag set when the node is shown ("sets")
        std::vector<DialogueChoice> choices;
    };

    struct Dialogue
    {
        std::string id;
        std::string start;
        std::unordered_map<std::string, DialogueNode> nodes;

        const DialogueNode* Find(std::string_view nodeId) const
        {
            const auto found = nodes.find(std::string(nodeId));
            return found != nodes.end() ? &found->second : nullptr;
        }
    };

    struct DialogueParseResult
    {
        std::optional<Dialogue> dialogue;
        std::string error; // empty on success
    };

    // Parses and validates (unique ids, start exists, every `next` points to
    // a node or "end").
    DialogueParseResult ParseDialogue(std::string_view json);
    DialogueParseResult LoadDialogueFile(const std::string& path);

    // All dialogues in a folder, keyed by id.
    class DialogueLibrary
    {
    public:
        // Loads every *.json in `directory`; reports and skips broken files.
        std::size_t LoadDirectory(const std::string& directory);
        void Add(Dialogue dialogue);
        const Dialogue* Find(std::string_view id) const;

    private:
        std::unordered_map<std::string, Dialogue> m_dialogues;
    };
}
