#include<hgl/vk/VKMaterialResourceDomain.h>
#include<hgl/type/ActiveMemoryBlockManager.h>

hgl::graph::MaterialResourceDomain::MaterialResourceDomain(
    hgl::graph::mtl::InstanceDataLayout layout,
    uint32_t max_count,
    uint8_t tex_array_slots)
{
    instance_layout         = layout;
    mi_max_count            = max_count;
    texture_array_slot_flags = tex_array_slots;

    const uint32_t stride = hgl::graph::mtl::GetInstanceDataStride(layout);
    if(stride > 0)
        mi_data_manager = new hgl::ActiveMemoryBlockManager(stride);
}

hgl::graph::MaterialResourceDomain::~MaterialResourceDomain()
{
    delete mi_data_manager;
    mi_data_manager = nullptr;
}

int hgl::graph::MaterialResourceDomain::AllocMISlot()
{
    if(!mi_data_manager)
        return -1;

    int mi_id = -1;
    mi_data_manager->GetOrCreate(&mi_id, 1);
    return mi_id;
}

void hgl::graph::MaterialResourceDomain::FreeMISlot(int mi_id)
{
    if(mi_id < 0 || !mi_data_manager)
        return;

    mi_data_manager->Release(&mi_id, 1);
}

void *hgl::graph::MaterialResourceDomain::GetMIData(int mi_id)
{
    if(!mi_data_manager)
        return nullptr;

    return mi_data_manager->GetData(mi_id);
}
