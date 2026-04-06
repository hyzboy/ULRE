#pragma once

#include <memory>
#include <initializer_list>
#include <utility>
#include <hgl/vk/VKFormat.h>
#include <hgl/type/String.h>

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
    Primitive *CreatePrimitive(Geometry *geometry, MaterialInstance *mi) const;
    Primitive *CreatePrimitive(GeometryCreater *creater, MaterialInstance *mi) const;

public:
    template<typename GeometryBuilder>
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

    static Geometry *CreateGeometry(GraphicsContext *graphics_context,
                                    MaterialInstance *material_instance,
                                    const AnsiString &geometry_name,
                                    uint32_t vertex_count,
                                    std::initializer_list<VertexAttribWrite> vertex_writes);

    static Primitive *CreatePrimitive(GraphicsContext *graphics_context,
                                      MaterialInstance *material_instance,
                                      const AnsiString &geometry_name,
                                      uint32_t vertex_count,
                                      std::initializer_list<VertexAttribWrite> vertex_writes);
};
}
