#pragma once

#include <memory>
#include <initializer_list>
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
