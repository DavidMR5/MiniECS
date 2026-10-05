#pragma once

#include "Components.h"

#include <ecs/Registry.h>
#include <ecs/memory/LinearAllocator.h>

#include <algorithm>
#include <cstdint>

namespace demo
{
    // Uniform grid rebuilt every frame in the frame arena (counting sort by cell).
    class SpatialGrid
    {
    public:
        bool build(ecs::Registry& registry, ecs::memory::LinearAllocator& arena,
                   float cellSize, float worldWidth, float worldHeight)
        {
            m_columns = std::max(3, static_cast<int>(worldWidth / cellSize));
            m_rows = std::max(3, static_cast<int>(worldHeight / cellSize));
            m_cellWidth = worldWidth / static_cast<float>(m_columns);
            m_cellHeight = worldHeight / static_cast<float>(m_rows);
            m_count = 0;

            const auto cellCount = static_cast<std::size_t>(m_columns) * static_cast<std::size_t>(m_rows);
            const std::size_t capacity = registry.count<Boid>();

            auto* cellOf      = arena.allocateArray<std::uint32_t>(capacity);
            auto* gatherPos   = arena.allocateArray<Vec2>(capacity);
            auto* gatherVel   = arena.allocateArray<Vec2>(capacity);
            auto* gatherId    = arena.allocateArray<ecs::EntityID>(capacity);
            m_cellStart       = arena.allocateArrayZeroed<std::uint32_t>(cellCount + 1);
            m_positions       = arena.allocateArray<Vec2>(capacity);
            m_velocities      = arena.allocateArray<Vec2>(capacity);
            m_ids             = arena.allocateArray<ecs::EntityID>(capacity);

            if (!cellOf || !gatherPos || !gatherVel || !gatherId ||
                !m_cellStart || !m_positions || !m_velocities || !m_ids)
            {
                m_count = 0;
                return false;
            }

            // 1. Gather and count boids per cell.
            registry.view<Position, Velocity, Boid>().each(
                [&](ecs::Entity entity, const Position& position, const Velocity& velocity, const Boid&)
                {
                    const std::uint32_t cell = cellIndex(position.value);

                    cellOf[m_count] = cell;
                    gatherPos[m_count] = position.value;
                    gatherVel[m_count] = velocity.value;
                    gatherId[m_count] = entity.id;

                    ++m_cellStart[cell + 1];
                    ++m_count;
                });

            // 2. Prefix sum: counts -> start offsets.
            for (std::size_t c = 0; c < cellCount; ++c)
            {
                m_cellStart[c + 1] += m_cellStart[c];
            }

            // 3. Scatter into cell order.
            auto* cursor = arena.allocateArray<std::uint32_t>(cellCount);

            if (!cursor)
            {
                m_count = 0;
                return false;
            }

            std::copy(m_cellStart, m_cellStart + cellCount, cursor);

            for (std::uint32_t i = 0; i < m_count; ++i)
            {
                const std::uint32_t destination = cursor[cellOf[i]]++;

                m_positions[destination] = gatherPos[i];
                m_velocities[destination] = gatherVel[i];
                m_ids[destination] = gatherId[i];
            }

            return true;
        }

        template<typename Func>
        void forEachNearby(Vec2 p, Func&& func) const
        {
            const int cx = cellCoord(p.x, m_cellWidth, m_columns);
            const int cy = cellCoord(p.y, m_cellHeight, m_rows);

            for (int dy = -1; dy <= 1; ++dy)
            {
                const int y = wrap(cy + dy, m_rows);

                for (int dx = -1; dx <= 1; ++dx)
                {
                    const int x = wrap(cx + dx, m_columns);
                    const auto cell = static_cast<std::size_t>(y * m_columns + x);

                    for (std::uint32_t i = m_cellStart[cell]; i < m_cellStart[cell + 1]; ++i)
                    {
                        func(i);
                    }
                }
            }
        }

        [[nodiscard]] const Vec2* positions() const { return m_positions; }
        [[nodiscard]] const Vec2* velocities() const { return m_velocities; }
        [[nodiscard]] const ecs::EntityID* ids() const { return m_ids; }
        [[nodiscard]] std::uint32_t count() const { return m_count; }
        [[nodiscard]] int columns() const { return m_columns; }
        [[nodiscard]] int rows() const { return m_rows; }
        [[nodiscard]] float cellWidth() const { return m_cellWidth; }
        [[nodiscard]] float cellHeight() const { return m_cellHeight; }

        [[nodiscard]] std::uint32_t boidsInCell(int x, int y) const
        {
            const auto cell = static_cast<std::size_t>(y * m_columns + x);
            return m_count ? m_cellStart[cell + 1] - m_cellStart[cell] : 0;
        }

    private:
        static int wrap(int value, int size)
        {
            return ((value % size) + size) % size;
        }

        static int cellCoord(float value, float cellExtent, int cells)
        {
            return std::clamp(static_cast<int>(value / cellExtent), 0, cells - 1);
        }

        [[nodiscard]] std::uint32_t cellIndex(Vec2 p) const
        {
            const int x = cellCoord(p.x, m_cellWidth, m_columns);
            const int y = cellCoord(p.y, m_cellHeight, m_rows);
            return static_cast<std::uint32_t>(y * m_columns + x);
        }

        float m_cellWidth = 1.0f;
        float m_cellHeight = 1.0f;
        int m_columns = 1;
        int m_rows = 1;
        std::uint32_t m_count = 0;

        std::uint32_t* m_cellStart = nullptr;
        Vec2* m_positions = nullptr;
        Vec2* m_velocities = nullptr;
        ecs::EntityID* m_ids = nullptr;
    };
}
