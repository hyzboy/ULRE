#include <hgl/graph/geo/GraphicsGeometryFactory.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <hgl/graph/module/GeometryManager.h>
#include <hgl/graph/module/PrimitiveManager.h>
#include <hgl/graph/module/MaterialRecipeRegistry.h>
#include <hgl/vk/VKMaterialInstance.h>
#include <hgl/vk/VertexDataManager.h>

namespace hgl::graph
{
GraphicsGeometryFactory::GraphicsGeometryFactory(GraphicsContext *gc)
    : graphics(gc)
{
}

std::unique_ptr<GeometryCreater> GraphicsGeometryFactory::CreateCreater(const GeometryVertexFormat &gvf) const
{
    if(!graphics || gvf.GetActiveCount()==0)
        return nullptr;

    auto *device = graphics->GetDevice();
    auto *buffer_manager = graphics->GetBufferManager();

    if(!device || !buffer_manager)
        return nullptr;

    return std::make_unique<GeometryCreater>(device, gvf, buffer_manager);
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

Geometry *GraphicsGeometryFactory::CreateGeometry(GraphicsContext *graphics_context,
                                                  const GeometryVertexFormat &gvf,
                                                  const AnsiString &geometry_name,
                                                  uint32_t vertex_count,
                                                  std::initializer_list<VertexAttribWrite> vertex_writes)
{
    if(!graphics_context || gvf.GetActiveCount()==0 || geometry_name.IsEmpty() || vertex_count == 0)
        return nullptr;

    GraphicsGeometryFactory geometry_factory(graphics_context);

    auto pc = geometry_factory.CreateCreater(gvf);
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
                                                    const GeometryVertexFormat &gvf,
                                                    MaterialInstance *material_instance,
                                                    const AnsiString &geometry_name,
                                                    uint32_t vertex_count,
                                                    std::initializer_list<VertexAttribWrite> vertex_writes)
{
    if(!graphics_context || !material_instance || gvf.GetActiveCount()==0)
        return nullptr;

    auto *geometry = CreateGeometry(graphics_context, gvf, geometry_name, vertex_count, vertex_writes);
    if(!geometry)
        return nullptr;

    GraphicsGeometryFactory geometry_factory(graphics_context);
    return geometry_factory.CreatePrimitive(geometry, material_instance);
}

Primitive *GraphicsGeometryFactory::CreatePrimitive(GraphicsContext *graphics_context,
                                                    const mtl::MaterialRecipe &rec,
                                                    const AnsiString &geometry_name,
                                                    uint32_t vertex_count,
                                                    std::initializer_list<VertexAttribWrite> vertex_writes,
                                                    const void *instance_data,
                                                    uint32_t instance_data_size)
{
    if(!graphics_context || geometry_name.IsEmpty() || vertex_count == 0)
        return nullptr;

    // 从 vertex_writes 推算 GVF
    GeometryVertexFormat gvf;
    for(const auto &w : vertex_writes)
    {
        if(!gvf.Set(w.attrib, w.format))
            return nullptr;
    }

    if(gvf.GetActiveCount() == 0)
        return nullptr;

    // 创建 Geometry
    auto *geometry = CreateGeometry(graphics_context, gvf, geometry_name, vertex_count, vertex_writes);
    if(!geometry)
        return nullptr;

    // 从 GVF 自动推算 MI
    auto *registry = graphics_context->GetMaterialAssetRegistry();
    if(!registry)
        return nullptr;

    auto *mi = registry->AcquireMI(rec, gvf, instance_data, instance_data_size);
    if(!mi)
        return nullptr;

    GraphicsGeometryFactory geometry_factory(graphics_context);
    return geometry_factory.CreatePrimitive(geometry, mi);
}
}
