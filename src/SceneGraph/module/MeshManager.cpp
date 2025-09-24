#include<hgl/graph/module/MeshManager.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN

GRAPH_MODULE_CONSTRUCT(MeshManager)
{
}

Mesh *MeshManager::CreateMesh(Geometry *r, MaterialInstance *mi, Pipeline *p)
{
    if(!p||!mi||!r)
        return(nullptr);

    Mesh *ri=VK_NAMESPACE::DirectCreateMesh(r,mi,p);

    if(ri)
        Add(ri);

    return ri;
}

Mesh *MeshManager::CreateMesh(GeometryCreater *pc, MaterialInstance *mi, Pipeline *p)
{
    if(!p||!mi||!pc)
        return(nullptr);

    Geometry *prim=pc->Create();

    if(!prim)
        return(nullptr);

    Mesh *ri=VK_NAMESPACE::DirectCreateMesh(prim,mi,p);

    if(ri)
    {
        // Add geometry ownership remains responsibility of caller in many places; keep behavior similar to previous
        Add(ri);
        return ri;
    }

    delete prim;
    return(nullptr);
}

VK_NAMESPACE_END
