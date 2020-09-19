#include<hgl/graph/vulkan/VKDatabase.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/RenderableInstance.h>

VK_NAMESPACE_BEGIN
VAB *Database::CreateVAB(VkFormat format,uint32_t count,const void *data,SharingMode sharing_mode)
{
    VAB *vb=device->CreateVAB(format,count,data,sharing_mode);

    if(!vb)
        return(nullptr);

    rm_buffers.Add(vb);

    return vb;
}

#define SCENE_DB_CREATE_BUFFER(name)    Buffer *Database::Create##name(VkDeviceSize size,void *data,SharingMode sharing_mode) \
                                        {   \
                                            Buffer *buf=device->Create##name(size,data,sharing_mode);   \
                                            \
                                            if(!buf)return(nullptr);    \
                                            rm_buffers.Add(buf);    \
                                            return(buf);    \
                                        }   \
                                        \
                                        Buffer *Database::Create##name(VkDeviceSize size,SharingMode sharing_mode)    \
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

IndexBuffer *Database::CreateIBO(IndexType index_type,uint32_t count,const void *data,SharingMode sharing_mode)
{
    IndexBuffer *buf=device->CreateIBO(index_type,count,data,sharing_mode);

    if(!buf)return(nullptr);
    rm_buffers.Add(buf);
    return(buf);
}
        
MaterialInstance *Database::CreateMaterialInstance(Material *mtl)
{
    if(!mtl)return(nullptr);

    MaterialInstance *mi=mtl->CreateInstance();

    if(mi)
        Add(mi);

    return mi;
}

MaterialInstance *Database::CreateMaterialInstance(const OSString &mtl_filename)
{
    Material *mtl=this->CreateMaterial(mtl_filename);

    if(!mtl)
        return(nullptr);

    return CreateMaterialInstance(mtl);
}

Pipeline *Database::CreatePipeline(Material *mtl,RenderTarget *rt,const OSString &pipeline_filename,const Prim &prim,const bool prim_restart)
{
    PipelineData *pd=vulkan::GetPipelineData(pipeline_filename);

    if(!pd)return(nullptr);

    pd->Set(prim,prim_restart);

    Pipeline *p=device->CreatePipeline(pd,mtl,rt);

    if(p)
        Add(p);

    return(p);
}

Pipeline *Database::CreatePipeline(MaterialInstance *mi,RenderTarget *rt,const OSString &filename,const Prim &prim,const bool prim_restart)
{
    return CreatePipeline(mi->GetMaterial(),rt,filename,prim,prim_restart);
}

Renderable *Database::CreateRenderable(Material *mtl,const uint32_t vertex_count)
{
    if(!mtl)return(nullptr);

    Renderable *ro=mtl->CreateRenderable(vertex_count);

    if(ro)
        Add(ro);

    return ro;
}

TextRenderable *Database::CreateTextRenderable(Material *mtl)
{
    if(!mtl)return(nullptr);
            
    TextRenderable *tr=new TextRenderable(device,mtl);

    if(tr)
        Add(tr);

    return tr;
}

RenderableInstance *Database::CreateRenderableInstance(Pipeline *p,MaterialInstance *mi,Renderable *r)
{
    if(!p||!mi||!r)
        return(nullptr);

    RenderableInstance *ri=new RenderableInstance(p,mi,r);

    if(ri)
        Add(ri);

    return ri;
}
        
Sampler *Database::CreateSampler(VkSamplerCreateInfo *sci)
{
    Sampler *s=device->CreateSampler(sci);

    if(s)
        Add(s);

    return s;
}
VK_NAMESPACE_END
