#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace AtomFramework
{
    // Notices when files change on disk, by comparing modification times
    // (polling: simple, portable, and cheap for a handful of files checked
    // once a second). A file appearing or disappearing counts as a change.
    class FileWatcher
    {
    public:
        // Starts watching exactly these files, as they are now.
        void Watch(std::vector<std::string> paths)
        {
            m_files.clear();
            for (std::string& path : paths)
            {
                const auto time = TimeOf(path);
                m_files.push_back({ std::move(path), time });
            }
        }

        // The files that changed since the last call (or Watch), each
        // reported once.
        std::vector<std::string> Poll()
        {
            std::vector<std::string> changed;
            for (File& file : m_files)
            {
                const auto time = TimeOf(file.path);
                if (time != file.time)
                {
                    file.time = time;
                    changed.push_back(file.path);
                }
            }
            return changed;
        }

        std::size_t Count() const { return m_files.size(); }

    private:
        using Time = std::optional<std::filesystem::file_time_type>;

        struct File
        {
            std::string path;
            Time time;
        };

        static Time TimeOf(const std::string& path)
        {
            std::error_code error;
            const auto time = std::filesystem::last_write_time(path, error);
            return error ? Time{} : Time{ time };
        }

        std::vector<File> m_files;
    };
}
