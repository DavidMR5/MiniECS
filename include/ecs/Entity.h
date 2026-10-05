#pragma once

#include <cstdint>

namespace ecs
{
    using EntityID = std::uint32_t;

    // ID 0 is reserved as "no entity".
    constexpr EntityID INVALID_ENTITY = 0;

    struct Entity
    {
        EntityID id = INVALID_ENTITY;
        std::uint32_t generation = 0; // bumped when the ID is reused

        [[nodiscard]] constexpr bool isValid() const
        {
            return id != INVALID_ENTITY;
        }

        constexpr bool operator==(const Entity& other) const = default;
    };
}
