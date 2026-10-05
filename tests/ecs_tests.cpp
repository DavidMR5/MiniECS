#include "ecs/ECS.h"

#include <cstdint>
#include <cstdio>
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace
{
    struct TestCase
    {
        const char* name;
        std::function<void()> body;
    };

    std::vector<TestCase>& registryOfTests()
    {
        static std::vector<TestCase> tests;
        return tests;
    }

    int g_failures = 0;

    struct Registrar
    {
        Registrar(const char* name, std::function<void()> body)
        {
            registryOfTests().push_back({ name, std::move(body) });
        }
    };
}

#define TEST(name)                                              \
    static void name();                                         \
    static const Registrar registrar_##name(#name, &name);      \
    static void name()

#define CHECK(expr)                                                         \
    do                                                                      \
    {                                                                       \
        if (!(expr))                                                        \
        {                                                                   \
            std::printf("    FAILED: %s  (%s:%d)\n", #expr, __FILE__, __LINE__); \
            ++g_failures;                                                   \
        }                                                                   \
    } while (false)

struct Position { float x = 0.0f; float y = 0.0f; };
struct Velocity { float x = 0.0f; float y = 0.0f; };
struct Health   { int value = 100; };
struct Tag      {};

struct NonAggregate
{
    NonAggregate(int a, int b) : sum(a + b) {}
    int sum;
};

TEST(default_entity_is_invalid)
{
    ecs::Registry registry;
    ecs::Entity entity;

    CHECK(!entity.isValid());
    CHECK(!registry.isAlive(entity));
}

TEST(create_gives_unique_live_entities)
{
    ecs::Registry registry;

    const ecs::Entity a = registry.createEntity();
    const ecs::Entity b = registry.createEntity();

    CHECK(a.isValid() && b.isValid());
    CHECK(a != b);
    CHECK(registry.isAlive(a) && registry.isAlive(b));
    CHECK(registry.entityCount() == 2);
}

TEST(destroyed_ids_are_recycled_with_new_generation)
{
    ecs::Registry registry;

    const ecs::Entity a = registry.createEntity();
    registry.destroyEntity(a);
    const ecs::Entity b = registry.createEntity();

    CHECK(b.id == a.id);
    CHECK(b.generation == a.generation + 1);
    CHECK(!registry.isAlive(a));
    CHECK(registry.isAlive(b));
    CHECK(registry.entityCount() == 1);
}

TEST(double_destroy_is_harmless)
{
    ecs::Registry registry;

    const ecs::Entity a = registry.createEntity();
    registry.destroyEntity(a);
    registry.destroyEntity(a);

    const ecs::Entity b = registry.createEntity();
    const ecs::Entity c = registry.createEntity();

    CHECK(b.id != c.id);
    CHECK(registry.entityCount() == 2);
}

TEST(emplace_get_has_remove)
{
    ecs::Registry registry;
    const ecs::Entity e = registry.createEntity();

    CHECK(!registry.has<Position>(e));

    registry.emplace<Position>(e, 1.0f, 2.0f);

    CHECK(registry.has<Position>(e));
    CHECK(registry.get<Position>(e).x == 1.0f);
    CHECK(registry.get<Position>(e).y == 2.0f);

    registry.get<Position>(e).x = 5.0f;
    CHECK(registry.get<Position>(e).x == 5.0f);

    registry.remove<Position>(e);
    CHECK(!registry.has<Position>(e));
    CHECK(registry.tryGet<Position>(e) == nullptr);
}

TEST(emplace_twice_replaces_component)
{
    ecs::Registry registry;
    const ecs::Entity e = registry.createEntity();

    registry.emplace<Health>(e, 10);
    registry.emplace<Health>(e, 42);

    CHECK(registry.get<Health>(e).value == 42);
    CHECK(registry.count<Health>() == 1);
}

TEST(add_accepts_a_component_value)
{
    ecs::Registry registry;
    const ecs::Entity e = registry.createEntity();

    const Velocity velocity{ 3.0f, 4.0f };
    registry.add(e, velocity);

    CHECK(registry.get<Velocity>(e).y == 4.0f);
}

TEST(emplace_supports_non_aggregate_types)
{
    ecs::Registry registry;
    const ecs::Entity e = registry.createEntity();

    registry.emplace<NonAggregate>(e, 2, 3);

    CHECK(registry.get<NonAggregate>(e).sum == 5);
}

TEST(remove_keeps_other_entities_data_intact)
{
    ecs::Registry registry;

    std::vector<ecs::Entity> entities;
    for (int i = 0; i < 5; ++i)
    {
        entities.push_back(registry.createEntity());
        registry.emplace<Health>(entities.back(), i);
    }

    registry.remove<Health>(entities[1]);

    CHECK(registry.count<Health>() == 4);
    CHECK(registry.get<Health>(entities[0]).value == 0);
    CHECK(registry.get<Health>(entities[2]).value == 2);
    CHECK(registry.get<Health>(entities[3]).value == 3);
    CHECK(registry.get<Health>(entities[4]).value == 4);
}

// Regression: destroy used to leave components behind.
TEST(destroy_removes_all_components)
{
    ecs::Registry registry;

    const ecs::Entity a = registry.createEntity();
    registry.emplace<Position>(a, 1.0f, 2.0f);
    registry.emplace<Health>(a, 7);

    registry.destroyEntity(a);

    CHECK(registry.count<Position>() == 0);
    CHECK(registry.count<Health>() == 0);

    const ecs::Entity b = registry.createEntity();
    CHECK(b.id == a.id);
    CHECK(!registry.has<Position>(b));
    CHECK(!registry.has<Health>(b));
}

TEST(stale_handle_cannot_access_new_entity_data)
{
    ecs::Registry registry;

    const ecs::Entity a = registry.createEntity();
    registry.destroyEntity(a);

    const ecs::Entity b = registry.createEntity();
    registry.emplace<Health>(b, 99);

    CHECK(registry.has<Health>(b));
    CHECK(!registry.has<Health>(a));
    CHECK(registry.tryGet<Health>(a) == nullptr);

    registry.remove<Health>(a);
    CHECK(registry.has<Health>(b));
}

TEST(empty_tag_components_work)
{
    ecs::Registry registry;
    const ecs::Entity e = registry.createEntity();

    registry.emplace<Tag>(e);

    CHECK(registry.has<Tag>(e));
    CHECK((registry.hasAll<Tag>(e)));
    CHECK(!(registry.hasAll<Tag, Position>(e)));
}

TEST(clear_destroys_everything)
{
    ecs::Registry registry;

    const ecs::Entity a = registry.createEntity();
    registry.emplace<Position>(a);

    registry.clear();

    CHECK(registry.entityCount() == 0);
    CHECK(!registry.isAlive(a));
    CHECK(registry.count<Position>() == 0);

    const ecs::Entity b = registry.createEntity();
    CHECK(registry.isAlive(b));
    CHECK(!registry.has<Position>(b));
}

TEST(view_visits_only_entities_with_all_components)
{
    ecs::Registry registry;

    const ecs::Entity moving = registry.createEntity();
    registry.emplace<Position>(moving);
    registry.emplace<Velocity>(moving, 1.0f, 0.0f);

    const ecs::Entity still = registry.createEntity();
    registry.emplace<Position>(still);

    const ecs::Entity ghost = registry.createEntity();
    registry.emplace<Velocity>(ghost);

    std::set<ecs::EntityID> visited;
    registry.view<Position, Velocity>().each(
        [&](ecs::Entity entity, Position&, Velocity&)
        {
            visited.insert(entity.id);
        });

    CHECK(visited.size() == 1);
    CHECK(visited.count(moving.id) == 1);
    CHECK((registry.view<Position, Velocity>().count() == 1));
}

TEST(view_callback_without_entity_parameter)
{
    ecs::Registry registry;

    for (int i = 0; i < 3; ++i)
    {
        const ecs::Entity e = registry.createEntity();
        registry.emplace<Position>(e, 0.0f, 0.0f);
        registry.emplace<Velocity>(e, 1.0f, 2.0f);
    }

    registry.view<Position, Velocity>().each(
        [](Position& p, const Velocity& v)
        {
            p.x += v.x;
            p.y += v.y;
        });

    int checked = 0;
    registry.view<Position>().each(
        [&](const Position& p)
        {
            CHECK(p.x == 1.0f && p.y == 2.0f);
            ++checked;
        });

    CHECK(checked == 3);
}

TEST(view_entity_handles_are_current)
{
    ecs::Registry registry;

    const ecs::Entity a = registry.createEntity();
    registry.destroyEntity(a);
    const ecs::Entity b = registry.createEntity();
    registry.emplace<Health>(b);

    registry.view<Health>().each(
        [&](ecs::Entity entity, Health&)
        {
            CHECK(entity == b);
            CHECK(registry.isAlive(entity));
        });
}

TEST(view_of_unused_component_is_empty)
{
    struct NeverUsed {};

    ecs::Registry registry;
    const ecs::Entity e = registry.createEntity();
    registry.emplace<Position>(e);

    int calls = 0;
    registry.view<Position, NeverUsed>().each([&](Position&, NeverUsed&) { ++calls; });

    CHECK(calls == 0);
    CHECK((registry.view<Position, NeverUsed>().count() == 0));
}

TEST(view_iterates_smallest_pool)
{
    ecs::Registry registry;

    for (int i = 0; i < 100; ++i)
    {
        const ecs::Entity e = registry.createEntity();
        registry.emplace<Position>(e);

        if (i % 10 == 0)
        {
            registry.emplace<Velocity>(e);
        }
    }

    auto view = registry.view<Position, Velocity>();

    CHECK(view.sizeHint() == 10);
    CHECK(view.count() == 10);
}

TEST(destroying_current_entity_during_view_is_safe)
{
    ecs::Registry registry;

    for (int i = 0; i < 10; ++i)
    {
        const ecs::Entity e = registry.createEntity();
        registry.emplace<Health>(e, i);
    }

    int visits = 0;
    registry.view<Health>().each(
        [&](ecs::Entity entity, Health& health)
        {
            ++visits;

            if (health.value % 2 == 0)
            {
                registry.destroyEntity(entity);
            }
        });

    CHECK(visits == 10);
    CHECK(registry.entityCount() == 5);
    CHECK(registry.count<Health>() == 5);

    registry.view<Health>().each([&](Health& health) { CHECK(health.value % 2 == 1); });
}

TEST(scheduler_runs_systems_in_order)
{
    struct Counter { std::string log; };

    struct AppendA final : ecs::System
    {
        void update(ecs::Registry& registry, float) override
        {
            registry.view<Counter>().each([](Counter& c) { c.log += "A"; });
        }
    };

    struct AppendB final : ecs::System
    {
        void update(ecs::Registry& registry, float) override
        {
            registry.view<Counter>().each([](Counter& c) { c.log += "B"; });
        }
    };

    ecs::Registry registry;
    const ecs::Entity e = registry.createEntity();
    registry.emplace<Counter>(e);

    ecs::Scheduler scheduler;
    scheduler.add<AppendA>();
    scheduler.add<AppendB>();

    scheduler.update(registry, 0.016f);
    scheduler.update(registry, 0.016f);

    CHECK(registry.get<Counter>(e).log == "ABAB");
}

TEST(arena_respects_alignment)
{
    ecs::memory::LinearAllocator arena(1024);

    void* a = arena.allocate(1, 1);
    void* b = arena.allocate(8, 16);
    double* c = arena.allocateArray<double>(4);

    CHECK(a != nullptr && b != nullptr && c != nullptr);
    CHECK(reinterpret_cast<std::uintptr_t>(b) % 16 == 0);
    CHECK(reinterpret_cast<std::uintptr_t>(c) % alignof(double) == 0);
    CHECK(arena.allocationCount() == 3);
}

TEST(arena_reset_reuses_memory)
{
    ecs::memory::LinearAllocator arena(256);

    int* first = arena.allocateArray<int>(16);
    arena.reset();
    int* second = arena.allocateArray<int>(16);

    CHECK(first == second);
    CHECK(arena.used() == 16 * sizeof(int));
    CHECK(arena.highWaterMark() == 16 * sizeof(int));
}

TEST(arena_reports_overflow_instead_of_corrupting)
{
    ecs::memory::LinearAllocator arena(64);

    CHECK(arena.allocate(48) != nullptr);
    CHECK(arena.allocate(32) == nullptr);
    CHECK(arena.failedAllocations() == 1);
    CHECK(arena.used() == 48);
}

TEST(arena_marker_and_rewind)
{
    ecs::memory::LinearAllocator arena(256);

    (void)arena.allocate(32);
    const std::size_t marker = arena.marker();
    (void)arena.allocate(64);
    arena.rewind(marker);

    CHECK(arena.used() == 32);
}

TEST(arena_zeroed_array)
{
    ecs::memory::LinearAllocator arena(256);

    std::uint32_t* values = arena.allocateArrayZeroed<std::uint32_t>(8);

    bool allZero = true;
    for (int i = 0; i < 8; ++i)
    {
        allZero = allZero && values[i] == 0;
    }

    CHECK(allZero);
}

TEST(pool_reuses_freed_blocks)
{
    ecs::memory::PoolAllocator pool(32, 4);

    void* a = pool.allocate();
    void* b = pool.allocate();
    CHECK(a != b);
    CHECK(pool.liveBlocks() == 2);

    pool.deallocate(a);
    void* c = pool.allocate();

    CHECK(c == a);
    CHECK(pool.liveBlocks() == 2);
    CHECK(pool.chunkCount() == 1);

    pool.deallocate(b);
    pool.deallocate(c);
    CHECK(pool.liveBlocks() == 0);
    CHECK(pool.peakLiveBlocks() == 2);
}

TEST(pool_grows_by_chunks_and_keeps_pointers_stable)
{
    ecs::memory::PoolAllocator pool(sizeof(int), 4);

    std::vector<int*> values;
    for (int i = 0; i < 10; ++i)
    {
        int* value = static_cast<int*>(pool.allocate());
        *value = i;
        values.push_back(value);
    }

    CHECK(pool.chunkCount() == 3);
    CHECK(pool.capacityBlocks() == 12);

    bool intact = true;
    for (int i = 0; i < 10; ++i)
    {
        intact = intact && *values[static_cast<std::size_t>(i)] == i;
        CHECK(pool.owns(values[static_cast<std::size_t>(i)]));
    }

    CHECK(intact);

    for (int* value : values)
    {
        pool.deallocate(value);
    }
}

TEST(pool_block_size_is_aligned)
{
    ecs::memory::PoolAllocator pool(3, 8, 16);

    CHECK(pool.blockSize() == 16);

    void* a = pool.allocate();
    void* b = pool.allocate();

    CHECK(reinterpret_cast<std::uintptr_t>(a) % 16 == 0);
    CHECK(reinterpret_cast<std::uintptr_t>(b) % 16 == 0);

    pool.deallocate(a);
    pool.deallocate(b);
}

TEST(object_pool_constructs_and_destroys)
{
    static int liveObjects = 0;

    struct Tracked
    {
        explicit Tracked(int v) : value(v) { ++liveObjects; }
        ~Tracked() { --liveObjects; }
        int value;
    };

    ecs::memory::ObjectPool<Tracked> pool(8);

    Tracked* a = pool.create(7);
    Tracked* b = pool.create(9);

    CHECK(liveObjects == 2);
    CHECK(a->value == 7 && b->value == 9);

    pool.destroy(a);
    pool.destroy(b);

    CHECK(liveObjects == 0);
    CHECK(pool.allocator().liveBlocks() == 0);
}

TEST(registry_reports_pool_memory)
{
    ecs::Registry registry;

    for (int i = 0; i < 100; ++i)
    {
        const ecs::Entity e = registry.createEntity();
        registry.emplace<Position>(e);
    }

    const auto stats = registry.poolStats();

    bool found = false;
    for (const auto& pool : stats)
    {
        if (pool.name == "Position")
        {
            found = true;
            CHECK(pool.count == 100);
            CHECK(pool.memory.componentBytes >= 100 * sizeof(Position));
            CHECK(pool.memory.entityBytes >= 100 * sizeof(ecs::EntityID));
            CHECK(pool.memory.sparseBytes >= 101 * sizeof(std::uint32_t));
        }
    }

    CHECK(found);
    CHECK(registry.componentMemoryBytes() >= 100 * sizeof(Position));
}

TEST(shrink_to_fit_releases_capacity)
{
    ecs::Registry registry;

    std::vector<ecs::Entity> entities;
    for (int i = 0; i < 1000; ++i)
    {
        entities.push_back(registry.createEntity());
        registry.emplace<Health>(entities.back(), i);
    }

    const std::size_t before = registry.componentMemoryBytes();

    for (std::size_t i = 10; i < entities.size(); ++i)
    {
        registry.destroyEntity(entities[i]);
    }

    registry.shrinkToFit();

    CHECK(registry.componentMemoryBytes() < before);
    CHECK(registry.count<Health>() == 10);
    CHECK(registry.get<Health>(entities[5]).value == 5);
}

int main()
{
    int failedTests = 0;

    for (const TestCase& test : registryOfTests())
    {
        const int before = g_failures;

        test.body();

        const bool passed = (g_failures == before);
        failedTests += passed ? 0 : 1;

        std::printf("[%s] %s\n", passed ? " OK " : "FAIL", test.name);
    }

    std::printf("\n%zu tests, %d failed\n", registryOfTests().size(), failedTests);

    return failedTests == 0 ? 0 : 1;
}
