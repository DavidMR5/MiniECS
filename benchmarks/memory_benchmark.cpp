// Arena vs. heap for per-frame data, pool vs. new/delete.
#include "ecs/memory/LinearAllocator.h"
#include "ecs/memory/PoolAllocator.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <random>
#include <vector>

namespace
{
    using Clock = std::chrono::steady_clock;

    template<typename Func>
    double measureMs(Func&& func)
    {
        const auto start = Clock::now();
        func();
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }

    volatile std::uint64_t g_sink = 0;

    struct Vec2 { float x; float y; };

    std::uint64_t rebuildWithVectors(const std::vector<Vec2>& points, int columns, int rows, float cell)
    {
        std::vector<std::vector<std::uint32_t>> cells(static_cast<std::size_t>(columns * rows));

        for (std::uint32_t i = 0; i < points.size(); ++i)
        {
            const int cx = std::min(columns - 1, static_cast<int>(points[i].x / cell));
            const int cy = std::min(rows - 1, static_cast<int>(points[i].y / cell));
            cells[static_cast<std::size_t>(cy * columns + cx)].push_back(i);
        }

        std::uint64_t checksum = 0;
        for (const auto& c : cells)
        {
            checksum += c.size();
        }

        return checksum;
    }

    std::uint64_t rebuildWithArena(ecs::memory::LinearAllocator& arena, const std::vector<Vec2>& points,
                                   int columns, int rows, float cell)
    {
        arena.reset();

        const auto cellCount = static_cast<std::size_t>(columns * rows);
        const std::size_t n = points.size();

        auto* cellOf = arena.allocateArray<std::uint32_t>(n);
        auto* start = arena.allocateArrayZeroed<std::uint32_t>(cellCount + 1);
        auto* cursor = arena.allocateArray<std::uint32_t>(cellCount);
        auto* sorted = arena.allocateArray<std::uint32_t>(n);

        for (std::uint32_t i = 0; i < n; ++i)
        {
            const int cx = std::min(columns - 1, static_cast<int>(points[i].x / cell));
            const int cy = std::min(rows - 1, static_cast<int>(points[i].y / cell));
            cellOf[i] = static_cast<std::uint32_t>(cy * columns + cx);
            ++start[cellOf[i] + 1];
        }

        for (std::size_t c = 0; c < cellCount; ++c)
        {
            start[c + 1] += start[c];
        }

        std::copy(start, start + cellCount, cursor);

        for (std::uint32_t i = 0; i < n; ++i)
        {
            sorted[cursor[cellOf[i]]++] = i;
        }

        return start[cellCount] + sorted[0];
    }

    void benchmarkScratch(std::size_t pointCount, int frames)
    {
        constexpr float WIDTH = 1600.0f;
        constexpr float HEIGHT = 900.0f;
        constexpr float CELL = 40.0f;
        constexpr int COLUMNS = static_cast<int>(WIDTH / CELL);
        constexpr int ROWS = static_cast<int>(HEIGHT / CELL);

        std::mt19937 rng(42);
        std::uniform_real_distribution<float> x(0.0f, WIDTH);
        std::uniform_real_distribution<float> y(0.0f, HEIGHT);

        std::vector<Vec2> points(pointCount);
        for (Vec2& p : points)
        {
            p = { x(rng), y(rng) };
        }

        ecs::memory::LinearAllocator arena(64u * 1024u * 1024u);

        const double vectorMs = measureMs([&]
        {
            for (int f = 0; f < frames; ++f)
            {
                g_sink = g_sink + rebuildWithVectors(points, COLUMNS, ROWS, CELL);
            }
        }) / frames;

        const double arenaMs = measureMs([&]
        {
            for (int f = 0; f < frames; ++f)
            {
                g_sink = g_sink + rebuildWithArena(arena, points, COLUMNS, ROWS, CELL);
            }
        }) / frames;

        std::printf("  %8zu points | vector-per-cell %8.3f ms | arena + counting sort %8.3f ms | %5.1fx | arena peak %.2f MB\n",
                    pointCount, vectorMs, arenaMs, vectorMs / arenaMs,
                    static_cast<double>(arena.highWaterMark()) / (1024.0 * 1024.0));
    }

    struct Particle
    {
        float position[3];
        float velocity[3];
        float lifetime;
        std::uint32_t flags;
    };

    std::vector<std::vector<std::uint32_t>> makeChurnOrders(std::size_t count, int rounds)
    {
        std::mt19937 rng(7);
        std::vector<std::vector<std::uint32_t>> orders(static_cast<std::size_t>(rounds));

        for (auto& order : orders)
        {
            order.resize(count);
            std::iota(order.begin(), order.end(), 0u);
            std::shuffle(order.begin(), order.end(), rng);
            order.resize(count / 2);
        }

        return orders;
    }

    template<typename Allocate, typename Free>
    double churn(std::size_t count, const std::vector<std::vector<std::uint32_t>>& orders,
                 Allocate&& allocate, Free&& release)
    {
        std::vector<Particle*> live(count, nullptr);

        return measureMs([&]
        {
            for (std::size_t i = 0; i < count; ++i)
            {
                live[i] = allocate();
            }

            for (const auto& order : orders)
            {
                for (const std::uint32_t index : order)
                {
                    release(live[index]);
                }

                for (const std::uint32_t index : order)
                {
                    live[index] = allocate();
                    live[index]->lifetime = 1.0f;
                }
            }

            for (Particle* p : live)
            {
                g_sink = g_sink + p->flags;
                release(p);
            }
        });
    }

    void benchmarkObjects(std::size_t count, int rounds)
    {
        const auto orders = makeChurnOrders(count, rounds);

        const double heapMs = churn(count, orders,
            [] { return new Particle{}; },
            [](Particle* p) { delete p; });

        ecs::memory::ObjectPool<Particle> pool(4096);

        const double poolMs = churn(count, orders,
            [&] { return pool.create(); },
            [&](Particle* p) { pool.destroy(p); });

        std::printf("  %8zu objects x %d rounds | new/delete %8.3f ms | PoolAllocator %8.3f ms | %5.1fx | %zu chunks\n",
                    count, rounds, heapMs, poolMs, heapMs / poolMs, pool.allocator().chunkCount());
    }
}

int main()
{
#ifndef NDEBUG
    std::printf("WARNING: built without NDEBUG - numbers are not representative.\n\n");
#endif

    std::printf("Per-frame spatial grid rebuild (1600x900 world, 40 px cells), avg per frame:\n");
    benchmarkScratch(10'000, 200);
    benchmarkScratch(100'000, 100);
    benchmarkScratch(1'000'000, 20);

    std::printf("\nFixed-size object churn (allocate all, then free/refill a random half per round):\n");
    benchmarkObjects(10'000, 50);
    benchmarkObjects(100'000, 20);
    benchmarkObjects(1'000'000, 5);

    return 0;
}
