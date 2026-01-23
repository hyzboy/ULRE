#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/type/ObjectManager.h>

VK_NAMESPACE_BEGIN

using PrimitiveID = int;

// Forward declarations to avoid header ordering issues
class GeometryCreater;
class Geometry;
class MaterialInstance;
class Pipeline;

GRAPH_MODULE_CLASS(PrimitiveManager)
{
private:

    AutoIdObjectManager<PrimitiveID, Primitive> rm_primitive_set;    ///<渲染实例集合集

    PrimitiveManager(RenderFramework *);
    ~PrimitiveManager() = default;

    friend class GraphModuleManager;

public: // Add/Get/Release

    PrimitiveID Add(Primitive *m) { return rm_primitive_set.Add(m); }
    Primitive *Get(const PrimitiveID &id) { return rm_primitive_set.Get(id); }
    void Release(Primitive *m) { rm_primitive_set.Release(m); }

public: // Create

    Primitive *CreatePrimitive(Geometry *r, MaterialInstance *mi, Pipeline *p);
    Primitive *CreatePrimitive(GeometryCreater *pc, MaterialInstance *mi, Pipeline *p);
};

VK_NAMESPACE_END
