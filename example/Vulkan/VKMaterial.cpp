#include"VKMaterial.h"
#include"VKDevice.h"
#include"VKDescriptorSets.h"
#include"VKShaderModule.h"
#include"VKVertexAttributeBinding.h"
#include"VKRenderable.h"
#include"VKBuffer.h"
VK_NAMESPACE_BEGIN
Material::Material(Device *dev,ShaderModuleMap *smm)
{
    device=*dev;
    shader_maps=smm;
    dsl_creater=new DescriptorSetLayoutCreater(dev);

    const int shader_count=shader_maps->GetCount();

    shader_stage_list.SetCount(shader_count);
    VkPipelineShaderStageCreateInfo *p=shader_stage_list.GetData();

    const ShaderModule *sm;
    auto **itp=shader_maps->GetDataList();
    for(int i=0;i<shader_count;i++)
    {
        sm=(*itp)->right;

        if(sm->GetStage()==VK_SHADER_STAGE_VERTEX_BIT)
            vertex_sm=(VertexShaderModule *)sm;

        memcpy(p,sm->GetCreateInfo(),sizeof(VkPipelineShaderStageCreateInfo));
        ++p;
        ++itp;

        {
            const ShaderBindingList &ubo_list=sm->GetUBOBindingList();

            dsl_creater->BindUBO(ubo_list.GetData(),ubo_list.GetCount(),sm->GetStage());
        }
    }
}

Material::~Material()
{
    delete dsl_creater;

    const int count=shader_stage_list.GetCount();

    if(count>0)
    {
        VkPipelineShaderStageCreateInfo *ss=shader_stage_list.GetData();
        for(int i=0;i<count;i++)
        {
            vkDestroyShaderModule(device,ss->module,nullptr);
            ++ss;
        }
    }

    delete shader_maps;
}

const int Material::GetUBOBinding(const UTF8String &name)const
{
    int result;
    const int shader_count=shader_maps->GetCount();

    const ShaderModule *sm;
    auto **itp=shader_maps->GetDataList();
    for(int i=0;i<shader_count;i++)
    {
        sm=(*itp)->right;
        result=sm->GetUBO(name);
        if(result!=-1)
            return result;

        ++itp;
    }

    return(-1);
}

const int Material::GetVBOBinding(const UTF8String &name)const
{
    if(!vertex_sm)return(-1);

    return vertex_sm->GetBinding(name);
}

MaterialInstance *Material::CreateInstance()const
{
    if(!vertex_sm)
        return(nullptr);

    VertexAttributeBinding *vab=vertex_sm->CreateVertexAttributeBinding();

    if(!vab)
        return(nullptr);

    DescriptorSetLayout *dsl=dsl_creater->Create();

    return(new MaterialInstance(this,vertex_sm,vab,dsl));
}

MaterialInstance::MaterialInstance(const Material *m,const VertexShaderModule *vsm,VertexAttributeBinding *v,DescriptorSetLayout *d)
{
    mat=m;
    vertex_sm=vsm;
    vab=v;
    desc_set_layout=d;
}

MaterialInstance::~MaterialInstance()
{
    delete desc_set_layout;
    delete vab;
}

bool MaterialInstance::UpdateUBO(const uint32_t binding,const VkDescriptorBufferInfo *buf_info)
{
    return desc_set_layout->UpdateUBO(binding,buf_info);
}

void MaterialInstance::Write(VkPipelineVertexInputStateCreateInfo &vis)const
{
    return vab->Write(vis);
}

Renderable *MaterialInstance::CreateRenderable()
{
    return(new Renderable(vertex_sm));
}
VK_NAMESPACE_END
