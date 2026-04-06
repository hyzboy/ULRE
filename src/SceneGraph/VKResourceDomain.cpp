#include<hgl/vk/VKResourceDomain.h>
#include<hgl/vk/VKMaterial.h>
#include<hgl/type/ActiveMemoryBlockManager.h>

namespace hgl::graph
{
ResourceDomain::ResourceDomain(uint32_t mi_bytes, uint32_t mi_count)
{
    mi_data_bytes = mi_bytes;
    mi_max_count  = mi_count;

    if(mi_data_bytes > 0)
        mi_data_manager = new hgl::ActiveMemoryBlockManager(mi_data_bytes);
}

ResourceDomain::ResourceDomain(Material *mtl)
    : ResourceDomain(mtl ? mtl->GetMIDataBytes() : 0,
                     mtl ? mtl->GetMIMaxCount() : 0)
{
}

ResourceDomain::~ResourceDomain()
{
    delete mi_data_manager;
    mi_data_manager = nullptr;
}

int ResourceDomain::AllocMISlot()
{
    if(!mi_data_manager)
        return -1;

    int mi_id = -1;
    mi_data_manager->GetOrCreate(&mi_id, 1);
    return mi_id;
}

void ResourceDomain::FreeMISlot(int mi_id)
{
    if(mi_id < 0 || !mi_data_manager)
        return;

    mi_data_manager->Release(&mi_id, 1);
}

void *ResourceDomain::GetMIData(int mi_id)
{
    if(!mi_data_manager)
        return nullptr;

    return mi_data_manager->GetData(mi_id);
}

} // namespace hgl::graph
