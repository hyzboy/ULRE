#include<hgl/graph/vulkan/VKMaterial.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKDescriptorSets.h>
#include<hgl/graph/vulkan/VKShaderModule.h>
#include<hgl/graph/vulkan/VKVertexAttributeBinding.h>
#include<hgl/graph/vulkan/VKRenderable.h>
#include<hgl/graph/vulkan/VKBuffer.h>
#include"VKDescriptorSetLayoutCreater.h"
VK_NAMESPACE_BEGIN
Material *CreateMaterial(Device *dev,ShaderModuleMap *shader_maps)
{
    const int shader_count=shader_maps->GetCount();

    if(shader_count<2)
        return(nullptr);

    const ShaderModule *sm;

    if(!shader_maps->Get(VK_SHADER_STAGE_VERTEX_BIT,sm))
        return(nullptr);

    const VertexShaderModule *vertex_sm=(VertexShaderModule *)sm;

    DescriptorSetLayoutCreater *dsl_creater=new DescriptorSetLayoutCreater(dev);
    List<VkPipelineShaderStageCreateInfo> *shader_stage_list=new List<VkPipelineShaderStageCreateInfo>;

    shader_stage_list->SetCount(shader_count);

    VkPipelineShaderStageCreateInfo *p=shader_stage_list->GetData();

    auto **itp=shader_maps->GetDataList();
    for(int i=0;i<shader_count;i++)
    {
        sm=(*itp)->right;
        memcpy(p,sm->GetCreateInfo(),sizeof(VkPipelineShaderStageCreateInfo));

        dsl_creater->Bind(sm->GetResource(),sm->GetStage());

        ++p;
        ++itp;
    }

    dsl_creater->CreatePipelineLayout();

    return(new Material(dev,shader_maps,shader_stage_list,dsl_creater));
}

Material::Material(Device *dev,ShaderModuleMap *smm,List<VkPipelineShaderStageCreateInfo> *psci_list,DescriptorSetLayoutCreater *dslc)
{
    device=dev;
    shader_maps=smm;
    shader_stage_list=psci_list;
    dsl_creater=dslc;

    const ShaderModule *sm;
    if(smm->Get(VK_SHADER_STAGE_VERTEX_BIT,sm))
    {
        vertex_sm=(VertexShaderModule *)sm;
        vab=vertex_sm->CreateVertexAttributeBinding();
    }
    else
    {
        //理论上不可能到达这里，前面CreateMaterial已经检测过了
        vertex_sm=nullptr;
        vab=nullptr;
    }
}

Material::~Material()
{
    delete dsl_creater;

    vertex_sm->Release(vab);
    delete vab;

    delete shader_stage_list;
    delete shader_maps;
}

const int Material::GetBinding(VkDescriptorType desc_type,const UTF8String &name)const
{
    if(desc_type<VK_DESCRIPTOR_TYPE_BEGIN_RANGE
     ||desc_type>VK_DESCRIPTOR_TYPE_END_RANGE
     ||name.IsEmpty())
        return(-1);

    int binding;
    const int shader_count=shader_maps->GetCount();

    const ShaderModule *sm;
    auto **itp=shader_maps->GetDataList();
    for(int i=0;i<shader_count;i++)
    {
        sm=(*itp)->right;
        binding=sm->GetBinding(desc_type,name);
        if(binding!=-1)
            return binding;

        ++itp;
    }

    return(-1);
}

const VkPipelineLayout Material::GetPipelineLayout()const
{
    return dsl_creater->GetPipelineLayout();
}

DescriptorSets *Material::CreateDescriptorSets()const
{
    return dsl_creater->Create();
}

void Material::Write(VkPipelineVertexInputStateCreateInfo &vis)const
{
    return vab->Write(vis);
}

Renderable *Material::CreateRenderable()
{
    return(new Renderable(vertex_sm));
}
VK_NAMESPACE_END
