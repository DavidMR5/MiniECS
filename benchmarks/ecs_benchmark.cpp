// Sparse-set views vs. the original unordered_map storage.
#include "ecs/ECS.h"

#include <chrono>
#include <cstdio>
#include <unordered_map>
#include <vector>

namespace
{
    struct Position { float x = 0.0f; float y = 0.0f; };
    struct Velocity { float x = 1.0f; float y = 1.0f; };

    using Clock = std::chrono::steady_clock;

    template<typename Func>
    double measureMs(Func&& func)
    {
        const auto start = Clock::now();
        func();
        const auto end = Clock::now();

        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    volatile float g_sink = 0.0f;

    template<typename T>
    class HashMapPool
    {
    public:
        void add(ecs::EntityID id, const T& value)
        {
            m_index[id] = m_data.size();
            m_data.push_back(value);
        }

        bool has(ecs::EntityID id) const { return m_index.contains(id); }

        T& get(ecs::EntityID id) { return m_data[m_index.find(id)->second]; }

    private:
        std::vector<T> m_data;
        std::unordered_map<ecs::EntityID, std::size_t> m_index;
    };

    void runBenchmark(std::size_t entityCount, int frames)
    {
        std::printf("--- %zu entities (half moving), %d frames ---\n", entityCount, frames);

        ecs::Registry registry;
        registry.reserve<Position>(entityCount);
        registry.reserve<Velocity>(entityCount / 2);

        std::vector<ecs::Entity> entities;
        entities.reserve(entityCount);

        const double createMs = measureMs([&]
        {
            for (std::size_t i = 0; i < entityCount; ++i)
            {
                const ecs::Entity e = registry.createEntity();
                registry.emplace<Position>(e);

                if (i % 2 == 0)
                {
                    registry.emplace<Velocity>(e);
                }

                entities.push_back(e);
            }
        });

        const double viewMs = measureMs([&]
        {
            for (int frame = 0; frame < frames; ++frame)
            {
                registry.view<Position, Velocity>().each(
                    [](Position& p, const Velocity& v)
                    {
                        p.x += v.x * 0.016f;
                        p.y += v.y * 0.016f;
                    });
            }
        }) / frames;

        const double perEntityMs = measureMs([&]
        {
            for (int frame = 0; frame < frames; ++frame)
            {
                for (const ecs::Entity e : entities)
                {
                    if (registry.has<Position>(e) && registry.has<Velocity>(e))
                    {
                        Position& p = registry.get<Position>(e);
                        const Velocity& v = registry.get<Velocity>(e);
                        p.x += v.x * 0.016f;
                        p.y += v.y * 0.016f;
                    }
                }
            }
        }) / frames;

        const double destroyMs = measureMs([&]
        {
            for (const ecs::Entity e : entities)
            {
                registry.destroyEntity(e);
            }
        });

        HashMapPool<Position> positions;
        HashMapPool<Velocity> velocities;

        for (std::size_t i = 0; i < entityCount; ++i)
        {
            const auto id = static_cast<ecs::EntityID>(i + 1);
            positions.add(id, {});

            if (i % 2 == 0)
            {
                velocities.add(id, {});
            }
        }

        const double baselineMs = measureMs([&]
        {
            for (int frame = 0; frame < frames; ++frame)
            {
                for (std::size_t i = 0; i < entityCount; ++i)
                {
                    const auto id = static_cast<ecs::EntityID>(i + 1);

                    if (positions.has(id) && velocities.has(id))
                    {
                        Position& p = positions.get(id);
                        const Velocity& v = velocities.get(id);
                        p.x += v.x * 0.016f;
                        p.y += v.y * 0.016f;
                    }
                }
            }
        }) / frames;

        g_sink = g_sink + positions.get(1).x;

        std::printf("  create + emplace           : %8.3f ms total\n", createMs);
        std::printf("  destroy all                : %8.3f ms total\n", destroyMs);
        std::printf("  update via view<P,V>       : %8.3f ms/frame\n", viewMs);
        std::printf("  update via has()/get()     : %8.3f ms/frame\n", perEntityMs);
        std::printf("  baseline (unordered_map)   : %8.3f ms/frame\n", baselineMs);
        std::printf("  speed-up view vs baseline  : %8.1fx\n\n", baselineMs / viewMs);
    }
}

int main()
{
#ifndef NDEBUG
    std::printf("WARNING: built without NDEBUG - numbers are not representative.\n\n");
#endif

    runBenchmark(10'000, 200);
    runBenchmark(100'000, 100);
    runBenchmark(1'000'000, 20);

    return 0;
}
