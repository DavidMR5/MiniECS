#pragma once

#include "ComponentPool.h"
#include "Entity.h"
#include "EntityManager.h"

#include <array>
#include <cstddef>
#include <tuple>
#include <type_traits>

namespace ecs
{
    // Iterates the smallest pool and checks membership in the others.
    template<typename... Ts>
    class View
    {
        static_assert(sizeof...(Ts) > 0, "A view needs at least one component type");

    public:
        View(const EntityManager& entities, ComponentPool<Ts>*... pools)
            : m_entities(&entities)
            , m_pools(pools...)
        {
            m_valid = ((pools != nullptr) && ...);

            if (m_valid)
            {
                const std::array<const IComponentPool*, sizeof...(Ts)> all{ pools... };

                m_leading = all[0];

                for (const IComponentPool* pool : all)
                {
                    if (pool->size() < m_leading->size())
                    {
                        m_leading = pool;
                    }
                }
            }
        }

        template<typename Func>
        void each(Func&& func)
        {
            if (!m_valid)
            {
                return;
            }

            forEachLeadingEntity([&](EntityID id)
            {
                if (!containsAll(id))
                {
                    return;
                }

                if constexpr (std::is_invocable_v<Func, Entity, Ts&...>)
                {
                    func(m_entities->handleOf(id), std::get<ComponentPool<Ts>*>(m_pools)->get(id)...);
                }
                else
                {
                    static_assert(std::is_invocable_v<Func, Ts&...>,
                        "View::each callback must accept (Entity, Ts&...) or (Ts&...)");

                    func(std::get<ComponentPool<Ts>*>(m_pools)->get(id)...);
                }
            });
        }

        [[nodiscard]] std::size_t count() const
        {
            if (!m_valid)
            {
                return 0;
            }

            std::size_t result = 0;

            forEachLeadingEntity([&](EntityID id)
            {
                if (containsAll(id))
                {
                    ++result;
                }
            });

            return result;
        }

        [[nodiscard]] std::size_t sizeHint() const
        {
            return m_valid ? m_leading->size() : 0;
        }

    private:
        template<typename Func>
        void forEachLeadingEntity(Func&& func) const
        {
            std::apply([&](auto*... pools)
            {
                const bool found = (tryWalk(pools, func) || ...);
                (void)found;
            }, m_pools);
        }

        template<typename Pool, typename Func>
        bool tryWalk(Pool* pool, Func& func) const
        {
            if (static_cast<const IComponentPool*>(pool) != m_leading)
            {
                return false;
            }

            // Back-to-front: destroying the current entity is safe.
            for (std::size_t i = pool->size(); i > 0; --i)
            {
                if (i - 1 < pool->size())
                {
                    func(pool->entities()[i - 1]);
                }
            }

            return true;
        }

        [[nodiscard]] bool containsAll(EntityID id) const
        {
            return (std::get<ComponentPool<Ts>*>(m_pools)->has(id) && ...);
        }

        const EntityManager* m_entities;
        std::tuple<ComponentPool<Ts>*...> m_pools;
        const IComponentPool* m_leading = nullptr;
        bool m_valid = false;
    };
}
