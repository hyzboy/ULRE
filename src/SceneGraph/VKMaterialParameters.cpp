#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKMaterialDescriptorManager.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKDescriptorSet.h>
#include<hgl/vk/VKBuffer.h>

namespace hgl::graph{
MaterialParameters::MaterialParameters(const MaterialDescriptorManager *mdm,const DescriptorSetType &type,DescriptorSet *ds)
{
    desc_manager=mdm;
    set_type=type;
    descriptor_set=ds;
}

MaterialParameters::~MaterialParameters()
{
    delete descriptor_set;
}

bool MaterialParameters::BindUBO(const int &index,const IGPUBuffer *gpu,bool dynamic)
{
    if(index<0||!gpu)
        return(false);

    if(!descriptor_set->BindUBO(index,gpu,dynamic))
        return(false);

    return(true);
}

bool MaterialParameters::BindUBO(const AnsiString &name,const IGPUBuffer *gpu,bool dynamic)
{
    if(name.IsEmpty()||!gpu)
        return(false);

    const int index=desc_manager->GetUBO(set_type,name,dynamic);

    if(index<0)
        return(false);

    if(!descriptor_set->BindUBO(index,gpu,dynamic))
        return(false);

    return(true);
}

bool MaterialParameters::BindSSBO(const int &index,const IGPUBuffer *gpu,bool dynamic)
{
    if(index<0||!gpu)
        return(false);

    if(!descriptor_set->BindSSBO(index,gpu,dynamic))
        return(false);

    return(true);
}

bool MaterialParameters::BindSSBO(const AnsiString &name,const IGPUBuffer *gpu,bool dynamic)
{
    if(name.IsEmpty()||!gpu)
        return(false);

    const int index=desc_manager->GetSSBO(set_type,name,dynamic);

    if(index<0)
        return(false);

    if(!descriptor_set->BindSSBO(index,gpu,dynamic))
        return(false);

    return(true);
}

bool MaterialParameters::BindSSBO(const AnsiString &name,const VkBuffer buf,const VkDeviceSize offset,const VkDeviceSize range,bool dynamic)
{
    if(name.IsEmpty()||!buf)
        return(false);

    const int index=desc_manager->GetSSBO(set_type,name,dynamic);

    if(index<0)
    {
        // 【诊断】GetSSBO 查找失败——descriptor 更新静默失败 → 残留前一次绑定（多对象场景全用首个对象）
        GLogWarning("[BIND-FAIL] GetSSBO(set=%d, name=%s, dynamic=%d) not found — descriptor NOT updated",
                    int(set_type), name.c_str(), int(dynamic));
        return(false);
    }

    if(!descriptor_set->BindSSBO(index,buf,offset,range,dynamic))
        return(false);

    return(true);
}

bool MaterialParameters::BindTexture(const int &index,Texture *tex)
{
    if(index < 0 || !tex)
        return(false);

    if(!descriptor_set->BindTexture(index,tex))
        return(false);

    return(true);
}

bool MaterialParameters::BindTexture(const AnsiString &name,Texture *tex)
{
    if(name.IsEmpty() || !tex)
        return(false);

    const int index = desc_manager->GetTexture(set_type,name);

    if(index < 0)
        return(false);

    if(!descriptor_set->BindTexture(index,tex))
        return(false);

    return(true);
}

bool MaterialParameters::BindTextureSampler(const int &index,Texture *tex,Sampler *sampler)
{
    if(index<0||!tex||!sampler)
        return(false);

    if(!descriptor_set->BindTextureSampler(index,tex,sampler))
        return(false);

    return(true);
}

bool MaterialParameters::BindTextureSampler(const AnsiString &name,Texture *tex,Sampler *sampler)
{
    if(name.IsEmpty()||!tex||!sampler)
        return(false);

    const int index=desc_manager->GetTextureSampler(set_type,name);

    if(index<0)
        return(false);

    if(!descriptor_set->BindTextureSampler(index,tex,sampler))
        return(false);

    return(true);
}

bool MaterialParameters::BindInputAttachment(const int &index,ImageView *iv)
{
    if(index<0||!iv)
        return(false);

    if(!descriptor_set->BindInputAttachment(index,iv))
        return(false);

    return(true);
}

bool MaterialParameters::BindInputAttachment(const AnsiString &name,ImageView *iv)
{
    if(name.IsEmpty()||!iv)
        return(false);

    const int index=desc_manager->GetInputAttachment(set_type,name);

    if(index<0)
        return(false);

    if(!descriptor_set->BindInputAttachment(index,iv))
        return(false);

    return(true);
}

void MaterialParameters::Update()
{
    descriptor_set->Update();
}
}//namespace hgl::graph
