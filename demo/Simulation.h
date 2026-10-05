#pragma once

#include "Components.h"
#include "SpatialGrid.h"

#include <ecs/Registry.h>
#include <ecs/System.h>
#include <ecs/memory/LinearAllocator.h>

#include <cmath>
#include <cstdint>

namespace demo
{
    struct SimulationSettings
    {
        float worldWidth = 1600.0f;
        float worldHeight = 900.0f;

        float perceptionRadius = 40.0f;
        float separationRadius = 15.0f;
        int maxNeighbors = 24;

        float separationWeight = 1.7f;
        float alignmentWeight = 1.1f;
        float cohesionWeight = 0.8f;

        float minSpeed = 45.0f;

        float mouseRadius = 180.0f;
        float mouseStrength = 900.0f;
    };

    struct SimulationContext
    {
        SimulationSettings settings;

        ecs::memory::LinearAllocator frameArena{ 32u * 1024u * 1024u };
        std::size_t frameArenaUsedLastFrame = 0;

        SpatialGrid grid;
        bool gridValid = false;

        Vec2 mouse;
        int mouseMode = 0;

        std::uint64_t neighborChecks = 0;
    };

    // Shortest vector between two points in a wrap-around world.
    inline Vec2 toroidalDelta(Vec2 from, Vec2 to, float width, float height)
    {
        Vec2 d = to - from;

        if (d.x > width * 0.5f)  d.x -= width;
        if (d.x < -width * 0.5f) d.x += width;
        if (d.y > height * 0.5f)  d.y -= height;
        if (d.y < -height * 0.5f) d.y += height;

        return d;
    }

    class SpatialGridSystem final : public ecs::System
    {
    public:
        explicit SpatialGridSystem(SimulationContext& context) : m_context(context) {}

        void update(ecs::Registry& registry, float) override
        {
            m_context.frameArenaUsedLastFrame = m_context.frameArena.used();
            m_context.frameArena.reset(); // frees last frame's data

            const SimulationSettings& s = m_context.settings;

            m_context.gridValid = m_context.grid.build(
                registry, m_context.frameArena, s.perceptionRadius, s.worldWidth, s.worldHeight);
        }

        [[nodiscard]] const char* name() const override { return "SpatialGrid build"; }

    private:
        SimulationContext& m_context;
    };

    // Reynolds boids: separation, alignment, cohesion.
    class FlockingSystem final : public ecs::System
    {
    public:
        explicit FlockingSystem(SimulationContext& context) : m_context(context) {}

        void update(ecs::Registry& registry, float) override
        {
            const SimulationSettings& s = m_context.settings;
            const SpatialGrid& grid = m_context.grid;

            m_context.neighborChecks = 0;

            if (!m_context.gridValid)
            {
                return;
            }

            const float perception2 = s.perceptionRadius * s.perceptionRadius;
            const float separation2 = s.separationRadius * s.separationRadius;

            const Vec2* positions = grid.positions();
            const Vec2* velocities = grid.velocities();
            const ecs::EntityID* ids = grid.ids();

            std::uint64_t checks = 0;

            registry.view<Position, Velocity, Acceleration, Boid>().each(
                [&](ecs::Entity entity, const Position& position, const Velocity& velocity,
                    Acceleration& acceleration, const Boid& boid)
                {
                    const Vec2 p = position.value;
                    const Vec2 v = velocity.value;

                    Vec2 separation;
                    Vec2 alignmentSum;
                    Vec2 cohesionSum;
                    int neighbors = 0;

                    grid.forEachNearby(p, [&](std::uint32_t i)
                    {
                        ++checks;

                        if (ids[i] == entity.id || neighbors >= s.maxNeighbors)
                        {
                            return;
                        }

                        const Vec2 d = toroidalDelta(p, positions[i], s.worldWidth, s.worldHeight);
                        const float d2 = d.lengthSquared();

                        if (d2 > perception2)
                        {
                            return;
                        }

                        ++neighbors;
                        alignmentSum += velocities[i];
                        cohesionSum += d;

                        if (d2 < separation2 && d2 > 1e-4f)
                        {
                            separation -= d * (1.0f / d2);
                        }
                    });

                    Vec2 steer;

                    if (neighbors > 0)
                    {
                        const float inv = 1.0f / static_cast<float>(neighbors);

                        const Vec2 desiredAlignment = (alignmentSum * inv).withLength(boid.maxSpeed);
                        steer += (desiredAlignment - v).limited(boid.maxForce) * s.alignmentWeight;

                        const Vec2 desiredCohesion = (cohesionSum * inv).withLength(boid.maxSpeed);
                        steer += (desiredCohesion - v).limited(boid.maxForce) * s.cohesionWeight;
                    }

                    if (separation.lengthSquared() > 0.0f)
                    {
                        const Vec2 desiredSeparation = separation.withLength(boid.maxSpeed);
                        steer += (desiredSeparation - v).limited(boid.maxForce) * s.separationWeight;
                    }

                    if (m_context.mouseMode != 0)
                    {
                        const Vec2 toMouse = m_context.mouse - p;
                        const float distance = toMouse.length();

                        if (distance < s.mouseRadius && distance > 1.0f)
                        {
                            const float falloff = 1.0f - distance / s.mouseRadius;
                            steer += toMouse.withLength(
                                s.mouseStrength * falloff * static_cast<float>(m_context.mouseMode));
                        }
                    }

                    acceleration.value = steer;
                });

            m_context.neighborChecks = checks;
        }

        [[nodiscard]] const char* name() const override { return "Flocking"; }

    private:
        SimulationContext& m_context;
    };

    class MovementSystem final : public ecs::System
    {
    public:
        explicit MovementSystem(SimulationContext& context) : m_context(context) {}

        void update(ecs::Registry& registry, float deltaTime) override
        {
            const SimulationSettings& s = m_context.settings;

            registry.view<Position, Velocity, Acceleration, Boid>().each(
                [&](Position& position, Velocity& velocity, const Acceleration& acceleration, const Boid& boid)
                {
                    Vec2 v = velocity.value + acceleration.value * deltaTime;
                    v = v.limited(boid.maxSpeed);

                    if (v.lengthSquared() < s.minSpeed * s.minSpeed)
                    {
                        v = v.lengthSquared() > 1e-6f ? v.withLength(s.minSpeed) : Vec2{ s.minSpeed, 0.0f };
                    }

                    velocity.value = v;

                    Vec2 p = position.value + v * deltaTime;
                    p.x = wrap(p.x, s.worldWidth);
                    p.y = wrap(p.y, s.worldHeight);
                    position.value = p;
                });
        }

        [[nodiscard]] const char* name() const override { return "Movement"; }

    private:
        static float wrap(float value, float size)
        {
            value = std::fmod(value, size);
            return value < 0.0f ? value + size : value;
        }

        SimulationContext& m_context;
    };
}
