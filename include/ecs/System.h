#pragma once

#include "Registry.h"

#include <chrono>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace ecs
{
    class System
    {
    public:
        virtual ~System() = default;

        virtual void update(Registry& registry, float deltaTime) = 0;

        [[nodiscard]] virtual const char* name() const
        {
            return "System";
        }
    };

    class Scheduler
    {
    public:
        struct Timing
        {
            const char* name = "";
            double lastMs = 0.0;
            double averageMs = 0.0;
        };

        template<typename T, typename... Args>
        T& add(Args&&... args)
        {
            static_assert(std::is_base_of_v<System, T>, "T must derive from ecs::System");

            auto system = std::make_unique<T>(std::forward<Args>(args)...);
            T& reference = *system;

            m_timings.push_back(Timing{ system->name(), 0.0, 0.0 });
            m_systems.push_back(std::move(system));

            return reference;
        }

        void update(Registry& registry, float deltaTime)
        {
            using Clock = std::chrono::steady_clock;

            for (std::size_t i = 0; i < m_systems.size(); ++i)
            {
                const auto start = Clock::now();

                m_systems[i]->update(registry, deltaTime);

                const double ms =
                    std::chrono::duration<double, std::milli>(Clock::now() - start).count();

                Timing& timing = m_timings[i];
                timing.lastMs = ms;
                timing.averageMs = (timing.averageMs == 0.0)
                    ? ms
                    : timing.averageMs + SMOOTHING * (ms - timing.averageMs);
            }
        }

        [[nodiscard]] const std::vector<Timing>& timings() const
        {
            return m_timings;
        }

        [[nodiscard]] std::size_t size() const
        {
            return m_systems.size();
        }

    private:
        static constexpr double SMOOTHING = 0.05;

        std::vector<std::unique_ptr<System>> m_systems;
        std::vector<Timing> m_timings;
    };
}
