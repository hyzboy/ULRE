#include <hgl/graph/geo/GraphicsGeometryFactory.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <hgl/graph/module/GeometryManager.h>
#include <hgl/graph/module/PrimitiveManager.h>
#include <hgl/graph/module/MaterialAssetRegistry.h>
#include <hgl/vk/VKMaterialInstance.h>
#include <hgl/vk/VKVertexInputConfig.h>
#include <hgl/vk/VKVertexInputLayout.h>
#include <hgl/vk/VertexDataManager.h>

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

    return std::make_unique<GeometryCreater>(device, vil, buffer_manager);
}

std::unique_ptr<GeometryCreater> GraphicsGeometryFactory::CreateCreater(VertexDataManager *vdm) const
{
    if(!graphics || !vdm)
        return nullptr;

    return std::make_unique<GeometryCreater>(vdm);
}

std::unique_ptr<GeometryCreater> GraphicsGeometryFactory::CreateCreater(MaterialInstance *mi) const
{
    if(!mi)
        return nullptr;

    return CreateCreater(mi->GetVIL());
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

Primitive *GraphicsGeometryFactory::CreatePrimitive(Geometry *geometry, MaterialInstance *mi) const
{
    if(!graphics || !geometry || !mi)
        return nullptr;

    auto *primitive_manager = graphics->GetPrimitiveManager();
    if(!primitive_manager)
        return nullptr;

    return primitive_manager->CreatePrimitive(geometry, mi);
}

Primitive *GraphicsGeometryFactory::CreatePrimitive(GeometryCreater *creater, MaterialInstance *mi) const
{
    if(!graphics || !creater || !mi)
        return nullptr;

    auto *geometry = CreateManagedGeometry(creater);
    if(!geometry)
        return nullptr;

    auto *primitive_manager = graphics->GetPrimitiveManager();
    if(!primitive_manager)
        return nullptr;

    return primitive_manager->CreatePrimitive(geometry, mi);
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
                                                  MaterialInstance *material_instance,
                                                  const AnsiString &geometry_name,
                                                  uint32_t vertex_count,
                                                  std::initializer_list<VertexAttribWrite> vertex_writes)
{
    if(!graphics_context || !material_instance || geometry_name.IsEmpty() || vertex_count == 0)
        return nullptr;

    GraphicsGeometryFactory geometry_factory(graphics_context);

    auto pc = geometry_factory.CreateCreater(material_instance);
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

Primitive *GraphicsGeometryFactory::CreatePrimitive(GraphicsContext *graphics_context,
                                                    MaterialInstance *material_instance,
                                                    const AnsiString &geometry_name,
                                                    uint32_t vertex_count,
                                                    std::initializer_list<VertexAttribWrite> vertex_writes)
{
    auto *geometry = CreateGeometry(graphics_context, material_instance, geometry_name, vertex_count, vertex_writes);
    if(!geometry)
        return nullptr;

    GraphicsGeometryFactory geometry_factory(graphics_context);
    return geometry_factory.CreatePrimitive(geometry, material_instance);
}

Primitive *GraphicsGeometryFactory::CreatePrimitive(GraphicsContext *graphics_context,
                                                    SemanticMaterialId semantic_id,
                                                    const AnsiString &geometry_name,
                                                    uint32_t vertex_count,
                                                    std::initializer_list<VertexAttribWrite> vertex_writes)
{
    if(!graphics_context || semantic_id == 0 || geometry_name.IsEmpty() || vertex_count == 0)
        return nullptr;

    auto *registry = graphics_context->GetMaterialAssetRegistry();
    if(!registry)
        return nullptr;

    mtl::MaterialAssetRecord rec;
    if(!registry->QuerySemanticMaterial(semantic_id, rec))
        return nullptr;

    MaterialDomainHandle handle = registry->Acquire(rec);
    if(!handle.material)
        return nullptr;

    VILConfig vil_cfg;
    for(const auto &write : vertex_writes)
    {
        if(!write.data)
            return nullptr;

        if(!vil_cfg.Add(write.attrib, VAConfig{write.format}))
            return nullptr;
    }

    const VIL *vil = handle.material->CreateVIL(&vil_cfg);
    if(!vil)
        vil = handle.material->GetDefaultVIL();

    if(!vil)
        return nullptr;

    const uint32_t vil_hash = ComputeVILHash(vil);

    GraphicsGeometryFactory geometry_factory(graphics_context);

    auto pc = geometry_factory.CreateCreater(vil);
    if(!pc)
        return nullptr;

    if(!pc->Init(geometry_name, vertex_count))
        return nullptr;

    for(const auto &write : vertex_writes)
    {
        if(!pc->WriteVAB(write.attrib, write.format, write.data))
            return nullptr;
    }

    auto *geometry = geometry_factory.CreateManagedGeometry(pc.get());
    if(!geometry)
        return nullptr;

    Primitive *primitive = DirectCreatePrimitive(geometry, semantic_id, vil_hash);
    if(!primitive)
        return nullptr;

    auto *primitive_manager = graphics_context->GetPrimitiveManager();
    if(!primitive_manager)
    {
        delete primitive;
        return nullptr;
    }

    primitive_manager->Add(primitive);
    return primitive;
}
}
