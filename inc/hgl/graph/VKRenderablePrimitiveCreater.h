#ifndef HGL_VK_RENDERABLE_PRIMITIVE_CREATER_INCLUDE
#define HGL_VK_RENDERABLE_PRIMITIVE_CREATER_INCLUDE

#include<hgl/graph/VKRenderResource.h>

VK_NAMESPACE_BEGIN
class RenderablePrimitiveCreater
{
    RenderResource *rr;

    uint32_t vertex_count;

    Primitive *prim;

public:

    RenderablePrimitiveCreater(RenderResource *_rr,const AnsiString &name,uint32_t vc)
    {
        rr=_rr;
        vertex_count=vc;

        prim=rr->CreatePrimitive(name,vertex_count);
    }

    VAB *SetVAB(const AnsiString &name,const VkFormat &fmt,const void *buf)
    {
        VAB *vab=rr->CreateVAB(fmt,vertex_count,buf);

        if(!vab)
            return(nullptr);

        prim->Set(name,vab);
        return(vab);
    }

    IndexBuffer *SetIBO(const IndexType &it,const void *buf,const uint32_t index_count)
    {
        IndexBuffer *ibo=rr->CreateIBO(it,index_count,buf);
    
        if(!ibo)
            return(nullptr);
    
        prim->Set(ibo);
        return(ibo);
    }

    Renderable *Create(MaterialInstance *mi,Pipeline *p)
    {
        return rr->CreateRenderable(prim,mi,p);
    }
};//class RenderablePrimitiveCreater
VK_NAMESPACE_END
#endif // HGL_VK_RENDERABLE_PRIMITIVE_CREATER_INCLUDE