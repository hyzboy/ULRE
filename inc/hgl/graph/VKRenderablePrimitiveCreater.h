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

    RenderablePrimitiveCreater(RenderResource *_rr,uint32_t vc)
    {
        rr=_rr;
        vertex_count=vc;

        prim=rr->CreatePrimitive(vertex_count);
    }

    VBO *SetVBO(const AnsiString &name,const VkFormat &fmt,const void *buf)
    {
        VBO *vbo=rr->CreateVBO(fmt,vertex_count,buf);

        if(!vbo)
            return(nullptr);

        prim->Set(name,vbo);
        return(vbo);
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