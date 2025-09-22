#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/mesh/SubMesh.h>
#include<hgl/type/ObjectManage.h>

VK_NAMESPACE_BEGIN

using MeshID = int;

// Forward declarations to avoid header ordering issues
class PrimitiveCreater;
class Primitive;
class MaterialInstance;
class Pipeline;

GRAPH_MODULE_CLASS(MeshManager)
{
private:

    IDObjectManage<MeshID, SubMesh> rm_mesh;    ///<渲染实例集合集

    MeshManager(RenderFramework *);
    ~MeshManager() = default;

    friend class GraphModuleManager;

public: // Add/Get/Release

    MeshID Add(SubMesh *m) { return rm_mesh.Add(m); }
    SubMesh *Get(const MeshID &id) { return rm_mesh.Get(id); }
    void Release(SubMesh *m) { rm_mesh.Release(m); }

public: // Create

    SubMesh *CreateSubMesh(Primitive *r, MaterialInstance *mi, Pipeline *p);
    SubMesh *CreateSubMesh(PrimitiveCreater *pc, MaterialInstance *mi, Pipeline *p);
};

VK_NAMESPACE_END
