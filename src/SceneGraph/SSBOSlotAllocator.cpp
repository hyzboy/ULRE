#include <hgl/graph/SSBOSlotAllocator.h>
#include <cstring>

namespace hgl::graph
{
    SSBOSlotAllocator::~SSBOSlotAllocator()
    {
        Clear();
    }

    bool SSBOSlotAllocator::Init(const uint32_t new_capacity)
    {
        Clear();

        if (new_capacity == 0)
            return false;

        used_slots = new bool[new_capacity];
        if (!used_slots)
            return false;

        capacity = new_capacity;
        used_count = 0;
        memset(used_slots, 0, sizeof(bool) * capacity);
        return true;
    }

    void SSBOSlotAllocator::Clear()
    {
        delete[] used_slots;
        used_slots = nullptr;
        capacity = 0;
        used_count = 0;
    }

    bool SSBOSlotAllocator::Allocate(uint32_t &out_slot)
    {
        if (!used_slots || capacity == 0)
            return false;

        for (uint32_t i = 0; i < capacity; ++i)
        {
            if (used_slots[i])
                continue;

            used_slots[i] = true;
            ++used_count;
            out_slot = i;
            return true;
        }

        return false;
    }

    bool SSBOSlotAllocator::Release(const uint32_t slot)
    {
        if (!used_slots || slot >= capacity || !used_slots[slot])
            return false;

        used_slots[slot] = false;
        --used_count;
        return true;
    }

    bool SSBOSlotAllocator::IsAllocated(const uint32_t slot) const
    {
        if (!used_slots || slot >= capacity)
            return false;

        return used_slots[slot];
    }
}
