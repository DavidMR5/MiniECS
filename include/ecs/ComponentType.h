#pragma once

#include <cstdint>
#include <type_traits>

namespace ecs
{
    using ComponentTypeID = std::uint32_t;

    namespace detail
    {
        inline ComponentTypeID nextComponentTypeId()
        {
            static ComponentTypeID counter = 0;
            return counter++;
        }
    }

    // Dense ID per component type, assigned on first use.
    template<typename T>
    ComponentTypeID componentTypeId()
    {
        using Bare = std::remove_cvref_t<T>;

        if constexpr (std::is_same_v<T, Bare>)
        {
            static const ComponentTypeID id = detail::nextComponentTypeId();
            return id;
        }
        else
        {
            return componentTypeId<Bare>();
        }
    }
}
