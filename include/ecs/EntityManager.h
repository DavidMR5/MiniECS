#pragma once

#include "Entity.h"

#include <cstdint>
#include <vector>

namespace ecs
{
    class EntityManager
    {
    public:
        Entity create();

        void destroy(Entity entity);

        bool isAlive(Entity entity) const;

        std::size_t size() const;

    private:
        std::vector<std::uint32_t> m_generations;
        std::vector<EntityID> m_freeIds;
    };
}