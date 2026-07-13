#pragma once

#include <hgl/graph/module/GraphModule.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <unordered_map>

namespace hgl::graph
{
class DeviceBuffer;
class IGPUBuffer;

struct ResourceDomainBinding
{
    mtl::SSBOType ssbo_type = mtl::SSBOType::UserDefined;
    uint32_t resource_domain_id = 0;
    DeviceBuffer *buffer = nullptr;
    uint32_t element_capacity = 0;
};

GRAPH_MODULE_CLASS(ResourceDomainManager)
{
private:

    std::unordered_map<uint64_t, ResourceDomainBinding> domain_map;

private:

    ResourceDomainManager(GraphicsContext *);
    ~ResourceDomainManager() = default;

    friend class GraphModuleManager;

private:

    static uint64_t MakeKey(mtl::SSBOType ssbo_type, uint32_t resource_domain_id) noexcept;
    ResourceDomainBinding *FindMutable(mtl::SSBOType ssbo_type, uint32_t resource_domain_id);
    const ResourceDomainBinding *Find(mtl::SSBOType ssbo_type, uint32_t resource_domain_id) const;

public:

    void Release() override;

    bool Touch(mtl::SSBOType ssbo_type, uint32_t resource_domain_id);
    bool RegisterBuffer(mtl::SSBOType ssbo_type, uint32_t resource_domain_id, DeviceBuffer *buffer, uint32_t element_capacity = 0);
    DeviceBuffer *EnsureBuffer(mtl::SSBOType ssbo_type,
                               uint32_t resource_domain_id,
                               const AnsiString &name,
                               VkDeviceSize byte_size,
                               uint32_t required_capacity,
                               SharingMode sm = SharingMode::Exclusive);

    bool ClearDomain(mtl::SSBOType ssbo_type, uint32_t resource_domain_id);

    DeviceBuffer *GetBuffer(mtl::SSBOType ssbo_type, uint32_t resource_domain_id) const;
    const IGPUBuffer *GetGPUBuffer(mtl::SSBOType ssbo_type, uint32_t resource_domain_id) const;
    uint32_t GetElementCapacity(mtl::SSBOType ssbo_type, uint32_t resource_domain_id) const;

    uint32_t GetCount() const { return static_cast<uint32_t>(domain_map.size()); }
};
} // namespace hgl::graph

