#pragma once

#include <cstdint>

namespace ecs
{
    using EntityID = std::uint32_t;

    constexpr EntityID INVALID_ENTITY = 0;

    struct Entity
    {
        EntityID id = INVALID_ENTITY;
        std::uint32_t generation = 0;

        bool isValid() const
        {
            return id != INVALID_ENTITY;
        }

        bool operator==(const Entity& other) const
        {
            return id == other.id &&
                   generation == other.generation;
        }

        bool operator!=(const Entity& other) const
        {
            return !(*this == other);
        }
    };
}