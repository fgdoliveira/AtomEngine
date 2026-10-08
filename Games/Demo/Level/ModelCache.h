#pragma once

#include "Assets/Model.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

namespace Atom
{
    class Renderer;
}

namespace Demo
{
    // Shares loaded models (M22): a model used by two levels, or by several
    // entities, is loaded once. Entries are weak, so a model dies with the
    // last level that uses it; during a level change the next level picks up
    // the models the old one still holds. A file changed on disk since it
    // was loaded is loaded again (hot reload).
    //
    // Materials are shared with the model: runtime material edits (a
    // flickering sign, a lightmap) reach every user of that file.
    class ModelCache
    {
    public:
        std::shared_ptr<Atom::Model> Get(Atom::Renderer& renderer, const std::string& path)
        {
            std::error_code error;
            const auto written = std::filesystem::last_write_time(path, error);
            Entry& entry = m_entries[path];
            if (auto alive = entry.model.lock(); alive && !error && written == entry.written)
            {
                ++m_hits;
                return alive;
            }
            std::shared_ptr<Atom::Model> model = Atom::Model::Load(renderer, path);
            if (model)
            {
                const Atom::Model::LoadTimes& t = model->GetLoadTimes();
                m_loadTimes.parseMs += t.parseMs;
                m_loadTimes.decodeMs += t.decodeMs;
                m_loadTimes.uploadMs += t.uploadMs;
                m_loadTimes.totalMs += t.totalMs;
            }
            entry.model = model;
            entry.written = error ? std::filesystem::file_time_type{} : written;
            ++m_loads;
            return model;
        }

        std::size_t GetHits() const { return m_hits; }
        std::size_t GetLoads() const { return m_loads; }
        // M57: the summed stage times of every model loaded (not reused).
        const Atom::Model::LoadTimes& GetLoadTimes() const { return m_loadTimes; }

    private:
        struct Entry
        {
            std::weak_ptr<Atom::Model> model;
            std::filesystem::file_time_type written{};
        };

        std::unordered_map<std::string, Entry> m_entries;
        std::size_t m_hits = 0;
        std::size_t m_loads = 0;
        Atom::Model::LoadTimes m_loadTimes;
    };
}
