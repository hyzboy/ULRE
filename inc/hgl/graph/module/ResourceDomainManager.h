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
    uint32_t ssbo_id = 0;
    DeviceBuffer *buffer = nullptr;
    uint32_t element_capacity = 0;
    uint32_t element_stride = 0;
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

    static uint64_t MakeKey(const mtl::SSBOAddress &address) noexcept;
    ResourceDomainBinding *FindMutable(const mtl::SSBOAddress &address);
    const ResourceDomainBinding *Find(const mtl::SSBOAddress &address) const;

public:

    void Release() override;

    bool Touch(const mtl::SSBOAddress &address);

    bool RegisterBuffer(const mtl::SSBOAddress &address, DeviceBuffer *buffer, uint32_t element_capacity = 0);

    DeviceBuffer *EnsureBuffer(const mtl::SSBOAddress &address,
                               const AnsiString &name,
                               VkDeviceSize byte_size,
                               uint32_t required_capacity,
                               SharingMode sm = SharingMode::Exclusive);

    bool ClearDomain(const mtl::SSBOAddress &address);

    bool HasBinding(const mtl::SSBOAddress &address) const;
    bool TryGetBinding(const mtl::SSBOAddress &address, ResourceDomainBinding &out_binding) const;

    DeviceBuffer *GetBuffer(const mtl::SSBOAddress &address) const;

    const IGPUBuffer *GetGPUBuffer(const mtl::SSBOAddress &address) const;

    uint32_t GetElementCapacity(const mtl::SSBOAddress &address) const;

    uint32_t GetCount() const { return static_cast<uint32_t>(domain_map.size()); }
};
} // namespace hgl::graph
