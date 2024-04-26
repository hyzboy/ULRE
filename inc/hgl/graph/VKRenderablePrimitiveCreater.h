#ifndef HGL_VK_RENDERABLE_PRIMITIVE_CREATER_INCLUDE
#define HGL_VK_RENDERABLE_PRIMITIVE_CREATER_INCLUDE

#include<hgl/graph/VKRenderResource.h>

VK_NAMESPACE_BEGIN
/**
* 可绘制图元创建器
*/
class RenderablePrimitiveCreater
{
    RenderResource *rr;

    VkDeviceSize vertex_count;

    Primitive *prim;

public:

    RenderablePrimitiveCreater(RenderResource *_rr,const AnsiString &name,VkDeviceSize vc)
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

        prim->SetVAB(name,vab);
        return(vab);
    }

    IndexBuffer *SetIndex(const IndexType &it,const void *buf,const VkDeviceSize index_count)
    {
        IndexBuffer *ibo=rr->CreateIBO(it,index_count,buf);
    
        if(!ibo)
            return(nullptr);
    
        prim->SetIndex(ibo,0,index_count);
        return(ibo);
    }

    Renderable *Create(MaterialInstance *mi,Pipeline *p)
    {
        return rr->CreateRenderable(prim,mi,p);
    }
};//class RenderablePrimitiveCreater
VK_NAMESPACE_END
#endif // HGL_VK_RENDERABLE_PRIMITIVE_CREATER_INCLUDE
