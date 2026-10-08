#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <string_view>

namespace AtomGame
{
    // Parses JSON text; on a syntax error, returns a message that says where
    // ("line 12, column 7: ...") so an author can jump straight to it.
    // Empty string on success.
    inline std::string ParseJsonText(std::string_view text, nlohmann::json& out)
    {
        try
        {
            out = nlohmann::json::parse(text);
            return {};
        }
        catch (const nlohmann::json::parse_error& error)
        {
            // Count lines up to the failing byte (1-based, as editors show).
            const std::size_t end = std::min<std::size_t>(error.byte > 0 ? error.byte - 1 : 0, text.size());
            std::size_t line = 1;
            std::size_t column = 1;
            for (std::size_t i = 0; i < end; ++i)
            {
                if (text[i] == '\n')
                {
                    ++line;
                    column = 1;
                }
                else
                {
                    ++column;
                }
            }
            // nlohmann's message ends with the reason after the last ": ".
            std::string reason = error.what();
            if (const std::size_t colon = reason.rfind(": "); colon != std::string::npos)
            {
                reason = reason.substr(colon + 2);
            }
            return "line " + std::to_string(line) + ", column " + std::to_string(column) + ": " + reason;
        }
    }

    // One step of a JSON Pointer (RFC 6901), escaping '~' and '/'.
    inline std::string JsonPath(const std::string& parent, const std::string& key)
    {
        std::string escaped;
        for (const char c : key)
        {
            escaped += c == '~' ? std::string("~0") : c == '/' ? std::string("~1") : std::string(1, c);
        }
        return parent + "/" + escaped;
    }

    inline std::string JsonPath(const std::string& parent, std::size_t index)
    {
        return parent + "/" + std::to_string(index);
    }
}
