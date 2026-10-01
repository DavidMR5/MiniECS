#include "ecs/EntityManager.h"

namespace ecs
{
    Entity EntityManager::create()
    {
        if (!m_freeIds.empty())
        {
            EntityID id = m_freeIds.back();
            m_freeIds.pop_back();

            return Entity{
                id,
                m_generations[id - 1]
            };
        }

        EntityID id =
            static_cast<EntityID>(m_generations.size() + 1);

        m_generations.push_back(0);

        return Entity{
            id,
            0
        };
    }

    void EntityManager::destroy(Entity entity)
    {
        if (!isAlive(entity))
        {
            return;
        }

        const std::size_t index = entity.id - 1;

        ++m_generations[index];

        m_freeIds.push_back(entity.id);
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

    std::size_t EntityManager::size() const
    {
        return m_generations.size() - m_freeIds.size();
    }
}