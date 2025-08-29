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

Sampler *RenderResource::CreateSampler(VkSamplerCreateInfo *sci)
{
    Sampler *s=device->CreateSampler(sci);

    if(s)
        Add(s);

    return s;
}

Sampler *RenderResource::CreateSampler(Texture *tex)
{
    Sampler *s=device->CreateSampler(tex);

    if(s)
        Add(s);

    return s;    
}
VK_NAMESPACE_END
