#include<hgl/vk/VKMaterialResourceDomain.h>
#include<hgl/vk/VKMaterialTemplate.h>
#include<hgl/type/ActiveMemoryBlockManager.h>

hgl::graph::MaterialResourceDomain::MaterialResourceDomain(uint32_t mi_bytes, uint32_t mi_count)
{
    mi_data_bytes = mi_bytes;
    mi_max_count  = mi_count;

    if(mi_data_bytes > 0)
        mi_data_manager = new hgl::ActiveMemoryBlockManager(mi_data_bytes);
}

hgl::graph::MaterialResourceDomain::MaterialResourceDomain(hgl::graph::MaterialTemplate *mtl)
    : MaterialResourceDomain(mtl ? mtl->GetMIDataBytes() : uint32_t(0),
                     mtl ? mtl->GetMIMaxCount() : uint32_t(0))
{
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
