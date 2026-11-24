#include<hgl/vk/VKBindlessSlotAllocator.h>
#include<hgl/common/RenderOptions.h>

using hgl::graph::BindlessSlotAllocator;

int main()
{
    BindlessSlotAllocator max_slots(8192);
    for(uint32_t i=1;i<=8192;++i)
    {
        if(max_slots.Allocate()!=i)
            return 1;
    }
    if(max_slots.Allocate()!=0)
        return 2;
    if(!max_slots.Retire(17,100))
        return 3;
    if(!max_slots.Retire(18,100))
        return 4;

    max_slots.Reclaim(108);
    if(max_slots.Allocate()!=18)
        return 5;
    if(max_slots.Allocate()!=17)
        return 6;

    return 0;
}
