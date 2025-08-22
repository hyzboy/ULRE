#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/Mesh.h>
#include<hgl/graph/VKInlinePipeline.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKIndexBuffer.h>

VK_NAMESPACE_BEGIN
VAB *RenderResource::CreateVAB(VkFormat format,uint32_t count,const void *data,SharingMode sharing_mode)
{
    VAB *vb=device->CreateVAB(format,count,data,sharing_mode);

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

MaterialInstance *RenderResource::CreateMaterialInstance(Material *mtl,const VIL *vil)
{
    if(!mtl||!vil)return(nullptr);

    MaterialInstance *mi=mtl->CreateMI(vil);

    if(mi)
        Add(mi);

    return mi;
}

MaterialInstance *RenderResource::CreateMaterialInstance(Material *mtl,const VILConfig *vil_cfg)
{
    if(!mtl)return(nullptr);

    MaterialInstance *mi=mtl->CreateMI(vil_cfg);

    if(mi)
        Add(mi);

    return mi;
}

MaterialInstance *RenderResource::CreateMaterialInstance(Material *mtl,const VIL *vil,const void *mi_data,const int mi_bytes)
{
    if(!mtl||!vil)return(nullptr);
    if(!mi_data||mi_bytes<=0)return(nullptr);

    MaterialInstance *mi=mtl->CreateMI(vil);

    if(!mi)
        return nullptr;

    Add(mi);
    mi->WriteMIData(mi_data,mi_bytes);

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

MaterialInstance *RenderResource::CreateMaterialInstance(const AnsiString &mtl_name,const mtl::MaterialCreateInfo *mci,const VILConfig *vil_cfg)
{
    Material *mtl=this->CreateMaterial(mtl_name,mci);

    if(!mtl)
        return(nullptr);

    return CreateMaterialInstance(mtl,vil_cfg);
}

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
