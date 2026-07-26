#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/type/ObjectManager.h>

namespace hgl::graph{

using PrimitiveID = int;

// Forward declarations to avoid header ordering issues
class PrimitiveAsset;
class GeometryCreater;
class Geometry;
class MaterialProgram;
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

    Primitive *CreatePrimitive(Geometry *r, MaterialInstance *mi, Pipeline *p=nullptr);
    Primitive *CreatePrimitive(GeometryCreater *pc, MaterialInstance *mi, Pipeline *p=nullptr);
    Primitive *CreatePrimitive(Geometry *r, MaterialProgram *material, DescriptorBindingSet *dbs, Pipeline *p=nullptr);
    Primitive *CreatePrimitive(GeometryCreater *pc, MaterialProgram *material, DescriptorBindingSet *dbs, Pipeline *p=nullptr);

public: // Runtime draw-unit creation (new naming, Phase 7 bridge)

    Primitive *CreateRuntimePrimitive(Geometry *r, MaterialProgram *material, Pipeline *p=nullptr);
    Primitive *CreateRuntimePrimitive(const PrimitiveAsset *asset, MaterialProgram *material, Pipeline *p=nullptr);
};

}//namespace hgl::graph
