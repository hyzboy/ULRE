#pragma once

#include<hgl/type/ValueArray.h>
#include<hgl/common/RenderOptions.h>
#include<cstdint>

namespace hgl::graph
{
    class BindlessSlotAllocator
    {
    public:
        struct RetiringSlot
        {
            uint32_t handle=0;
            uint64_t retire_frame=0;
        };

        enum class State:uint8_t
        {
            Empty,
            Reserved,
            Live,
            Retiring
        };

    private:
        uint32_t max_slots_=8192;
        uint32_t high_water_mark_=0;
        hgl::ValueArray<uint32_t> free_slots_;
        hgl::ValueArray<RetiringSlot> retiring_slots_;
        hgl::ValueArray<State> slot_states_;

    public:
        explicit BindlessSlotAllocator(uint32_t max_slots=8192)
            : max_slots_(max_slots)
        {
            if(max_slots_==0)
                max_slots_=1;
            slot_states_.Resize(static_cast<int>(max_slots_));
            for(int i=0;i<slot_states_.GetCount();++i)
                slot_states_[i]=State::Empty;
        }

        uint32_t Allocate()
        {
            if(!free_slots_.IsEmpty())
            {
                const uint32_t handle=*free_slots_.last();
                free_slots_.Delete(free_slots_.GetCount()-1);
                slot_states_[static_cast<int>(handle-1)]=State::Reserved;
                return handle;
            }

            if(high_water_mark_>=max_slots_)
                return 0;

            ++high_water_mark_;
            slot_states_[static_cast<int>(high_water_mark_-1)]=State::Reserved;
            return high_water_mark_;
        }

        bool MarkLive(uint32_t handle)
        {
            if(handle==0||handle>high_water_mark_) return false;
            slot_states_[static_cast<int>(handle-1)]=State::Live;
            return true;
        }

        bool Retire(uint32_t handle,uint64_t current_frame)
        {
            if(handle==0||handle>high_water_mark_) return false;
            slot_states_[static_cast<int>(handle-1)]=State::Retiring;
            RetiringSlot item;
            item.handle=handle;
            item.retire_frame=current_frame+HGL_FRAME_SLOT_TOTAL;
            retiring_slots_.Add(item);
            return true;
        }

        uint32_t Reclaim(uint64_t current_frame)
        {
            uint32_t reclaimed=0;
            hgl::ValueArray<RetiringSlot> remain;
            for(int i=0;i<retiring_slots_.GetCount();++i)
            {
                RetiringSlot item=retiring_slots_[i];
                if(current_frame>=item.retire_frame)
                {
                    if(item.handle!=0&&item.handle<=high_water_mark_)
                    {
                        slot_states_[static_cast<int>(item.handle-1)]=State::Empty;
                        free_slots_.Add(item.handle);
                        ++reclaimed;
                    }
                }
                else
                {
                    remain.Add(item);
                }
            }
            retiring_slots_=remain;
            return reclaimed;
        }

        uint32_t GetHighWaterMark() const noexcept {return high_water_mark_;}
        uint32_t GetMaxSlots() const noexcept {return max_slots_;}
    };
}
