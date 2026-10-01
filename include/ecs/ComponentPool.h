#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <stdexcept>

namespace ecs
{
    template<typename T>
    class ComponentPool
    {
    public:

        bool has(std::uint32_t entityId) const
        {
            return m_entityToIndex.contains(entityId);
        }

        void add(std::uint32_t entityId, const T& component)
        {
            if (has(entityId))
            {
                return;
            }

            const std::size_t index = m_components.size();

            m_components.push_back(component);
            m_entities.push_back(entityId);

            m_entityToIndex[entityId] = index;
        }

        void remove(std::uint32_t entityId)
        {
            auto it = m_entityToIndex.find(entityId);

            if (it == m_entityToIndex.end())
            {
                return;
            }

            const std::size_t index = it->second;
            const std::size_t lastIndex = m_components.size() - 1;

            if (index != lastIndex)
            {
                m_components[index] = m_components[lastIndex];

                const std::uint32_t movedEntity =
                    m_entities[lastIndex];

                m_entities[index] = movedEntity;

                m_entityToIndex[movedEntity] = index;
            }

            m_components.pop_back();
            m_entities.pop_back();

            m_entityToIndex.erase(it);
        }

        T& get(std::uint32_t entityId)
        {
            auto it = m_entityToIndex.find(entityId);

            if (it == m_entityToIndex.end())
            {
                throw std::runtime_error(
                    "Component does not exist"
                );
            }

            return m_components[it->second];
        }

        const T& get(std::uint32_t entityId) const
        {
            auto it = m_entityToIndex.find(entityId);

            if (it == m_entityToIndex.end())
            {
                throw std::runtime_error(
                    "Component does not exist"
                );
            }

            return m_components[it->second];
        }

        std::vector<T>& data()
        {
            return m_components;
        }

        const std::vector<T>& data() const
        {
            return m_components;
        }

        std::size_t size() const
        {
            return m_components.size();
        }

    private:
        std::vector<T> m_components;

        std::vector<std::uint32_t> m_entities;

        std::unordered_map<
            std::uint32_t,
            std::size_t
        > m_entityToIndex;
    };
}