#include "Project/ProjectDescriptor.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace AtomProject
{
    namespace
    {
        using Json = nlohmann::json;

        std::string Upper(std::string_view text)
        {
            std::string out(text);
            std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            return out;
        }
    }

    std::string CheckProjectName(std::string_view name)
    {
        if (name.empty())
        {
            return "a project needs a name";
        }
        if (name.size() > 64)
        {
            return "at most 64 characters";
        }
        if (!std::isalpha(static_cast<unsigned char>(name.front())))
        {
            return "must start with a letter (A-Z, a-z)";
        }
        for (const char c : name)
        {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
            {
                return std::string("only letters, digits and '_' (not '") + c + "')";
            }
        }
        // Windows refuses these as file or folder names, in any case, with
        // or without an extension.
        static const std::array<std::string_view, 22> Reserved = {
            "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7",
            "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9" };
        const std::string upper = Upper(name);
        if (std::find(Reserved.begin(), Reserved.end(), upper) != Reserved.end())
        {
            return "\"" + std::string(name) + "\" is a reserved device name on Windows";
        }
        return {};
    }

    std::string CheckRelativePath(std::string_view text)
    {
        if (text.empty())
        {
            return "empty path";
        }
        const std::filesystem::path path{ std::string(text) };
        if (path.has_root_name() || path.has_root_directory())
        {
            return "\"" + std::string(text) + "\" must be relative to the project";
        }
        for (const auto& part : path)
        {
            if (part == "..")
            {
                return "\"" + std::string(text) + "\" leaves the project (\"..\")";
            }
        }
        return {};
    }

    DescriptorLoad ParseDescriptor(std::string_view text)
    {
        const Json root = Json::parse(text, nullptr, false);
        if (root.is_discarded() || !root.is_object())
        {
            return { std::nullopt, "not a JSON object" };
        }
        const int schema = root.value("schema", 0);
        if (schema != ProjectDescriptor::SchemaVersion)
        {
            return { std::nullopt, "schema " + std::to_string(schema) + ", expected "
                + std::to_string(ProjectDescriptor::SchemaVersion) };
        }
        ProjectDescriptor d;
        d.name = root.value("name", std::string{});
        d.engine = root.value("engine", std::string{});
        d.source = root.value("source", d.source);
        d.assets = root.value("assets", d.assets);
        d.executable = root.value("executable", d.name);
        d.startupLevel = root.value("startupLevel", std::string{});
        if (const std::string problem = CheckProjectName(d.name); !problem.empty())
        {
            return { std::nullopt, "/name: " + problem };
        }
        if (const std::string problem = CheckProjectName(d.executable); !problem.empty())
        {
            return { std::nullopt, "/executable: " + problem };
        }
        for (const auto& [key, value] : { std::pair{ "source", d.source }, std::pair{ "assets", d.assets } })
        {
            if (const std::string problem = CheckRelativePath(value); !problem.empty())
            {
                return { std::nullopt, std::string("/") + key + ": " + problem };
            }
        }
        return { d, {} };
    }

    std::string WriteDescriptor(const ProjectDescriptor& d)
    {
        // An ordered object: the keys in the order a person reads them.
        nlohmann::ordered_json root;
        root["schema"] = ProjectDescriptor::SchemaVersion;
        root["name"] = d.name;
        root["engine"] = d.engine;
        root["source"] = d.source;
        root["assets"] = d.assets;
        root["executable"] = d.executable.empty() ? d.name : d.executable;
        root["startupLevel"] = d.startupLevel;
        return root.dump(2) + "\n";
    }

    DescriptorLoad LoadDescriptor(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            return { std::nullopt, "cannot open " + path };
        }
        std::stringstream text;
        text << file.rdbuf();
        DescriptorLoad load = ParseDescriptor(text.str());
        if (!load.error.empty())
        {
            load.error = std::filesystem::path(path).filename().string() + ": " + load.error;
        }
        return load;
    }
}
