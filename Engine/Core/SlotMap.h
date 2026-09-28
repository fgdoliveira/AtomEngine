#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace Atom
{
    // Stable handle into a SlotMap. A handle stays safe to hold after its
    // object is removed: lookups simply fail, because the slot's generation
    // moved on. Generation 0 is never issued, so a default handle is null.
    struct Handle
    {
        std::uint32_t index = 0;
        std::uint32_t generation = 0;

        bool IsNull() const { return generation == 0; }
        friend bool operator==(const Handle&, const Handle&) = default;
    };

    // Owns objects in slots that are reused after removal. Insert and remove
    // are O(1); handles never dangle (unlike pointers or indices).
    template <typename T>
    class SlotMap
    {
    public:
        Handle Insert(T value)
        {
            std::uint32_t index;
            if (!m_free.empty())
            {
                index = m_free.back();
                m_free.pop_back();
            }
            else
            {
                index = static_cast<std::uint32_t>(m_slots.size());
                m_slots.emplace_back();
            }

            Slot& slot = m_slots[index];
            slot.value.emplace(std::move(value));
            ++m_size;
            return Handle{ index, slot.generation };
        }

        bool Remove(Handle handle)
        {
            if (!Contains(handle))
            {
                return false;
            }
            Slot& slot = m_slots[handle.index];
            slot.value.reset();
            // Invalidate every outstanding handle to this slot. Skip 0 on
            // wrap-around so no handle ever looks null by accident.
            if (++slot.generation == 0)
            {
                slot.generation = 1;
            }
            m_free.push_back(handle.index);
            --m_size;
            return true;
        }

        bool Contains(Handle handle) const
        {
            return !handle.IsNull()
                && handle.index < m_slots.size()
                && m_slots[handle.index].generation == handle.generation
                && m_slots[handle.index].value.has_value();
        }

        T* Get(Handle handle)
        {
            return Contains(handle) ? &*m_slots[handle.index].value : nullptr;
        }

        const T* Get(Handle handle) const
        {
            return Contains(handle) ? &*m_slots[handle.index].value : nullptr;
        }

        std::size_t Size() const { return m_size; }

        void Clear()
        {
            for (std::uint32_t i = 0; i < m_slots.size(); ++i)
            {
                if (m_slots[i].value)
                {
                    Remove(Handle{ i, m_slots[i].generation });
                }
            }
        }

        // Calls f(Handle, T&) for every live object.
        template <typename F>
        void ForEach(F&& f)
        {
            for (std::uint32_t i = 0; i < m_slots.size(); ++i)
            {
                if (m_slots[i].value)
                {
                    f(Handle{ i, m_slots[i].generation }, *m_slots[i].value);
                }
            }
        }

        template <typename F>
        void ForEach(F&& f) const
        {
            for (std::uint32_t i = 0; i < m_slots.size(); ++i)
            {
                if (m_slots[i].value)
                {
                    f(Handle{ i, m_slots[i].generation }, *m_slots[i].value);
                }
            }
        }

    private:
        struct Slot
        {
            std::optional<T> value;
            std::uint32_t generation = 1;
        };

        std::vector<Slot> m_slots;
        std::vector<std::uint32_t> m_free;
        std::size_t m_size = 0;
    };
}
