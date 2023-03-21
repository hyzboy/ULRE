#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKInlinePipeline.h>
#include<hgl/graph/VKVertexAttribBuffer.h>

VK_NAMESPACE_BEGIN
VBO *RenderResource::CreateVBO(VkFormat format,uint32_t count,const void *data,SharingMode sharing_mode)
{
    VBO *vb=device->CreateVBO(format,count,data,sharing_mode);

    if(!vb)
        return(nullptr);

    rm_buffers.Add(vb);

    return vb;
}

#define SCENE_DB_CREATE_BUFFER(name)    DeviceBuffer *RenderResource::Create##name(VkDeviceSize size,void *data,SharingMode sharing_mode) \
                                        {   \
                                            DeviceBuffer *buf=device->Create##name(size,data,sharing_mode);   \
                                            \
                                            if(!buf)return(nullptr);    \
                                            rm_buffers.Add(buf);    \
                                            return(buf);    \
                                        }   \
                                        \
                                        DeviceBuffer *RenderResource::Create##name(VkDeviceSize size,SharingMode sharing_mode)    \
                                        {   \
                                            DeviceBuffer *buf=device->Create##name(size,sharing_mode);    \
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
        
MaterialInstance *RenderResource::CreateMaterialInstance(Material *mtl,const VILConfig *vil_cfg)
{
    if(!mtl)return(nullptr);

    MaterialInstance *mi=device->CreateMI(mtl,vil_cfg);

    if(mi)
        Add(mi);

    return mi;
}

MaterialInstance *RenderResource::CreateMaterialInstance(const OSString &mtl_filename,const VILConfig *vil_cfg)
{
    Material *mtl=this->CreateMaterial(mtl_filename);

    if(!mtl)
        return(nullptr);

    return CreateMaterialInstance(mtl,vil_cfg);
}

MaterialInstance *RenderResource::CreateMaterialInstance(const hgl::shadergen::MaterialCreateInfo *mci,const VILConfig *vil_cfg)
{
    Material *mtl=this->CreateMaterial(mci);

    if(!mtl)
        return(nullptr);

    return CreateMaterialInstance(mtl,vil_cfg);
}

Primitive *RenderResource::CreatePrimitive(const uint32_t vertex_count)
{
    if(!vertex_count)return(nullptr);

    Primitive *ro=new Primitive(vertex_count);

    if(ro)
        Add(ro);

    return ro;
}

Renderable *RenderResource::CreateRenderable(Primitive *r,MaterialInstance *mi,Pipeline *p)
{
    if(!p||!mi||!r)
        return(nullptr);

    Renderable *ri=VK_NAMESPACE::CreateRenderable(r,mi,p);

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

Texture2D *CreateTexture2DFromFile(GPUDevice *device,const OSString &filename,bool auto_mipmaps);

Texture2D *RenderResource::LoadTexture2D(const OSString &filename,bool auto_mipmaps)
{
    Texture2D *tex;
    
    if(texture_by_name.Get(filename,(Texture *&)tex))
        return tex;
    
    tex=CreateTexture2DFromFile(device,filename,auto_mipmaps);

    if(tex)
    {
        texture_by_name.Add(filename,tex);
        Add(tex);

    #ifdef _DEBUG
        GPUDeviceAttribute *da=device->GetDeviceAttribute();
        const UTF8String name=ToUTF8String(filename);
        
        if(da->debug_maker)
            da->debug_maker->SetImage(tex->GetImage(),"[debug maker] "+name);
        if(da->debug_utils)
            da->debug_utils->SetImage(tex->GetImage(),"[debug utils] "+name);
    #endif//_DEBUG
    }

    return tex;
}

TextureCube *CreateTextureCubeFromFile(GPUDevice *device,const OSString &filename,bool auto_mipmaps);

TextureCube *RenderResource::LoadTextureCube(const OSString &filename,bool auto_mipmaps)
{
    TextureCube *tex;

    if(texture_by_name.Get(filename,(Texture *&)tex))
        return tex;

    tex=CreateTextureCubeFromFile(device,filename,auto_mipmaps);

    if(tex)
    {
        texture_by_name.Add(filename,tex);
        Add(tex);
    }

    return tex;
}
VK_NAMESPACE_END
