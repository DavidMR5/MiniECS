#pragma once

#include "Entity.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ecs
{
    class EntityManager
    {
    public:
        [[nodiscard]] Entity create();

        bool destroy(Entity entity);

        [[nodiscard]] bool isAlive(Entity entity) const;

        [[nodiscard]] Entity handleOf(EntityID id) const;

        [[nodiscard]] std::size_t size() const;

        void reserve(std::size_t count);

        void clear();

    private:
        std::vector<std::uint32_t> m_generations;
        std::vector<EntityID> m_freeIds;
    };
}
