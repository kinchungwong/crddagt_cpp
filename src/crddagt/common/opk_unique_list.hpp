/**
 * @file opk_unique_list.hpp
 */
#pragma once
#include "opaque_ptr_key.hpp"

namespace crddagt
{

/**
 * @brief A list of unique OpaquePtrKey<T> elements.
 * 
 * @note Insertion order is preserved.
 * @note Duplicate insertions return the existing index.
 * @note No thread safety guarantees; users must synchronize externally.
 */
template <typename T>
class OpkUniqueList
{
public:
    static constexpr std::size_t npos = ~static_cast<std::size_t>(0);

public:
    /**
     * @brief Insert an OpaquePtrKey<T> into the list if not already present.
     * @return The index of the inserted or existing element.
     * @throw std::invalid_argument if the OpaquePtrKey<T> is null, or originally
     *        initialized from an expired std::weak_ptr.
     */
    std::size_t insert(const OpaquePtrKey<T>& opk)
    {
        if (!opk)
        {
            throw std::invalid_argument("OpkUniqueList::insert: null OpaquePtrKey");
        }
        auto it = m_map.find(opk);
        if (it != m_map.end())
        {
            return it->second;
        }
        std::size_t index = m_list.size();
        m_list.push_back(opk);
        m_map.emplace(opk, index);
        return index;
    }

    /**
     * @brief Find the index of the given `OpaquePtrKey<T>`.
     * @return The index if found; otherwise, `OpkUniqueList<T>::npos`.
     */
    std::size_t find(const OpaquePtrKey<T>& opk) const noexcept
    {
        auto it = m_map.find(opk);
        if (it != m_map.end())
        {
            return it->second;
        }
        return npos;
    }

    /**
     * @brief Access the `OpaquePtrKey<T>` at the given index.
     * @throw std::out_of_range if index is invalid.
     * @return The `OpaquePtrKey<T>` at the specified index.
     */
    OpaquePtrKey<T> at(std::size_t index) const
    {
        if (index >= m_list.size())
        {
            throw std::out_of_range("OpkUniqueList::at: index out of range");
        }
        return m_list[index];
    }

    std::size_t size() const noexcept
    {
        return m_list.size();
    }

    /**
     * @brief Enumerate all elements in insertion order.
     * @note No protection for reentrancy. Do not modify the list from inside the callback.
     */
    template <typename Func>
    void enumerate(Func&& func) const
    {
        static_assert(std::is_invocable_v<Func&, std::size_t, const OpaquePtrKey<T>&>,
            "Func must be callable as f(size_t, const OpaquePtrKey<T>&)");
        const size_t count = m_list.size();
        for (size_t idx = 0u; idx < count; ++idx)
        {
            func(idx, m_list[idx]);
        }
    }

private:
    std::vector<OpaquePtrKey<T>> m_list;
    std::unordered_map<OpaquePtrKey<T>, std::size_t> m_map;
};

} // namespace crddagt
