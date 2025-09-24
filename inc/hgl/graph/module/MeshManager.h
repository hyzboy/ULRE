#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/mesh/Mesh.h>
#include<hgl/type/ObjectManage.h>

VK_NAMESPACE_BEGIN

using MeshID = int;

// Forward declarations to avoid header ordering issues
class GeometryCreater;
class Geometry;
class MaterialInstance;
class Pipeline;

GRAPH_MODULE_CLASS(MeshManager)
{
private:

    IDObjectManage<MeshID, Mesh> rm_mesh;    ///<渲染实例集合集

    MeshManager(RenderFramework *);
    ~MeshManager() = default;

    friend class GraphModuleManager;

public: // Add/Get/Release

    MeshID Add(Mesh *m) { return rm_mesh.Add(m); }
    Mesh *Get(const MeshID &id) { return rm_mesh.Get(id); }
    void Release(Mesh *m) { rm_mesh.Release(m); }

public: // Create

    Mesh *CreateMesh(Geometry *r, MaterialInstance *mi, Pipeline *p);
    Mesh *CreateMesh(GeometryCreater *pc, MaterialInstance *mi, Pipeline *p);
};

VK_NAMESPACE_END
