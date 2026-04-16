#pragma once

#include <hgl/graph/module/GraphModule.h>
#include <hgl/mtl/ShaderDataSchema.h>
#include <cstdint>
#include <unordered_map>

namespace hgl::graph
{
class ResourceDomain;

struct ResourceDomainCreateInfo
{
    mtl::ShaderDataSchema schema = mtl::ShaderDataSchema::None;
    uint32_t domain_id = 0;
    uint32_t initial_capacity = 256;
};

GRAPH_MODULE_CLASS(ResourceDomainManager)
{
private:

    std::unordered_map<uint64_t, ResourceDomain *> domain_map;
    std::unordered_map<uint32_t, uint32_t> next_domain_id_by_schema;

private:

    ResourceDomainManager(GraphicsContext *);
    ~ResourceDomainManager() = default;

    friend class GraphModuleManager;

private:

    static uint64_t MakeKey(mtl::ShaderDataSchema schema, uint32_t domain_id);

public:

    void Release() override;

    ResourceDomain *Create(const ResourceDomainCreateInfo &info);
    ResourceDomain *CreateAuto(mtl::ShaderDataSchema schema, uint32_t initial_capacity = 256);
    ResourceDomain *Get(mtl::ShaderDataSchema schema, uint32_t domain_id) const;

    bool Destroy(ResourceDomain *domain);
    bool Destroy(mtl::ShaderDataSchema schema, uint32_t domain_id);

    uint32_t GetCount() const { return static_cast<uint32_t>(domain_map.size()); }
};
}
