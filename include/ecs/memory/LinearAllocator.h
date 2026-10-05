#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>

namespace ecs::memory
{
    // Bump allocator: no individual frees, reset() releases everything.
    class LinearAllocator
    {
    public:
        explicit LinearAllocator(std::size_t capacityBytes)
            : m_buffer(static_cast<std::byte*>(
                  ::operator new(capacityBytes, std::align_val_t{ MAX_ALIGN })))
            , m_capacity(capacityBytes)
        {
        }

        ~LinearAllocator()
        {
            ::operator delete(m_buffer, std::align_val_t{ MAX_ALIGN });
        }

        LinearAllocator(const LinearAllocator&) = delete;
        LinearAllocator& operator=(const LinearAllocator&) = delete;

        [[nodiscard]] void* allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t))
        {
            assert(alignment != 0 && (alignment & (alignment - 1)) == 0 && "alignment must be a power of two");
            assert(alignment <= MAX_ALIGN);

            const std::size_t aligned = (m_offset + alignment - 1) & ~(alignment - 1); // round up

            if (aligned + bytes > m_capacity)
            {
                ++m_failedAllocations;
                return nullptr;
            }

            m_offset = aligned + bytes;
            ++m_allocationCount;

            if (m_offset > m_highWaterMark)
            {
                m_highWaterMark = m_offset;
            }

            return m_buffer + aligned;
        }

        template<typename T>
        [[nodiscard]] T* allocateArray(std::size_t count)
        {
            static_assert(std::is_trivially_destructible_v<T>,
                "LinearAllocator never runs destructors; use trivially destructible types");

            return static_cast<T*>(allocate(sizeof(T) * count, alignof(T)));
        }

        template<typename T>
        [[nodiscard]] T* allocateArrayZeroed(std::size_t count)
        {
            T* data = allocateArray<T>(count);

            if (data)
            {
                std::uninitialized_value_construct_n(data, count);
            }

            return data;
        }

        void reset()
        {
            m_offset = 0;
            m_allocationCount = 0;
        }

        [[nodiscard]] std::size_t marker() const { return m_offset; }

        void rewind(std::size_t marker)
        {
            assert(marker <= m_offset);
            m_offset = marker;
        }

        [[nodiscard]] std::size_t used() const { return m_offset; }
        [[nodiscard]] std::size_t capacity() const { return m_capacity; }
        [[nodiscard]] std::size_t highWaterMark() const { return m_highWaterMark; }
        [[nodiscard]] std::size_t allocationCount() const { return m_allocationCount; }
        [[nodiscard]] std::size_t failedAllocations() const { return m_failedAllocations; }

    private:
        static constexpr std::size_t MAX_ALIGN = 64;

        std::byte* m_buffer = nullptr;
        std::size_t m_capacity = 0;
        std::size_t m_offset = 0;
        std::size_t m_highWaterMark = 0;
        std::size_t m_allocationCount = 0;
        std::size_t m_failedAllocations = 0;
    };
}
