#pragma once

#include <memory>
#include <initializer_list>
#include <utility>
#include <hgl/vk/VKFormat.h>
#include <hgl/vk/VKMaterialInstance.h>
#include <hgl/type/String.h>
#include <hgl/graph/geo/GeometryVertexFormat.h>

namespace hgl::graph
{
class GraphicsContext;
class GeometryCreater;
class Geometry;
class Primitive;
class MaterialInstance;
class VertexDataManager;
class GeometryVertexFormat;

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
    std::unique_ptr<GeometryCreater> CreateCreater(const GeometryVertexFormat &gvf) const;
    std::unique_ptr<GeometryCreater> CreateCreater(VertexDataManager *vdm) const;

public:
    Geometry *RegisterGeometry(Geometry *geometry) const;
    Geometry *CreateManagedGeometry(GeometryCreater *creater) const;

public:
    Primitive *CreatePrimitive(Geometry *geometry, MaterialInstance *mi) const;
    Primitive *CreatePrimitive(GeometryCreater *creater, MaterialInstance *mi) const;

public:
    template<typename GeometryBuilder>
    static Primitive *CreatePrimitive(GraphicsContext *graphics_context,
                                      const GeometryVertexFormat &gvf,
                                      MaterialInstance *material_instance,
                                      GeometryBuilder &&builder)
    {
        if(!graphics_context || !material_instance || gvf.GetActiveCount()==0)
            return nullptr;

        GraphicsGeometryFactory geometry_factory(graphics_context);

        auto pc = geometry_factory.CreateCreater(gvf);
        if(!pc)
            return nullptr;

        Geometry *geometry = builder(pc.get());
        if(!geometry)
            return nullptr;

        if(!geometry_factory.RegisterGeometry(geometry))
            return nullptr;

        return geometry_factory.CreatePrimitive(geometry, material_instance);
    }

    static Geometry *CreateGeometry(GraphicsContext *graphics_context,
                                    const GeometryVertexFormat &gvf,
                                    const AnsiString &geometry_name,
                                    uint32_t vertex_count,
                                    std::initializer_list<VertexAttribWrite> vertex_writes);

    static Primitive *CreatePrimitive(GraphicsContext *graphics_context,
                                      const GeometryVertexFormat &gvf,
                                      MaterialInstance *material_instance,
                                      const AnsiString &geometry_name,
                                      uint32_t vertex_count,
                                      std::initializer_list<VertexAttribWrite> vertex_writes);
};
}
