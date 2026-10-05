#pragma once

#include "ComponentPool.h"
#include "ComponentType.h"
#include "Entity.h"
#include "EntityManager.h"
#include "View.h"

#include <cassert>
#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace ecs
{
    struct ComponentPoolStats
    {
        std::string name;
        std::size_t count = 0;
        PoolMemoryUsage memory;
    };

    class Registry
    {
    public:
        Registry() = default;

        Registry(const Registry&) = delete;
        Registry& operator=(const Registry&) = delete;
        Registry(Registry&&) noexcept = default;
        Registry& operator=(Registry&&) noexcept = default;

        [[nodiscard]] Entity createEntity()
        {
            return m_entityManager.create();
        }

        void destroyEntity(Entity entity)
        {
            if (!m_entityManager.isAlive(entity))
            {
                return;
            }

            // Remove components first so a recycled ID starts clean.
            for (auto& pool : m_pools)
            {
                if (pool)
                {
                    pool->remove(entity.id);
                }
            }

            m_entityManager.destroy(entity);
        }

        [[nodiscard]] bool isAlive(Entity entity) const
        {
            return m_entityManager.isAlive(entity);
        }

        [[nodiscard]] std::size_t entityCount() const
        {
            return m_entityManager.size();
        }

        void clear()
        {
            for (auto& pool : m_pools)
            {
                if (pool)
                {
                    pool->clear();
                }
            }

            m_entityManager.clear();
        }

        template<typename T, typename... Args>
        T& emplace(Entity entity, Args&&... args)
        {
            assert(isAlive(entity) && "emplace() on a dead entity");
            return assurePool<T>().emplace(entity.id, std::forward<Args>(args)...);
        }

        template<typename T>
        T& add(Entity entity, T&& component)
        {
            using Bare = std::remove_cvref_t<T>;
            return emplace<Bare>(entity, std::forward<T>(component));
        }

        template<typename T>
        void remove(Entity entity)
        {
            if (!isAlive(entity))
            {
                return;
            }

            if (auto* pool = tryGetPool<T>())
            {
                pool->remove(entity.id);
            }
        }

        template<typename T>
        [[nodiscard]] bool has(Entity entity) const
        {
            const auto* pool = tryGetPool<T>();
            return pool && isAlive(entity) && pool->has(entity.id);
        }

        template<typename... Ts>
        [[nodiscard]] bool hasAll(Entity entity) const
        {
            return (has<Ts>(entity) && ...);
        }

        template<typename T>
        [[nodiscard]] T& get(Entity entity)
        {
            assert(has<T>(entity) && "get() on missing component or dead entity");
            return tryGetPool<T>()->get(entity.id);
        }

        template<typename T>
        [[nodiscard]] const T& get(Entity entity) const
        {
            assert(has<T>(entity) && "get() on missing component or dead entity");
            return tryGetPool<T>()->get(entity.id);
        }

        template<typename T>
        [[nodiscard]] T* tryGet(Entity entity)
        {
            auto* pool = tryGetPool<T>();
            return (pool && isAlive(entity)) ? pool->tryGet(entity.id) : nullptr;
        }

        template<typename T>
        [[nodiscard]] const T* tryGet(Entity entity) const
        {
            const auto* pool = tryGetPool<T>();
            return (pool && isAlive(entity)) ? pool->tryGet(entity.id) : nullptr;
        }

        template<typename T>
        [[nodiscard]] std::size_t count() const
        {
            const auto* pool = tryGetPool<T>();
            return pool ? pool->size() : 0;
        }

        template<typename T>
        void reserve(std::size_t capacity)
        {
            assurePool<T>().reserve(capacity);
        }

        [[nodiscard]] std::vector<ComponentPoolStats> poolStats() const
        {
            std::vector<ComponentPoolStats> stats;
            stats.reserve(m_pools.size());

            for (const auto& pool : m_pools)
            {
                if (pool)
                {
                    stats.push_back({ pool->componentName(), pool->size(), pool->memoryUsage() });
                }
            }

            return stats;
        }

        [[nodiscard]] std::size_t componentMemoryBytes() const
        {
            std::size_t total = 0;

            for (const auto& pool : m_pools)
            {
                if (pool)
                {
                    total += pool->memoryUsage().total();
                }
            }

            return total;
        }

        void shrinkToFit()
        {
            for (auto& pool : m_pools)
            {
                if (pool)
                {
                    pool->shrinkToFit();
                }
            }
        }

        template<typename... Ts>
        [[nodiscard]] View<Ts...> view()
        {
            return View<Ts...>(m_entityManager, tryGetPool<Ts>()...);
        }

    private:
        template<typename T>
        ComponentPool<T>& assurePool()
        {
            static_assert(std::is_same_v<T, std::remove_cvref_t<T>>,
                "Use the plain component type (no const/reference)");

            const ComponentTypeID typeId = componentTypeId<T>();

            if (typeId >= m_pools.size())
            {
                m_pools.resize(static_cast<std::size_t>(typeId) + 1);
            }

            if (!m_pools[typeId])
            {
                m_pools[typeId] = std::make_unique<ComponentPool<T>>();
            }

            return static_cast<ComponentPool<T>&>(*m_pools[typeId]);
        }

        template<typename T>
        ComponentPool<T>* tryGetPool()
        {
            const ComponentTypeID typeId = componentTypeId<T>();

            if (typeId >= m_pools.size() || !m_pools[typeId])
            {
                return nullptr;
            }

            return static_cast<ComponentPool<T>*>(m_pools[typeId].get());
        }

        template<typename T>
        const ComponentPool<T>* tryGetPool() const
        {
            const ComponentTypeID typeId = componentTypeId<T>();

            if (typeId >= m_pools.size() || !m_pools[typeId])
            {
                return nullptr;
            }

            return static_cast<const ComponentPool<T>*>(m_pools[typeId].get());
        }

        EntityManager m_entityManager;
        std::vector<std::unique_ptr<IComponentPool>> m_pools;
    };
}
