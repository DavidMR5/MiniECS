#include "ecs/EntityManager.h"

#include <cassert>

namespace ecs
{
    Entity EntityManager::create()
    {
        if (!m_freeIds.empty())
        {
            const EntityID id = m_freeIds.back();
            m_freeIds.pop_back();

            return Entity{ id, m_generations[id - 1] };
        }

        m_generations.push_back(0);

        const auto id = static_cast<EntityID>(m_generations.size());

        return Entity{ id, 0 };
    }

    bool EntityManager::destroy(Entity entity)
    {
        if (!isAlive(entity))
        {
            return false;
        }

        // Invalidates every old handle to this ID.
        ++m_generations[entity.id - 1];

        m_freeIds.push_back(entity.id);

        return true;
    }

    bool EntityManager::isAlive(Entity entity) const
    {
        if (!entity.isValid())
        {
            return false;
        }

        const std::size_t index = entity.id - 1;

        if (index >= m_generations.size())
        {
            return false;
        }

        return m_generations[index] == entity.generation;
    }

    Entity EntityManager::handleOf(EntityID id) const
    {
        assert(id != INVALID_ENTITY && id <= m_generations.size());

        return Entity{ id, m_generations[id - 1] };
    }

    std::size_t EntityManager::size() const
    {
        return m_generations.size() - m_freeIds.size();
    }

    void EntityManager::reserve(std::size_t count)
    {
        m_generations.reserve(count);
    }

    void EntityManager::clear()
    {
        m_freeIds.clear();

        for (std::size_t i = m_generations.size(); i > 0; --i)
        {
            ++m_generations[i - 1];
            m_freeIds.push_back(static_cast<EntityID>(i));
        }
    }
}
