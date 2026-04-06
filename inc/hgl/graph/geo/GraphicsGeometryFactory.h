#pragma once

#include <memory>
#include <initializer_list>
#include <utility>
#include <hgl/vk/VKFormat.h>
#include <hgl/type/String.h>
#include <hgl/vk/VKShaderProgram.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/RuntimeMaterialRequest.h>
#include <hgl/graph/module/MaterialDomainHandle.h>
#include <hgl/graph/module/MaterialAssetRegistry.h>
#include <hgl/mtl/MaterialAssetRecord.h>

namespace hgl::graph
{
class GraphicsContext;
class GeometryCreater;
class Geometry;
class Primitive;
class MaterialInstance;
class VertexDataManager;
class VertexInputLayout;
using VIL=VertexInputLayout;

class GraphicsGeometryFactory
{
public:
    struct VertexAttribWrite
    {
        VertexAttrib attrib;
        VkFormat format;
        const void *data;
    };

private:
    GraphicsContext *graphics = nullptr;

public:
    explicit GraphicsGeometryFactory(GraphicsContext *gc);

    GraphicsContext *GetGraphicsContext() const { return graphics; }

public:
    std::unique_ptr<GeometryCreater> CreateCreater(const VIL *vil) const;
    std::unique_ptr<GeometryCreater> CreateCreater(VertexDataManager *vdm) const;
    std::unique_ptr<GeometryCreater> CreateCreater(MaterialInstance *mi) const;

public:
    Geometry *RegisterGeometry(Geometry *geometry) const;
    Geometry *CreateManagedGeometry(GeometryCreater *creater) const;

public:
    [[deprecated("Use SemanticMaterialId overload; direct MI binding will be removed in a future cleanup phase.")]]
    Primitive *CreatePrimitive(Geometry *geometry, MaterialInstance *mi) const;
    [[deprecated("Use SemanticMaterialId overload; direct MI binding will be removed in a future cleanup phase.")]]
    Primitive *CreatePrimitive(GeometryCreater *creater, MaterialInstance *mi) const;

    Primitive *CreatePrimitive(Geometry *geometry, SemanticMaterialId semantic_id) const;
    Primitive *CreatePrimitive(GeometryCreater *creater, SemanticMaterialId semantic_id) const;

public:
    template<typename GeometryBuilder>
    [[deprecated("Use SemanticMaterialId overload; direct MI binding will be removed in a future cleanup phase.")]]
    static Primitive *CreatePrimitive(GraphicsContext *graphics_context,
                                      MaterialInstance *material_instance,
                                      GeometryBuilder &&builder)
    {
        if(!graphics_context || !material_instance)
            return nullptr;

        GraphicsGeometryFactory geometry_factory(graphics_context);

        auto pc = geometry_factory.CreateCreater(material_instance);
        if(!pc)
            return nullptr;

        Geometry *geometry = builder(pc.get());
        if(!geometry)
            return nullptr;

        if(!geometry_factory.RegisterGeometry(geometry))
            return nullptr;

        return geometry_factory.CreatePrimitive(geometry, material_instance);
    }

    template<typename GeometryBuilder>
    static Primitive *CreatePrimitive(GraphicsContext *graphics_context,
                                      SemanticMaterialId semantic_id,
                                      GeometryBuilder &&builder)
    {
        if(!graphics_context || semantic_id == 0)
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

        const VIL *vil = handle.material->GetDefaultVIL();
        if(!vil)
            return nullptr;

        GraphicsGeometryFactory geometry_factory(graphics_context);

        auto pc = geometry_factory.CreateCreater(vil);
        if(!pc)
            return nullptr;

        Geometry *geometry = builder(pc.get());
        if(!geometry)
            return nullptr;

        if(!geometry_factory.RegisterGeometry(geometry))
            return nullptr;

        return geometry_factory.CreatePrimitive(geometry, semantic_id);
    }

    static Geometry *CreateGeometry(GraphicsContext *graphics_context,
                                    MaterialInstance *material_instance,
                                    const AnsiString &geometry_name,
                                    uint32_t vertex_count,
                                    std::initializer_list<VertexAttribWrite> vertex_writes);

    [[deprecated("Use SemanticMaterialId overload; direct MI binding will be removed in a future cleanup phase.")]]
    static Primitive *CreatePrimitive(GraphicsContext *graphics_context,
                                      MaterialInstance *material_instance,
                                      const AnsiString &geometry_name,
                                      uint32_t vertex_count,
                                      std::initializer_list<VertexAttribWrite> vertex_writes);

    static Primitive *CreatePrimitive(GraphicsContext *graphics_context,
                                      SemanticMaterialId semantic_id,
                                      const AnsiString &geometry_name,
                                      uint32_t vertex_count,
                                      std::initializer_list<VertexAttribWrite> vertex_writes);
};
}
