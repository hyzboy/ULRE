#include <hgl/graph/geo/GraphicsGeometryFactory.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <hgl/graph/module/GeometryManager.h>
#include <hgl/graph/module/PrimitiveManager.h>
#include <hgl/graph/module/MaterialAssetRegistry.h>
#include <hgl/vk/VKVertexInput.h>
#include <hgl/vk/VKVertexInputConfig.h>
#include <hgl/vk/VKVertexInputLayout.h>
#include <hgl/vk/VertexDataManager.h>
#include <memory>
#include <unordered_map>

namespace hgl::graph
{
namespace
{
static uint32_t ComputeVILHash(const graph::VIL *vil)
{
    if (!vil)
        return 0;

    uint64_t h = 14695981039346656037ULL;
    auto feed = [&](const void *p, size_t n)
    {
        const auto *bytes = reinterpret_cast<const uint8_t*>(p);
        for (size_t i = 0; i < n; ++i)
            h = (h ^ bytes[i]) * 1099511628211ULL;
    };

    const uint32_t count = vil->GetVertexAttribCount();
    feed(&count, sizeof(count));

    const auto *vif = vil->GetVIFList();
    if (vif && count > 0)
        feed(vif, sizeof(graph::VertexInputFormat) * count);

    return static_cast<uint32_t>((h >> 32) ^ (h & 0xFFFFFFFFu));
}

static bool DeriveShaderTypeByFormat(const VkFormat format, VABaseType &base_type, uint8_t &vec_size)
{
    const VulkanFormat *vf = hgl::graph::GetVulkanFormat(format);
    const char *name = hgl::graph::GetVulkanFormatName(format);
    if (!vf || !name)
        return false;

    const char *prefix_end = name;
    while (*prefix_end && *prefix_end != '_')
        ++prefix_end;

    uint8_t channels = 0;
    for (const char *p = name; p < prefix_end; ++p)
    {
        switch (*p)
        {
            case 'R':
            case 'G':
            case 'B':
            case 'A':
                ++channels;
                break;
            default:
                break;
        }
    }

    if (channels == 0 || channels > 4)
        return false;

    vec_size = channels;

    switch (vf->color)
    {
        case VulkanBaseType::UINT:
            base_type = VABaseType::UInt;
            return true;

        case VulkanBaseType::SINT:
            base_type = VABaseType::Int;
            return true;

        default:
            base_type = VABaseType::Float;
            return true;
    }
}

static std::string BuildWritesLayoutKey(const std::initializer_list<GraphicsGeometryFactory::VertexAttribWrite> &vertex_writes)
{
    std::string key;
    key.reserve(vertex_writes.size() * 16);

    for (const auto &write : vertex_writes)
    {
        key += std::to_string(static_cast<int>(write.attrib));
        key += ':';
        key += std::to_string(static_cast<int>(write.format));
        key += ';';
    }

    return key;
}

static const VIL *GetOrCreateSchemaVIL(const std::initializer_list<GraphicsGeometryFactory::VertexAttribWrite> &vertex_writes)
{
    if (vertex_writes.size() == 0)
        return nullptr;

    static std::unordered_map<std::string, std::unique_ptr<VIL>> s_schema_vil_cache;

    const std::string key = BuildWritesLayoutKey(vertex_writes);
    const auto it = s_schema_vil_cache.find(key);
    if (it != s_schema_vil_cache.end())
        return it->second.get();

    VIAArray via_array;
    via_array.Init(static_cast<uint>(vertex_writes.size()));

    uint idx = 0;
    for (const auto &write : vertex_writes)
    {
        VABaseType base_type;
        uint8_t vec_size = 0;
        if (!DeriveShaderTypeByFormat(write.format, base_type, vec_size))
            return nullptr;

        auto &via = via_array.items[idx];
        via.attrib = write.attrib;
        via.location = static_cast<uint8_t>(idx);
        via.basetype = static_cast<uint8_t>(base_type);
        via.vec_size = vec_size;
        via.storage_format = write.format;
        via.interpolation = Interpolation::Smooth;
        ++idx;
    }

    VertexInputConfig config(via_array);
    auto vil = std::unique_ptr<VIL>(config.CreateVIL(nullptr));
    if (!vil)
        return nullptr;

    const VIL *result = vil.get();
    s_schema_vil_cache.emplace(key, std::move(vil));
    return result;
}
}

GraphicsGeometryFactory::GraphicsGeometryFactory(GraphicsContext *gc)
    : graphics(gc)
{
}

std::unique_ptr<GeometryCreater> GraphicsGeometryFactory::CreateCreater(const VIL *vil) const
{
    if(!graphics || !vil)
        return nullptr;

    auto *device = graphics->GetDevice();
    auto *buffer_manager = graphics->GetBufferManager();

    if(!device || !buffer_manager)
        return nullptr;

    return std::make_unique<GeometryCreater>(device, MakeGeometryVertexFormatMap(vil), buffer_manager);
}

std::unique_ptr<GeometryCreater> GraphicsGeometryFactory::CreateCreater(VertexDataManager *vdm) const
{
    if(!graphics || !vdm)
        return nullptr;

    return std::make_unique<GeometryCreater>(vdm);
}

Geometry *GraphicsGeometryFactory::RegisterGeometry(Geometry *geometry) const
{
    if(!graphics || !geometry)
        return nullptr;

    auto *geometry_manager = graphics->GetGeometryManager();
    if(!geometry_manager)
        return nullptr;

    geometry_manager->Add(geometry);
    return geometry;
}

Geometry *GraphicsGeometryFactory::CreateManagedGeometry(GeometryCreater *creater) const
{
    if(!creater)
        return nullptr;

    return RegisterGeometry(creater->Create());
}

Primitive *GraphicsGeometryFactory::CreatePrimitive(Geometry *geometry, const PrimitiveMaterialSlot &slot) const
{
    if(!graphics || !geometry || !slot.IsValid())
        return nullptr;

    auto *primitive_manager = graphics->GetPrimitiveManager();
    if(!primitive_manager)
        return nullptr;

    return primitive_manager->CreatePrimitive(geometry, slot);
}

Primitive *GraphicsGeometryFactory::CreatePrimitive(GeometryCreater *creater, const PrimitiveMaterialSlot &slot) const
{
    if(!graphics || !creater || !slot.IsValid())
        return nullptr;

    auto *geometry = CreateManagedGeometry(creater);
    if(!geometry)
        return nullptr;

    auto *primitive_manager = graphics->GetPrimitiveManager();
    if(!primitive_manager)
        return nullptr;

    return primitive_manager->CreatePrimitive(geometry, slot);
}

Primitive *GraphicsGeometryFactory::CreatePrimitive(Geometry *geometry, SemanticMaterialId semantic_id) const
{
    if(!graphics || !geometry || semantic_id == 0)
        return nullptr;

    auto *primitive_manager = graphics->GetPrimitiveManager();
    if(!primitive_manager)
        return nullptr;

    return primitive_manager->CreatePrimitive(geometry, semantic_id);
}

Primitive *GraphicsGeometryFactory::CreatePrimitive(GeometryCreater *creater, SemanticMaterialId semantic_id) const
{
    if(!graphics || !creater || semantic_id == 0)
        return nullptr;

    auto *geometry = CreateManagedGeometry(creater);
    if(!geometry)
        return nullptr;

    return CreatePrimitive(geometry, semantic_id);
}

Geometry *GraphicsGeometryFactory::CreateGeometry(GraphicsContext *graphics_context,
                                                  const VIL *vil,
                                                  const AnsiString &geometry_name,
                                                  uint32_t vertex_count,
                                                  std::initializer_list<VertexAttribWrite> vertex_writes)
{
    if(!graphics_context || !vil || geometry_name.IsEmpty() || vertex_count == 0)
        return nullptr;

    GraphicsGeometryFactory geometry_factory(graphics_context);

    auto pc = geometry_factory.CreateCreater(vil);
    if(!pc)
        return nullptr;

    if(!pc->Init(geometry_name, vertex_count))
        return nullptr;

    for(const auto &write : vertex_writes)
    {
        if(!write.data)
            return nullptr;

        if(!pc->WriteVAB(write.attrib, write.format, write.data))
            return nullptr;
    }

    return geometry_factory.CreateManagedGeometry(pc.get());
}

Geometry *GraphicsGeometryFactory::CreateGeometry(GraphicsContext *graphics_context,
                                                  const AnsiString &geometry_name,
                                                  uint32_t vertex_count,
                                                  std::initializer_list<VertexAttribWrite> vertex_writes)
{
    const VIL *vil = GetOrCreateSchemaVIL(vertex_writes);
    if (!vil)
        return nullptr;

    return CreateGeometry(graphics_context,
                          vil,
                          geometry_name,
                          vertex_count,
                          vertex_writes);
}

}
