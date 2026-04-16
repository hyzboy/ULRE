#include <hgl/graph/module/ResourceDomainManager.h>
#include <hgl/vk/VKResourceDomain.h>

namespace hgl::graph
{
GRAPH_MODULE_CONSTRUCT(ResourceDomainManager)
{
}

uint64_t ResourceDomainManager::MakeKey(mtl::ShaderDataSchema schema, uint32_t domain_id)
{
    return (uint64_t(uint32_t(schema)) << 32) | uint64_t(domain_id);
}

void ResourceDomainManager::Release()
{
    for (auto &kv : domain_map)
        delete kv.second;

    domain_map.clear();
    next_domain_id_by_schema.clear();
}

ResourceDomain *ResourceDomainManager::Create(const ResourceDomainCreateInfo &info)
{
    if (info.schema == mtl::ShaderDataSchema::None)
        return nullptr;

    const uint64_t key = MakeKey(info.schema, info.domain_id);
    auto it = domain_map.find(key);
    if (it != domain_map.end())
        return it->second;

    ResourceDomain *domain = new ResourceDomain(info.schema, info.domain_id, info.initial_capacity);
    domain_map[key] = domain;

    uint32_t &next_id = next_domain_id_by_schema[uint32_t(info.schema)];
    if (next_id <= info.domain_id)
        next_id = info.domain_id + 1;

    return domain;
}

ResourceDomain *ResourceDomainManager::CreateAuto(mtl::ShaderDataSchema schema, uint32_t initial_capacity)
{
    if (schema == mtl::ShaderDataSchema::None)
        return nullptr;

    uint32_t &next_id = next_domain_id_by_schema[uint32_t(schema)];
    ResourceDomainCreateInfo info;
    info.schema = schema;
    info.domain_id = next_id++;
    info.initial_capacity = initial_capacity;
    return Create(info);
}

ResourceDomain *ResourceDomainManager::Get(mtl::ShaderDataSchema schema, uint32_t domain_id) const
{
    auto it = domain_map.find(MakeKey(schema, domain_id));
    return it != domain_map.end() ? it->second : nullptr;
}

bool ResourceDomainManager::Destroy(ResourceDomain *domain)
{
    if (!domain)
        return false;

    return Destroy(domain->GetShaderDataSchema(), domain->GetDomainID());
}

bool ResourceDomainManager::Destroy(mtl::ShaderDataSchema schema, uint32_t domain_id)
{
    auto it = domain_map.find(MakeKey(schema, domain_id));
    if (it == domain_map.end())
        return false;

    delete it->second;
    domain_map.erase(it);
    return true;
}
}
