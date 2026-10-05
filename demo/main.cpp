// Controls: LMB/RMB attract/repel, Up/Down boids, G grid, I inspect, M shrink, Space pause.

#include "Components.h"
#include "Render.h"
#include "Simulation.h"

#include <ecs/ECS.h>

#include <raylib.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

namespace
{
    using namespace demo;

    constexpr int WINDOW_WIDTH = 1600;
    constexpr int WINDOW_HEIGHT = 900;
    constexpr std::size_t MAX_BOIDS = 200'000;

    struct Options
    {
        std::size_t initialBoids = 8000;
        const char* screenshotPath = nullptr;
        int screenshotFrames = 240;
        bool showGrid = false;
    };

    Options parseArguments(int argc, char** argv)
    {
        Options options;

        for (int i = 1; i < argc; ++i)
        {
            const bool hasValue = i + 1 < argc;

            if (std::strcmp(argv[i], "--grid") == 0)
            {
                options.showGrid = true;
            }
            else if (!hasValue)
            {
                break;
            }
            else if (std::strcmp(argv[i], "--boids") == 0)
            {
                options.initialBoids = std::strtoul(argv[++i], nullptr, 10);
            }
            else if (std::strcmp(argv[i], "--screenshot") == 0)
            {
                options.screenshotPath = argv[++i];
            }
            else if (std::strcmp(argv[i], "--frames") == 0)
            {
                options.screenshotFrames = std::atoi(argv[++i]);
            }
        }

        options.initialBoids = std::min(options.initialBoids, MAX_BOIDS);

        return options;
    }

    void spawnBoids(ecs::Registry& registry, const SimulationSettings& settings,
                    std::mt19937& rng, std::size_t count)
    {
        std::uniform_real_distribution<float> x(0.0f, settings.worldWidth);
        std::uniform_real_distribution<float> y(0.0f, settings.worldHeight);
        std::uniform_real_distribution<float> angle(0.0f, 2.0f * PI);
        std::uniform_real_distribution<float> speed(90.0f, 150.0f);
        std::uniform_real_distribution<float> size(3.5f, 5.5f);
        std::uniform_real_distribution<float> hue(-25.0f, 25.0f);

        for (std::size_t i = 0; i < count; ++i)
        {
            const ecs::Entity boid = registry.createEntity();
            const float a = angle(rng);
            const float maxSpeed = speed(rng);

            registry.emplace<Position>(boid, Vec2{ x(rng), y(rng) });
            registry.emplace<Velocity>(boid, Vec2{ std::cos(a) * maxSpeed, std::sin(a) * maxSpeed });
            registry.emplace<Acceleration>(boid);
            registry.emplace<Boid>(boid, maxSpeed, maxSpeed * 1.8f);
            registry.emplace<Renderable>(boid, size(rng), hue(rng));
        }
    }

    void despawnBoids(ecs::Registry& registry, std::size_t count)
    {
        // Safe: views allow destroying the current entity.
        std::size_t removed = 0;

        registry.view<Boid>().each([&](ecs::Entity entity, Boid&)
        {
            if (removed < count)
            {
                registry.destroyEntity(entity);
                ++removed;
            }
        });
    }

    std::string formatBytes(std::size_t bytes)
    {
        if (bytes >= 1024u * 1024u)
        {
            return TextFormat("%.2f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
        }

        return TextFormat("%.1f KB", static_cast<double>(bytes) / 1024.0);
    }

    const char* shortName(const std::string& name)
    {
        const std::size_t separator = name.rfind("::");
        return separator == std::string::npos ? name.c_str() : name.c_str() + separator + 2;
    }

    class Panel
    {
    public:
        Panel(int x, int y) : m_x(x), m_y(y) {}

        void heading(const std::string& text)
        {
            m_y += 6;
            DrawText(text.c_str(), m_x, m_y, 18, Color{ 255, 200, 90, 255 });
            m_y += 22;
        }

        void line(const std::string& text, Color color = RAYWHITE)
        {
            DrawText(text.c_str(), m_x, m_y, 16, color);
            m_y += 19;
        }

        [[nodiscard]] int bottom() const { return m_y; }

    private:
        int m_x;
        int m_y;
    };

    void drawOverlay(const ecs::Registry& registry, const SimulationContext& context,
                     const ecs::Scheduler& simulation, const ecs::Scheduler& rendering,
                     bool paused, bool capped)
    {
        static int panelBottom = 560;
        DrawRectangle(10, 10, 450, panelBottom - 4, Color{ 10, 12, 20, 210 });

        Panel panel(22, 16);

        panel.heading("MiniECS - Boids");
        panel.line(TextFormat("FPS: %d   (%.2f ms/frame)%s%s", GetFPS(), static_cast<double>(GetFrameTime()) * 1000.0,
                              capped ? "  [60 cap]" : "", paused ? "  [PAUSED]" : ""));

        const std::size_t boids = registry.count<Boid>();
        panel.line(TextFormat("Boids: %zu    Entities alive: %zu", boids, registry.entityCount()));

        const double naive = static_cast<double>(boids) * static_cast<double>(boids);
        panel.line(TextFormat("Neighbour checks: %.2f M  (brute force: %.1f M)",
                              static_cast<double>(context.neighborChecks) / 1e6, naive / 1e6),
                   Color{ 180, 180, 190, 255 });

        panel.heading("Systems (avg ms)");

        double total = 0.0;

        for (const auto& timing : simulation.timings())
        {
            panel.line(TextFormat("  %-20s %6.3f", timing.name, timing.averageMs));
            total += timing.averageMs;
        }

        for (const auto& timing : rendering.timings())
        {
            panel.line(TextFormat("  %-20s %6.3f", timing.name, timing.averageMs));
            total += timing.averageMs;
        }

        panel.line(TextFormat("  %-20s %6.3f", "Total", total), Color{ 180, 180, 190, 255 });

        panel.heading("Memory");

        const auto& arena = context.frameArena;
        panel.line(TextFormat("Frame arena: %s / %s", formatBytes(arena.used()).c_str(),
                              formatBytes(arena.capacity()).c_str()));
        panel.line(TextFormat("  peak %s, %zu allocs/frame, 0 frees",
                              formatBytes(arena.highWaterMark()).c_str(), arena.allocationCount()),
                   Color{ 180, 180, 190, 255 });

        if (arena.failedAllocations() > 0)
        {
            panel.line(TextFormat("  %zu failed allocations!", arena.failedAllocations()), RED);
        }

        panel.line(TextFormat("Component pools: %s (capacity)", formatBytes(registry.componentMemoryBytes()).c_str()));

        for (const auto& pool : registry.poolStats())
        {
            panel.line(TextFormat("  %-13s %7zu  %s", shortName(pool.name), pool.count,
                                  formatBytes(pool.memory.total()).c_str()),
                       Color{ 180, 180, 190, 255 });
        }

        panel.heading("Controls");
        panel.line("LMB attract  RMB repel  Up/Down +/-1000 boids", Color{ 160, 160, 170, 255 });
        panel.line("G grid  I inspect  M shrink  Space pause  V cap", Color{ 160, 160, 170, 255 });

        panelBottom = panel.bottom();
    }
}

int main(int argc, char** argv)
{
    const Options options = parseArguments(argc, argv);

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "MiniECS - Boids");
    SetTargetFPS(0);

    SimulationContext context;
    context.settings.worldWidth = static_cast<float>(WINDOW_WIDTH);
    context.settings.worldHeight = static_cast<float>(WINDOW_HEIGHT);

    RenderOptions renderOptions;
    renderOptions.showGrid = options.showGrid;

    ecs::Registry registry;
    std::mt19937 rng(1234);

    spawnBoids(registry, context.settings, rng, options.initialBoids);

    ecs::Scheduler simulation;
    simulation.add<SpatialGridSystem>(context);
    simulation.add<FlockingSystem>(context);
    simulation.add<MovementSystem>(context);

    ecs::Scheduler rendering;
    rendering.add<RenderSystem>(context, renderOptions);

    bool paused = false;
    bool capped = false;
    bool showOverlay = true;
    int frame = 0;

    if (options.screenshotPath)
    {
        renderOptions.inspect = true;
    }

    while (!WindowShouldClose())
    {
        const Vector2 mouse = GetMousePosition();
        context.mouse = Vec2{ mouse.x, mouse.y };
        context.mouseMode = IsMouseButtonDown(MOUSE_BUTTON_LEFT)    ? 1
                          : IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ? -1
                                                                  : 0;

        const std::size_t step = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 5000u : 1000u;

        if (IsKeyPressed(KEY_UP))
        {
            const std::size_t current = registry.count<Boid>();
            spawnBoids(registry, context.settings, rng, std::min(step, MAX_BOIDS - current));
        }

        if (IsKeyPressed(KEY_DOWN)) despawnBoids(registry, step);
        if (IsKeyPressed(KEY_G)) renderOptions.showGrid = !renderOptions.showGrid;
        if (IsKeyPressed(KEY_I)) renderOptions.inspect = !renderOptions.inspect;
        if (IsKeyPressed(KEY_M)) registry.shrinkToFit();
        if (IsKeyPressed(KEY_H)) showOverlay = !showOverlay;
        if (IsKeyPressed(KEY_SPACE)) paused = !paused;

        if (IsKeyPressed(KEY_V))
        {
            capped = !capped;
            SetTargetFPS(capped ? 60 : 0);
        }

        const float dt = std::min(GetFrameTime(), 1.0f / 30.0f);

        if (!paused)
        {
            simulation.update(registry, options.screenshotPath ? 1.0f / 60.0f : dt);
        }

        BeginDrawing();
        ClearBackground(Color{ 14, 16, 26, 255 });

        rendering.update(registry, dt);

        if (showOverlay)
        {
            drawOverlay(registry, context, simulation, rendering, paused, capped);
        }

        EndDrawing();

        ++frame;

        if (options.screenshotPath && frame >= options.screenshotFrames)
        {
            TakeScreenshot(options.screenshotPath);
            break;
        }
    }

    CloseWindow();

    return 0;
}
