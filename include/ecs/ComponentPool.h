#pragma once

#include "Entity.h"
#include "TypeName.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace ecs
{
    struct PoolMemoryUsage
    {
        std::size_t componentBytes = 0;
        std::size_t entityBytes = 0;
        std::size_t sparseBytes = 0;

        [[nodiscard]] std::size_t total() const
        {
            return componentBytes + entityBytes + sparseBytes;
        }
    };

    class IComponentPool
    {
    public:
        virtual ~IComponentPool() = default;

        virtual void remove(EntityID entityId) = 0;

        [[nodiscard]] virtual bool has(EntityID entityId) const = 0;

        [[nodiscard]] virtual std::size_t size() const = 0;

        virtual void clear() = 0;

        [[nodiscard]] virtual PoolMemoryUsage memoryUsage() const = 0;

        [[nodiscard]] virtual const std::string& componentName() const = 0;

        virtual void shrinkToFit() = 0;
    };

    // Sparse set: sparse[entity] -> index into the dense arrays.
    template<typename T>
    class ComponentPool final : public IComponentPool
    {
        static_assert(std::is_move_constructible_v<T>,
            "Components must be move constructible");
        static_assert(std::is_move_assignable_v<T>,
            "Components must be move assignable");

    public:
        using Index = std::uint32_t;
        static constexpr Index NPOS = std::numeric_limits<Index>::max();

        [[nodiscard]] bool has(EntityID entityId) const override
        {
            return entityId < m_sparse.size() && m_sparse[entityId] != NPOS;
        }

        template<typename... Args>
        T& emplace(EntityID entityId, Args&&... args)
        {
            assert(entityId != INVALID_ENTITY);

            if (has(entityId))
            {
                T& existing = m_components[m_sparse[entityId]];
                existing = makeComponent(std::forward<Args>(args)...);
                return existing;
            }

            if (entityId >= m_sparse.size())
            {
                m_sparse.resize(static_cast<std::size_t>(entityId) + 1, NPOS);
            }

            m_sparse[entityId] = static_cast<Index>(m_components.size());
            m_entities.push_back(entityId);
            m_components.push_back(makeComponent(std::forward<Args>(args)...));

            return m_components.back();
        }

        void remove(EntityID entityId) override
        {
            if (!has(entityId))
            {
                return;
            }

            const Index index = m_sparse[entityId];
            const auto lastIndex = static_cast<Index>(m_components.size() - 1);

            if (index != lastIndex)
            {
                // Swap-and-pop: move the last element into the hole.
                const EntityID movedEntity = m_entities[lastIndex];

                m_components[index] = std::move(m_components[lastIndex]);
                m_entities[index] = movedEntity;
                m_sparse[movedEntity] = index;
            }

            m_components.pop_back();
            m_entities.pop_back();
            m_sparse[entityId] = NPOS;
        }

        [[nodiscard]] T& get(EntityID entityId)
        {
            assert(has(entityId) && "Entity does not have this component");
            return m_components[m_sparse[entityId]];
        }

        [[nodiscard]] const T& get(EntityID entityId) const
        {
            assert(has(entityId) && "Entity does not have this component");
            return m_components[m_sparse[entityId]];
        }

        [[nodiscard]] T* tryGet(EntityID entityId)
        {
            return has(entityId) ? &m_components[m_sparse[entityId]] : nullptr;
        }

        [[nodiscard]] const T* tryGet(EntityID entityId) const
        {
            return has(entityId) ? &m_components[m_sparse[entityId]] : nullptr;
        }

        [[nodiscard]] std::span<T> components()
        {
            return m_components;
        }

        [[nodiscard]] std::span<const T> components() const
        {
            return m_components;
        }

        [[nodiscard]] std::span<const EntityID> entities() const
        {
            return m_entities;
        }

        [[nodiscard]] std::size_t size() const override
        {
            return m_components.size();
        }

        void reserve(std::size_t count)
        {
            m_components.reserve(count);
            m_entities.reserve(count);
        }

        void clear() override
        {
            m_sparse.clear();
            m_entities.clear();
            m_components.clear();
        }

        [[nodiscard]] PoolMemoryUsage memoryUsage() const override
        {
            return PoolMemoryUsage{
                m_components.capacity() * sizeof(T),
                m_entities.capacity() * sizeof(EntityID),
                m_sparse.capacity() * sizeof(Index)
            };
        }

        [[nodiscard]] const std::string& componentName() const override
        {
            return typeName<T>();
        }

        void shrinkToFit() override
        {
            std::size_t needed = 0;

            for (const EntityID id : m_entities)
            {
                needed = std::max(needed, static_cast<std::size_t>(id) + 1);
            }

            m_sparse.resize(needed);
            m_sparse.shrink_to_fit();
            m_entities.shrink_to_fit();
            m_components.shrink_to_fit();
        }

    private:
        template<typename... Args>
        static T makeComponent(Args&&... args)
        {
            if constexpr (std::is_aggregate_v<T>)
            {
                return T{ std::forward<Args>(args)... };
            }
            else
            {
                return T(std::forward<Args>(args)...);
            }
        }

        std::vector<Index> m_sparse;
        std::vector<EntityID> m_entities;
        std::vector<T> m_components;
    };
}
