#pragma once

#include "Registry.h"

namespace ecs
{
    class System
    {
    public:
        virtual ~System() = default;

        virtual void update(
            Registry& registry,
            float deltaTime
        ) = 0;
    };
}