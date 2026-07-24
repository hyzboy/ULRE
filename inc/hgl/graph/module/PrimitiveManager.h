#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/type/ObjectManager.h>

namespace hgl::graph{

using PrimitiveID = int;

// Forward declarations to avoid header ordering issues
class GeometryCreater;
class Geometry;
class Material;
class MaterialInstance;
class DescriptorBindingSet;
class Pipeline;

GRAPH_MODULE_CLASS(PrimitiveManager)
{
private:

    AutoIdObjectManager<PrimitiveID, Primitive> rm_primitive_set;    ///<渲染实例集合集

    PrimitiveManager(GraphicsContext *);
    ~PrimitiveManager() = default;

    friend class GraphModuleManager;

public: // Add/Get/Release

    PrimitiveID Add(Primitive *m) { return rm_primitive_set.Add(m); }
    Primitive *Get(const PrimitiveID &id) { return rm_primitive_set.Get(id); }
    void Release(Primitive *m) { rm_primitive_set.Release(m); }

    void Release() override
    {
        if (rm_primitive_set.GetCount() > 0)
            rm_primitive_set.Clear();
    }

public: // Create

    Primitive *CreatePrimitive(Geometry *r, MaterialInstance *mi, Pipeline *p);
    Primitive *CreatePrimitive(GeometryCreater *pc, MaterialInstance *mi, Pipeline *p);
    Primitive *CreatePrimitive(Geometry *r, Material *material, DescriptorBindingSet *dbs, Pipeline *p);
    Primitive *CreatePrimitive(GeometryCreater *pc, Material *material, DescriptorBindingSet *dbs, Pipeline *p);
};

}//namespace hgl::graph
