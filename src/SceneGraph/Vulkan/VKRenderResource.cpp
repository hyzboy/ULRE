#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/Mesh.h>
#include<hgl/graph/VKInlinePipeline.h>

VK_NAMESPACE_BEGIN

Mesh *RenderResource::CreateMesh(Primitive *r,MaterialInstance *mi,Pipeline *p)
{
    if(!p||!mi||!r)
        return(nullptr);

    Mesh *ri=VK_NAMESPACE::CreateMesh(r,mi,p);

    if(ri)
        Add(ri);

    return ri;
}

Mesh *RenderResource::CreateMesh(PrimitiveCreater *pc,MaterialInstance *mi,Pipeline *p)
{
    if(!p||!mi||!pc)
        return(nullptr);

    Primitive *prim=pc->Create();

    if(!prim)
        return(nullptr);

    Mesh *ri=VK_NAMESPACE::CreateMesh(prim,mi,p);

    if(ri)
    {
        Add(prim);
        Add(ri);

        return ri;
    }

    delete prim;
    return(nullptr);
}

VK_NAMESPACE_END
