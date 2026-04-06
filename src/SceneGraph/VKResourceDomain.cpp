#include<hgl/vk/VKResourceDomain.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/type/ActiveMemoryBlockManager.h>

hgl::graph::ResourceDomain::ResourceDomain(uint32_t mi_bytes, uint32_t mi_count)
{
    mi_data_bytes = mi_bytes;
    mi_max_count  = mi_count;

    if(mi_data_bytes > 0)
        mi_data_manager = new hgl::ActiveMemoryBlockManager(mi_data_bytes);
}

hgl::graph::ResourceDomain::ResourceDomain(hgl::graph::ShaderProgram *mtl)
    : ResourceDomain(mtl ? mtl->GetMIDataBytes() : uint32_t(0),
                     mtl ? mtl->GetMIMaxCount() : uint32_t(0))
{
}

hgl::graph::ResourceDomain::~ResourceDomain()
{
    delete mi_data_manager;
    mi_data_manager = nullptr;
}

int hgl::graph::ResourceDomain::AllocMISlot()
{
    if(!mi_data_manager)
        return -1;

    int mi_id = -1;
    mi_data_manager->GetOrCreate(&mi_id, 1);
    return mi_id;
}

void hgl::graph::ResourceDomain::FreeMISlot(int mi_id)
{
    if(mi_id < 0 || !mi_data_manager)
        return;

    mi_data_manager->Release(&mi_id, 1);
}

void *hgl::graph::ResourceDomain::GetMIData(int mi_id)
{
    if(!mi_data_manager)
        return nullptr;

    return mi_data_manager->GetData(mi_id);
}
