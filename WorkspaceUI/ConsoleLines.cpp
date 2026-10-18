#include "ConsoleLines.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace AtomWorkspace
{
    namespace
    {
        bool Contains(std::string_view text, std::string_view word)
        {
            const auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
            return std::search(text.begin(), text.end(), word.begin(), word.end(),
                               [&](char a, char b) { return lower(static_cast<unsigned char>(a)) == b; })
                != text.end();
        }

        std::string_view Trim(std::string_view text)
        {
            while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
            {
                text.remove_prefix(1);
            }
            while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
            {
                text.remove_suffix(1);
            }
            return text;
        }
    }

    LineKind ClassifyLine(std::string_view line)
    {
        const std::string_view text = Trim(line);
        if (text.starts_with("> ") || text.starts_with("< "))
        {
            return LineKind::Note;
        }
        // "0 error(s)" and "0 Error(s)" are summaries of success, not errors.
        if (Contains(" " + std::string(text), " 0 error") || text.starts_with("PASS") || text.starts_with("ok "))
        {
            return LineKind::Success;
        }
        if (text.starts_with("ERROR") || text.starts_with("FAIL") || Contains(text, ": error") || Contains(text, "fatal error")
            || Contains(text, "cmake error") || Contains(text, "[error"))
        {
            return LineKind::Error;
        }
        if (text.starts_with("WARNING") || Contains(text, ": warning") || Contains(text, "cmake warning") || Contains(text, "[warn"))
        {
            return LineKind::Warning;
        }
        return LineKind::Plain;
    }

    std::string FileInLine(std::string_view line, const std::vector<std::string>& searchRoots)
    {
        const std::string_view text = Trim(line);
        std::string_view candidate;
        if (text.starts_with("Source: "))
        {
            // AtomLevelCheck: "Source: <file>", maybe followed by " (...)" or
            // a JSON Pointer after '#'.
            candidate = text.substr(8);
            candidate = candidate.substr(0, std::min(candidate.find(" ("), candidate.find('#')));
        }
        else if (text.starts_with("CMake Error at ") || text.starts_with("CMake Warning at "))
        {
            // "CMake Error at <file>:<line> (<command>):" - the command off,
            // then the line number.
            candidate = text.substr(text.find(" at ") + 4);
            candidate = candidate.substr(0, candidate.find(" ("));
            candidate = candidate.substr(0, candidate.rfind(':'));
        }
        else if (const std::size_t paren = text.find('('); paren != std::string_view::npos && paren > 0
                 && text.find("): ", paren) != std::string_view::npos)
        {
            // MSVC: "<file>(<line>[,<col>]): error C...: ..."
            const std::size_t close = text.find(')', paren);
            const std::string_view number = text.substr(paren + 1, close - paren - 1);
            if (!number.empty() && std::all_of(number.begin(), number.end(),
                                               [](char c) { return std::isdigit(static_cast<unsigned char>(c)) || c == ','; }))
            {
                candidate = text.substr(0, paren);
            }
        }
        candidate = Trim(candidate);
        if (candidate.empty())
        {
            return {};
        }
        std::error_code error;
        const std::filesystem::path path{ std::string(candidate) };
        if (path.is_absolute())
        {
            return std::filesystem::is_regular_file(path, error) ? path.lexically_normal().string() : std::string{};
        }
        for (const std::string& root : searchRoots)
        {
            const std::filesystem::path full = std::filesystem::path(root) / path;
            if (std::filesystem::is_regular_file(full, error))
            {
                return full.lexically_normal().string();
            }
        }
        return {};
    }
}
