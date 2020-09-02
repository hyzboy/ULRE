#include<hgl/graph/vulkan/VKDatabase.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/RenderableInstance.h>

VK_NAMESPACE_BEGIN
VAB *SceneDB::CreateVAB(VkFormat format,uint32_t count,const void *data,VkSharingMode sharing_mode)
{
    VAB *vb=device->CreateVAB(format,count,data,sharing_mode);

    if(!vb)
        return(nullptr);

    rm_buffers.Add(vb);

    return vb;
}

#define SCENE_DB_CREATE_BUFFER(name)    Buffer *SceneDB::Create##name(VkDeviceSize size,void *data,VkSharingMode sharing_mode) \
                                        {   \
                                            Buffer *buf=device->Create##name(size,data,sharing_mode);   \
                                            \
                                            if(!buf)return(nullptr);    \
                                            rm_buffers.Add(buf);    \
                                            return(buf);    \
                                        }   \
                                        \
                                        Buffer *SceneDB::Create##name(VkDeviceSize size,VkSharingMode sharing_mode)    \
                                        {   \
                                            Buffer *buf=device->Create##name(size,sharing_mode);    \
                                            \
                                            if(!buf)return(nullptr);    \
                                            rm_buffers.Add(buf);    \
                                            return(buf);    \
                                        }

    SCENE_DB_CREATE_BUFFER(UBO)
    SCENE_DB_CREATE_BUFFER(SSBO)
    SCENE_DB_CREATE_BUFFER(INBO)

#undef SCENE_DB_CREATE_BUFFER

IndexBuffer *SceneDB::CreateIBO(VkIndexType index_type,uint32_t count,const void *data,VkSharingMode sharing_mode)
{
    IndexBuffer *buf=device->CreateIBO(index_type,count,data,sharing_mode);

    if(!buf)return(nullptr);
    rm_buffers.Add(buf);
    return(buf);
}
        
MaterialInstance *SceneDB::CreateMaterialInstance(Material *mtl)
{
    if(!mtl)return(nullptr);

    MaterialInstance *mi=mtl->CreateInstance();

    if(mi)
        Add(mi);

    return mi;
}

Renderable *SceneDB::CreateRenderable(Material *mtl,const uint32_t vertex_count)
{
    if(!mtl)return(nullptr);

    Renderable *ro=mtl->CreateRenderable(vertex_count);

    if(ro)
        Add(ro);

    return ro;
}

TextRenderable *SceneDB::CreateTextRenderable(Material *mtl)
{
    if(!mtl)return(nullptr);
            
    TextRenderable *tr=new TextRenderable(device,mtl);

    if(tr)
        Add(tr);

    return tr;
}

RenderableInstance *SceneDB::CreateRenderableInstance(Pipeline *p,MaterialInstance *mi,Renderable *r)
{
    if(!p||!mi||!r)
        return(nullptr);

    RenderableInstance *ri=new RenderableInstance(p,mi,r);

    if(ri)
        Add(ri);

    return ri;
}
        
Sampler *SceneDB::CreateSampler(VkSamplerCreateInfo *sci)
{
    Sampler *s=device->CreateSampler(sci);

    if(s)
        Add(s);

    return s;
}
VK_NAMESPACE_END
