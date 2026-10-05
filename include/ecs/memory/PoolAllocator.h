#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <new>
#include <utility>
#include <vector>

namespace ecs::memory
{
    // Fixed-size blocks; free blocks form an intrusive linked list.
    class PoolAllocator
    {
    public:
        PoolAllocator(std::size_t blockSize, std::size_t blocksPerChunk,
                      std::size_t alignment = alignof(std::max_align_t))
            : m_alignment(alignment)
            , m_blockSize(roundUp(std::max(blockSize, sizeof(FreeBlock)), alignment))
            , m_blocksPerChunk(blocksPerChunk)
        {
            assert(blocksPerChunk > 0);
            assert(alignment != 0 && (alignment & (alignment - 1)) == 0);
        }

        ~PoolAllocator()
        {
            for (std::byte* chunk : m_chunks)
            {
                ::operator delete(chunk, std::align_val_t{ m_alignment });
            }
        }

        PoolAllocator(const PoolAllocator&) = delete;
        PoolAllocator& operator=(const PoolAllocator&) = delete;

        [[nodiscard]] void* allocate()
        {
            if (!m_freeList)
            {
                addChunk();
            }

            FreeBlock* block = m_freeList;
            m_freeList = block->next;

            ++m_liveBlocks;
            m_peakLiveBlocks = std::max(m_peakLiveBlocks, m_liveBlocks);

            return block;
        }

        void deallocate(void* pointer)
        {
            if (!pointer)
            {
                return;
            }

            assert(owns(pointer) && "Pointer was not allocated by this pool");

            auto* block = static_cast<FreeBlock*>(pointer);
            block->next = m_freeList;
            m_freeList = block;

            --m_liveBlocks;
        }

        [[nodiscard]] bool owns(const void* pointer) const
        {
            const auto* bytes = static_cast<const std::byte*>(pointer);
            const std::size_t chunkBytes = m_blockSize * m_blocksPerChunk;

            for (const std::byte* chunk : m_chunks)
            {
                if (bytes >= chunk && bytes < chunk + chunkBytes)
                {
                    return (static_cast<std::size_t>(bytes - chunk) % m_blockSize) == 0;
                }
            }

            return false;
        }

        [[nodiscard]] std::size_t blockSize() const { return m_blockSize; }
        [[nodiscard]] std::size_t liveBlocks() const { return m_liveBlocks; }
        [[nodiscard]] std::size_t peakLiveBlocks() const { return m_peakLiveBlocks; }
        [[nodiscard]] std::size_t chunkCount() const { return m_chunks.size(); }
        [[nodiscard]] std::size_t capacityBlocks() const { return m_chunks.size() * m_blocksPerChunk; }
        [[nodiscard]] std::size_t reservedBytes() const { return capacityBlocks() * m_blockSize; }

    private:
        struct FreeBlock
        {
            FreeBlock* next;
        };

        static std::size_t roundUp(std::size_t value, std::size_t alignment)
        {
            return (value + alignment - 1) & ~(alignment - 1);
        }

        void addChunk()
        {
            auto* chunk = static_cast<std::byte*>(
                ::operator new(m_blockSize * m_blocksPerChunk, std::align_val_t{ m_alignment }));

            m_chunks.push_back(chunk);

            for (std::size_t i = m_blocksPerChunk; i > 0; --i)
            {
                auto* block = reinterpret_cast<FreeBlock*>(chunk + (i - 1) * m_blockSize);
                block->next = m_freeList;
                m_freeList = block;
            }
        }

        std::size_t m_alignment;
        std::size_t m_blockSize;
        std::size_t m_blocksPerChunk;

        std::vector<std::byte*> m_chunks;
        FreeBlock* m_freeList = nullptr;

        std::size_t m_liveBlocks = 0;
        std::size_t m_peakLiveBlocks = 0;
    };

    template<typename T>
    class ObjectPool
    {
    public:
        explicit ObjectPool(std::size_t objectsPerChunk = 256)
            : m_pool(sizeof(T), objectsPerChunk, std::max(alignof(T), alignof(void*)))
        {
        }

        template<typename... Args>
        [[nodiscard]] T* create(Args&&... args)
        {
            void* memory = m_pool.allocate();
            return ::new (memory) T(std::forward<Args>(args)...);
        }

        void destroy(T* object)
        {
            if (!object)
            {
                return;
            }

            object->~T();
            m_pool.deallocate(object);
        }

        [[nodiscard]] const PoolAllocator& allocator() const { return m_pool; }

    private:
        PoolAllocator m_pool;
    };
}
