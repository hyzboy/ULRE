#pragma once

#include <memory>
#include <initializer_list>
#include <utility>
#include <hgl/vk/VKFormat.h>
#include <hgl/type/String.h>
#include <hgl/vk/VKMaterialTemplate.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/geo/GeometryCreater.h>
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
    [[deprecated("VIL-first geometry creation is deprecated. Prefer schema-first CreateGeometry(...) overloads or CreateCreater(VertexDataManager*).")]]
    std::unique_ptr<GeometryCreater> CreateCreater(const VIL *vil) const;
    std::unique_ptr<GeometryCreater> CreateCreater(VertexDataManager *vdm) const;

public:
    Geometry *RegisterGeometry(Geometry *geometry) const;
    Geometry *CreateManagedGeometry(GeometryCreater *creater) const;

public:
    Primitive *CreatePrimitive(Geometry *geometry, const PrimitiveMaterialSlot &slot) const;
    Primitive *CreatePrimitive(GeometryCreater *creater, const PrimitiveMaterialSlot &slot) const;

    Primitive *CreatePrimitive(Geometry *geometry, SemanticMaterialId semantic_id) const;
    Primitive *CreatePrimitive(GeometryCreater *creater, SemanticMaterialId semantic_id) const;

public:
    template<typename GeometryBuilder>
    static Primitive *CreatePrimitive(GraphicsContext *graphics_context,
                                      const PrimitiveMaterialSlot &slot,
                                      GeometryBuilder &&builder)
    {
        if(!graphics_context || !slot.IsValid() || !slot.vil)
            return nullptr;

        GraphicsGeometryFactory geometry_factory(graphics_context);

        auto pc = geometry_factory.CreateCreater(slot.vil);
        if(!pc)
            return nullptr;

        Geometry *geometry = builder(pc.get());
        if(!geometry)
            return nullptr;

        if(!geometry_factory.RegisterGeometry(geometry))
            return nullptr;

        return geometry_factory.CreatePrimitive(geometry, slot);
    }

    template<typename GeometryBuilder>
    [[deprecated("Implicit VIL-first semantic CreatePrimitive is deprecated. Prefer schema-first geometry creation + CreatePrimitive(geometry, semantic_id).")]]
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

        auto *device = graphics_context->GetDevice();
        auto *buffer_manager = graphics_context->GetBufferManager();
        if(!device || !buffer_manager)
            return nullptr;

        VertexFormatMap format_map;
        const uint32_t attr_count = vil->GetVertexAttribCount();
        for(uint32_t i = 0; i < attr_count; ++i)
        {
            const auto *cfg = vil->GetConfig(i);
            if(!cfg)
                continue;

            format_map[cfg->attrib] = cfg->format;
        }

        auto pc = std::make_unique<GeometryCreater>(device, format_map, buffer_manager);
        if(!pc)
            return nullptr;

        Geometry *geometry = builder(pc.get());
        if(!geometry)
            return nullptr;

        if(!geometry_factory.RegisterGeometry(geometry))
            return nullptr;

        return geometry_factory.CreatePrimitive(geometry, semantic_id);
    }

    [[deprecated("VIL-first CreateGeometry is deprecated. Use schema-first CreateGeometry(graphics_context, geometry_name, vertex_count, vertex_writes).")]]
    static Geometry *CreateGeometry(GraphicsContext *graphics_context,
                                    const VIL *vil,
                                    const AnsiString &geometry_name,
                                    uint32_t vertex_count,
                                    std::initializer_list<VertexAttribWrite> vertex_writes);

    static Geometry *CreateGeometry(GraphicsContext *graphics_context,
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
