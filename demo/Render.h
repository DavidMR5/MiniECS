#pragma once

#include "Components.h"
#include "Simulation.h"

#include <ecs/Registry.h>
#include <ecs/System.h>

#include <raylib.h>

#include <cmath>
#include <cstdint>

namespace demo
{
    struct RenderOptions
    {
        bool showGrid = false;
        bool inspect = false;
    };

    class RenderSystem final : public ecs::System
    {
    public:
        RenderSystem(SimulationContext& context, RenderOptions& options)
            : m_context(context)
            , m_options(options)
        {
        }

        void update(ecs::Registry& registry, float) override
        {
            if (m_options.showGrid && m_context.gridValid)
            {
                drawGrid();
            }

            registry.view<Position, Velocity, Renderable>().each(
                [](const Position& position, const Velocity& velocity, const Renderable& renderable)
                {
                    const Vec2 heading = velocity.value.withLength(1.0f);
                    const Vec2 side{ -heading.y, heading.x };
                    const float size = renderable.size;

                    const Vec2 p = position.value;
                    const Vec2 tip = p + heading * (size * 1.8f);
                    const Vec2 left = p - heading * size + side * (size * 0.8f);
                    const Vec2 right = p - heading * size - side * (size * 0.8f);

                    float hue = std::atan2(heading.y, heading.x) * (180.0f / PI) + 180.0f + renderable.hueShift;
                    hue = std::fmod(hue, 360.0f);

                    const Color color = ColorFromHSV(hue, 0.55f, 1.0f);

                    // raylib expects counter-clockwise order.
                    DrawTriangle(toRay(tip), toRay(right), toRay(left), color);
                });

            if (m_options.inspect && m_context.gridValid)
            {
                drawInspector();
            }

            if (m_context.mouseMode != 0)
            {
                const Color color = m_context.mouseMode > 0 ? Color{ 120, 220, 255, 90 } : Color{ 255, 120, 120, 90 };
                DrawCircleLinesV(toRay(m_context.mouse), m_context.settings.mouseRadius, color);
            }
        }

        [[nodiscard]] const char* name() const override { return "Render"; }

    private:
        static Vector2 toRay(Vec2 v)
        {
            return Vector2{ v.x, v.y };
        }

        void drawGrid() const
        {
            const SpatialGrid& grid = m_context.grid;
            const float w = grid.cellWidth();
            const float h = grid.cellHeight();

            for (int y = 0; y < grid.rows(); ++y)
            {
                for (int x = 0; x < grid.columns(); ++x)
                {
                    const std::uint32_t count = grid.boidsInCell(x, y);

                    if (count == 0)
                    {
                        continue;
                    }

                    const float heat = std::fmin(1.0f, static_cast<float>(count) / 16.0f);
                    const auto alpha = static_cast<unsigned char>(20.0f + 90.0f * heat);

                    DrawRectangleV(Vector2{ static_cast<float>(x) * w, static_cast<float>(y) * h },
                                   Vector2{ w, h }, Color{ 255, 170, 60, alpha });
                }
            }

            const Color line{ 255, 255, 255, 25 };

            for (int x = 0; x <= grid.columns(); ++x)
            {
                const float px = static_cast<float>(x) * w;
                DrawLineV(Vector2{ px, 0.0f }, Vector2{ px, m_context.settings.worldHeight }, line);
            }

            for (int y = 0; y <= grid.rows(); ++y)
            {
                const float py = static_cast<float>(y) * h;
                DrawLineV(Vector2{ 0.0f, py }, Vector2{ m_context.settings.worldWidth, py }, line);
            }
        }

        void drawInspector() const
        {
            const SpatialGrid& grid = m_context.grid;
            const SimulationSettings& s = m_context.settings;
            const Vec2* positions = grid.positions();

            std::uint32_t closest = UINT32_MAX;
            float best = 1e30f;

            grid.forEachNearby(m_context.mouse, [&](std::uint32_t i)
            {
                const float d2 = (positions[i] - m_context.mouse).lengthSquared();

                if (d2 < best)
                {
                    best = d2;
                    closest = i;
                }
            });

            if (closest == UINT32_MAX)
            {
                return;
            }

            const Vec2 p = positions[closest];

            const int cx = static_cast<int>(p.x / grid.cellWidth());
            const int cy = static_cast<int>(p.y / grid.cellHeight());

            DrawRectangleLinesEx(
                Rectangle{ static_cast<float>(cx - 1) * grid.cellWidth(), static_cast<float>(cy - 1) * grid.cellHeight(),
                           grid.cellWidth() * 3.0f, grid.cellHeight() * 3.0f },
                1.0f, Color{ 255, 255, 255, 90 });

            DrawCircleLinesV(toRay(p), s.perceptionRadius, Color{ 255, 255, 255, 160 });
            DrawCircleLinesV(toRay(p), s.separationRadius, Color{ 255, 110, 110, 160 });

            const float perception2 = s.perceptionRadius * s.perceptionRadius;

            grid.forEachNearby(p, [&](std::uint32_t i)
            {
                if (i == closest)
                {
                    return;
                }

                const Vec2 d = toroidalDelta(p, positions[i], s.worldWidth, s.worldHeight);

                if (d.lengthSquared() <= perception2)
                {
                    DrawLineV(toRay(p), toRay(p + d), Color{ 255, 255, 255, 120 });
                }
            });

            DrawCircleV(toRay(p), 3.0f, WHITE);
        }

        SimulationContext& m_context;
        RenderOptions& m_options;
    };
}
