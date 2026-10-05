#include "ecs/ECS.h"

#include <iostream>

struct Position
{
    float x = 0.0f;
    float y = 0.0f;
};

struct Velocity
{
    float x = 0.0f;
    float y = 0.0f;
};

struct Lifetime
{
    float remaining = 0.0f;
};

struct Name
{
    const char* value = "";
};

class MovementSystem final : public ecs::System
{
public:
    void update(ecs::Registry& registry, float deltaTime) override
    {
        registry.view<Position, Velocity>().each(
            [deltaTime](Position& position, const Velocity& velocity)
            {
                position.x += velocity.x * deltaTime;
                position.y += velocity.y * deltaTime;
            });
    }
};

class LifetimeSystem final : public ecs::System
{
public:
    void update(ecs::Registry& registry, float deltaTime) override
    {
        registry.view<Lifetime>().each(
            [&registry, deltaTime](ecs::Entity entity, Lifetime& lifetime)
            {
                lifetime.remaining -= deltaTime;

                if (lifetime.remaining <= 0.0f)
                {
                    registry.destroyEntity(entity);
                }
            });
    }
};

static void printState(ecs::Registry& registry)
{
    registry.view<Name, Position>().each(
        [](const Name& name, const Position& position)
        {
            std::cout << "  " << name.value
                      << " -> (" << position.x << ", " << position.y << ")\n";
        });

    std::cout << "  alive entities: " << registry.entityCount() << "\n\n";
}

int main()
{
    ecs::Registry registry;

    const ecs::Entity player = registry.createEntity();
    registry.emplace<Name>(player, "player");
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<Velocity>(player, 10.0f, 5.0f);

    const ecs::Entity bullet = registry.createEntity();
    registry.emplace<Name>(bullet, "bullet");
    registry.emplace<Position>(bullet, 0.0f, 0.0f);
    registry.emplace<Velocity>(bullet, 50.0f, 0.0f);
    registry.emplace<Lifetime>(bullet, 1.5f);

    const ecs::Entity tree = registry.createEntity();
    registry.emplace<Name>(tree, "tree");
    registry.emplace<Position>(tree, 3.0f, 7.0f);

    ecs::Scheduler scheduler;
    scheduler.add<MovementSystem>();
    scheduler.add<LifetimeSystem>();

    std::cout << "MiniECS Demo\n============\n\n";
    std::cout << "t = 0s\n";
    printState(registry);

    for (int second = 1; second <= 2; ++second)
    {
        scheduler.update(registry, 1.0f);

        std::cout << "t = " << second << "s\n";
        printState(registry);
    }

    std::cout << "bullet alive? " << std::boolalpha << registry.isAlive(bullet) << "\n";

    const ecs::Entity recycled = registry.createEntity();
    std::cout << "recycled id " << recycled.id << " (gen " << recycled.generation << ")"
              << " has Position? " << registry.has<Position>(recycled) << "\n";

    return 0;
}
