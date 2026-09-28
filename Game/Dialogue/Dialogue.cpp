#include "Dialogue/Dialogue.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace AtomGame
{
    namespace
    {
        using Json = nlohmann::json;

        std::string OptionalString(const Json& object, const char* key)
        {
            const auto found = object.find(key);
            return found != object.end() && found->is_string()
                ? found->get<std::string>()
                : std::string{};
        }

        bool IsEnd(const std::string& next)
        {
            return next.empty() || next == DialogueEnd;
        }
    }

    DialogueParseResult ParseDialogue(std::string_view text)
    {
        // Parse without exceptions: malformed content is a data error to
        // report, not a crash.
        const Json root = Json::parse(text, nullptr, false);
        if (root.is_discarded() || !root.is_object())
        {
            return { std::nullopt, "not valid JSON" };
        }

        Dialogue dialogue;
        dialogue.id = OptionalString(root, "id");
        dialogue.start = OptionalString(root, "start");
        if (dialogue.id.empty() || dialogue.start.empty())
        {
            return { std::nullopt, "missing \"id\" or \"start\"" };
        }

        const auto nodes = root.find("nodes");
        if (nodes == root.end() || !nodes->is_array())
        {
            return { std::nullopt, "missing \"nodes\" array" };
        }

        for (const Json& item : *nodes)
        {
            DialogueNode node;
            node.id = OptionalString(item, "id");
            node.speaker = OptionalString(item, "speaker");
            node.text = OptionalString(item, "text");
            node.next = OptionalString(item, "next");
            node.setsFlag = OptionalString(item, "sets");
            if (node.id.empty())
            {
                return { std::nullopt, "a node has no \"id\"" };
            }

            if (const auto choices = item.find("choices");
                choices != item.end() && choices->is_array())
            {
                for (const Json& entry : *choices)
                {
                    node.choices.push_back(DialogueChoice{
                        OptionalString(entry, "text"),
                        OptionalString(entry, "next"),
                        OptionalString(entry, "requires"),
                        OptionalString(entry, "requiresNot"),
                        OptionalString(entry, "sets"),
                    });
                }
            }

            const std::string id = node.id;
            if (!dialogue.nodes.emplace(id, std::move(node)).second)
            {
                return { std::nullopt, "duplicate node id \"" + id + "\"" };
            }
        }

        // Every link must land somewhere: catch typos at load time rather
        // than mid-conversation.
        if (!dialogue.Find(dialogue.start))
        {
            return { std::nullopt, "start node \"" + dialogue.start + "\" does not exist" };
        }
        for (const auto& [id, node] : dialogue.nodes)
        {
            if (!IsEnd(node.next) && !dialogue.Find(node.next))
            {
                return { std::nullopt, "node \"" + id + "\" continues to unknown \"" + node.next + "\"" };
            }
            for (const DialogueChoice& choice : node.choices)
            {
                if (!IsEnd(choice.next) && !dialogue.Find(choice.next))
                {
                    return { std::nullopt, "a choice in \"" + id + "\" leads to unknown \"" + choice.next + "\"" };
                }
            }
        }

        return { std::move(dialogue), {} };
    }

    DialogueParseResult LoadDialogueFile(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            return { std::nullopt, "cannot open file" };
        }
        std::stringstream contents;
        contents << file.rdbuf();
        return ParseDialogue(contents.str());
    }

    std::size_t DialogueLibrary::LoadDirectory(const std::string& directory)
    {
        std::size_t loaded = 0;
        std::error_code error;
        for (const auto& entry : std::filesystem::directory_iterator(directory, error))
        {
            if (entry.path().extension() != ".json")
            {
                continue;
            }

            DialogueParseResult result = LoadDialogueFile(entry.path().string());
            if (!result.dialogue)
            {
                std::cerr
                    << "Dialogue '" << entry.path().filename().string()
                    << "' skipped: " << result.error << '\n';
                continue;
            }
            std::cout
                << "Loaded dialogue '" << result.dialogue->id << "' ("
                << result.dialogue->nodes.size() << " nodes)\n";
            Add(std::move(*result.dialogue));
            ++loaded;
        }

        if (error)
        {
            std::cerr << "Cannot read dialogue folder '" << directory << "': " << error.message() << '\n';
        }
        return loaded;
    }

    void DialogueLibrary::Add(Dialogue dialogue)
    {
        const std::string id = dialogue.id;
        m_dialogues.insert_or_assign(id, std::move(dialogue));
    }

    const Dialogue* DialogueLibrary::Find(std::string_view id) const
    {
        const auto found = m_dialogues.find(std::string(id));
        return found != m_dialogues.end() ? &found->second : nullptr;
    }
}
