#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKRenderableInstance.h>
#include<hgl/graph/VKInlinePipeline.h>
#include<hgl/graph/VKVertexAttribBuffer.h>

VK_NAMESPACE_BEGIN
VAB *RenderResource::CreateVAB(VkFormat format,uint32_t count,const void *data,SharingMode sharing_mode)
{
    VAB *vb=device->CreateVAB(format,count,data,sharing_mode);

    if(!vb)
        return(nullptr);

    rm_buffers.Add(vb);

    return vb;
}

#define SCENE_DB_CREATE_BUFFER(name)    GPUBuffer *RenderResource::Create##name(VkDeviceSize size,void *data,SharingMode sharing_mode) \
                                        {   \
                                            GPUBuffer *buf=device->Create##name(size,data,sharing_mode);   \
                                            \
                                            if(!buf)return(nullptr);    \
                                            rm_buffers.Add(buf);    \
                                            return(buf);    \
                                        }   \
                                        \
                                        GPUBuffer *RenderResource::Create##name(VkDeviceSize size,SharingMode sharing_mode)    \
                                        {   \
                                            GPUBuffer *buf=device->Create##name(size,sharing_mode);    \
                                            \
                                            if(!buf)return(nullptr);    \
                                            rm_buffers.Add(buf);    \
                                            return(buf);    \
                                        }

    SCENE_DB_CREATE_BUFFER(UBO)
    SCENE_DB_CREATE_BUFFER(SSBO)
    SCENE_DB_CREATE_BUFFER(INBO)

#undef SCENE_DB_CREATE_BUFFER

IndexBuffer *RenderResource::CreateIBO(IndexType index_type,uint32_t count,const void *data,SharingMode sharing_mode)
{
    IndexBuffer *buf=device->CreateIBO(index_type,count,data,sharing_mode);

    if(!buf)return(nullptr);
    rm_buffers.Add(buf);
    return(buf);
}
        
MaterialInstance *RenderResource::CreateMaterialInstance(Material *mtl)
{
    if(!mtl)return(nullptr);

    MaterialInstance *mi=device->CreateMI(mtl);

    if(mi)
        Add(mi);

    return mi;
}

MaterialInstance *RenderResource::CreateMaterialInstance(const OSString &mtl_filename)
{
    Material *mtl=this->CreateMaterial(mtl_filename);

    if(!mtl)
        return(nullptr);

    return CreateMaterialInstance(mtl);
}

Renderable *RenderResource::CreateRenderable(const uint32_t vertex_count)
{
    if(!vertex_count)return(nullptr);

    Renderable *ro=new Renderable(vertex_count);

    if(ro)
        Add(ro);

    return ro;
}

TextRenderable *RenderResource::CreateTextRenderable(Material *mtl)
{
    if(!mtl)return(nullptr);
            
    TextRenderable *tr=new TextRenderable(device,mtl);

    if(tr)
        Add(tr);

    return tr;
}

RenderableInstance *RenderResource::CreateRenderableInstance(Renderable *r,MaterialInstance *mi,Pipeline *p)
{
    if(!p||!mi||!r)
        return(nullptr);

    RenderableInstance *ri=VK_NAMESPACE::CreateRenderableInstance(r,mi,p);

    if(ri)
        Add(ri);

    return ri;
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

Texture2D *CreateTextureFromFile(GPUDevice *device,const OSString &filename,bool auto_mipmaps);

Texture2D *RenderResource::LoadTexture2D(const OSString &filename,bool auto_mipmaps)
{
    Texture2D *tex;
    
    if(texture_by_name.Get(filename,(Texture *&)tex))
        return tex;
    
    tex=CreateTextureFromFile(device,filename,auto_mipmaps);

    if(tex)
    {
        texture_by_name.Add(filename,tex);
        Add(tex);
    }

    return tex;
}
VK_NAMESPACE_END
