#pragma once

#include <hgl/ecs/support/transform/TransformID.h>
#include <hgl/type/ValueArray.h>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    class TransformSlotTable
    {
        ValueArray<TransformID> static_slots;
        ValueArray<TransformID> movable_slots;
        ValueArray<uint32_t> static_free_slots;
        ValueArray<uint32_t> movable_free_slots;

        ValueArray<TransformID> &MutableSlots(bool movable)
        {
            return movable ? movable_slots : static_slots;
        }

    public:
        const ValueArray<TransformID> &Slots(bool movable) const
        {
            return movable ? movable_slots : static_slots;
        }

        uint32_t Acquire(bool movable, uint32_t budget)
        {
            auto &free_slots = movable ? movable_free_slots : static_free_slots;
            auto &slots = MutableSlots(movable);
            if (!free_slots.IsEmpty())
            {
                const int last = free_slots.GetCount() - 1;
                const uint32_t slot = free_slots[last];
                free_slots.Delete(last);
                return slot;
            }
            if (budget != 0 && static_cast<uint32_t>(slots.GetCount()) >= budget)
                return INVALID_TRANSFORM_SLOT;
            return static_cast<uint32_t>(slots.Add(INVALID_TRANSFORM_ID));
        }

        bool Assign(bool movable, uint32_t slot, TransformID id)
        {
            auto &slots = MutableSlots(movable);
            if (slot >= static_cast<uint32_t>(slots.GetCount()) || !IsValidTransformID(id))
            {
                GLogError(u8"[TransformSlotTable] invalid assignment: movable=%u slot=%u id=%u",
                          movable ? 1u : 0u, slot, id);
                return false;
            }
            slots[slot] = id;
            return true;
        }

        void Release(bool movable, uint32_t slot)
        {
            auto &slots = MutableSlots(movable);
            if (slot >= static_cast<uint32_t>(slots.GetCount()) || !IsValidTransformID(slots[slot]))
                return;
            slots[slot] = INVALID_TRANSFORM_ID;
            (movable ? movable_free_slots : static_free_slots).Add(slot);
        }

        bool Find(TransformID id, bool movable, uint32_t &out_slot) const
        {
            if (!IsValidTransformID(id))
                return false;
            const auto &slots = Slots(movable);
            for (int slot = 0; slot < slots.GetCount(); ++slot)
                if (slots[slot] == id)
                {
                    out_slot = static_cast<uint32_t>(slot);
                    return true;
                }
            return false;
        }

        uint32_t LiveCount(bool movable) const
        {
            uint32_t count = 0;
            for (TransformID id : Slots(movable))
                if (IsValidTransformID(id))
                    ++count;
            return count;
        }

        void Clear()
        {
            static_slots.Clear();
            movable_slots.Clear();
            static_free_slots.Clear();
            movable_free_slots.Clear();
        }
    };
}
