﻿#include<hgl/graph/VKRenderResource.h>
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

#define SCENE_DB_CREATE_BUFFER(name)    DeviceBuffer *RenderResource::Create##name(const AnsiString &buf_name,VkDeviceSize size,void *data,SharingMode sharing_mode) \
                                        {   \
                                            DeviceBuffer *buf=device->Create##name(size,data,sharing_mode);   \
                                            \
                                            if(!buf)return(nullptr);    \
                                            AddBuffer(#name ":"+buf_name,buf);    \
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

    MaterialInstance *mi=mtl->CreateMI(vil_cfg);

    if(mi)
        Add(mi);

    return mi;
}

MaterialInstance *RenderResource::CreateMaterialInstance(Material *mtl,const VILConfig *vil_cfg,const void *mi_data,const int mi_bytes)
{
    if(!mtl)return(nullptr);
    if(!mi_data||mi_bytes<=0)return(nullptr);

    MaterialInstance *mi=mtl->CreateMI(vil_cfg);

    if(!mi)
        return nullptr;

    Add(mi);
    mi->WriteMIData(mi_data,mi_bytes);

    return mi;
}

MaterialInstance *RenderResource::CreateMaterialInstance(const mtl::MaterialCreateInfo *mci,const VILConfig *vil_cfg)
{
    Material *mtl=this->CreateMaterial(mci);

    if(!mtl)
        return(nullptr);

    return CreateMaterialInstance(mtl,vil_cfg);
}

Primitive *RenderResource::CreatePrimitive(const AnsiString &name,const uint32_t vertex_count)
{
    if(!vertex_count)return(nullptr);

    Primitive *ro=new Primitive(device,name,vertex_count);

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
        DebugUtils *du=device->GetDebugUtils();

        if(du)
        {
            const UTF8String name=ToUTF8String(filename);
        
            du->SetImage(tex->GetImage(),"Tex2D:"+name);
        }
    #endif//_DEBUG
    }

    return tex;
}

Texture2DArray *RenderResource::CreateTexture2DArray(const AnsiString &name,const uint32_t width,const uint32_t height,const uint32_t layer,const VkFormat &fmt,bool auto_mipmaps)
{
    Texture2DArray *ta=device->CreateTexture2DArray(width,height,layer,fmt,auto_mipmaps);

    if(ta)
        Add(ta);
    else
        return nullptr;

    #ifdef _DEBUG
        DebugUtils *du=device->GetDebugUtils();
        
        if(du)
        {
            du->SetImage(ta->GetImage(),"Tex2DArrayImage:"+name);
            du->SetImageView(ta->GetVulkanImageView(),"Tex2DArrayImageView:"+name);
            du->SetDeviceMemory(ta->GetDeviceMemory(),"Tex2DArrayMemory:"+name);
        }
    #endif//_DEBUG

    return ta;
}

bool LoadTexture2DLayerFromFile(GPUDevice *device,Texture2DArray *t2d,const uint32_t layer,const OSString &filename,bool auto_mipmaps);

bool RenderResource::LoadTexture2DToArray(Texture2DArray *ta,const uint32_t layer,const OSString &filename)
{
    if(!ta)return(false);

    if(!LoadTexture2DLayerFromFile(device,ta,layer,filename,false))
        return(false);

    return(true);
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
