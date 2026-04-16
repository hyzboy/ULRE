#include<hgl/vk/VKResourceDomain.h>
#include<hgl/mtl/ShaderDataSchema.h>
#include<hgl/type/ActiveMemoryBlockManager.h>

namespace hgl::graph
{

ResourceDomain::ResourceDomain(mtl::ShaderDataSchema init_schema, uint32_t init_domain_id, uint32_t init_capacity)
    : schema(init_schema), domain_id(init_domain_id), initial_capacity(init_capacity)
{
    const mtl::ShaderDataSchemaInfo &schema_info = mtl::GetShaderDataSchemaInfo(schema);
    mi_data_bytes = schema_info.byte_size;

    if(mi_data_bytes > 0)
        mi_data_manager = new hgl::ActiveMemoryBlockManager(mi_data_bytes);
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
