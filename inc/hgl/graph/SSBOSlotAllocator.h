#pragma once

#include <cstdint>

namespace hgl::graph
{
    class SSBOSlotAllocator
    {
    private:
        uint32_t capacity = 0;
        uint32_t used_count = 0;
        bool *used_slots = nullptr;

    public:
        SSBOSlotAllocator() = default;
        ~SSBOSlotAllocator();

        bool Init(uint32_t new_capacity);
        void Clear();

        bool Allocate(uint32_t &out_slot);
        bool Release(uint32_t slot);
        bool IsAllocated(uint32_t slot) const;

        uint32_t GetCapacity() const { return capacity; }
        uint32_t GetUsedCount() const { return used_count; }
    };
}
