#include<hgl/graph/vulkan/VKMaterial.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKDescriptorSets.h>
#include<hgl/graph/vulkan/VKShaderModule.h>
#include<hgl/graph/vulkan/ShaderModuleMap.h>
#include<hgl/graph/vulkan/VKVertexAttributeBinding.h>
#include<hgl/graph/vulkan/VKRenderable.h>
#include<hgl/graph/vulkan/VKBuffer.h>
#include"VKDescriptorSetLayoutCreater.h"
VK_NAMESPACE_BEGIN
Material *Device::CreateMaterial(ShaderModuleMap *shader_maps)
{
    const int shader_count=shader_maps->GetCount();

    if(shader_count<2)
        return(nullptr);

    const ShaderModule *sm;

    if(!shader_maps->Get(VK_SHADER_STAGE_VERTEX_BIT,sm))
        return(nullptr);

    DescriptorSetLayoutCreater *dsl_creater=CreateDescriptorSetLayoutCreater();
    List<VkPipelineShaderStageCreateInfo> *shader_stage_list=new List<VkPipelineShaderStageCreateInfo>;

    shader_stage_list->SetCount(shader_count);

    VkPipelineShaderStageCreateInfo *p=shader_stage_list->GetData();

    auto **itp=shader_maps->GetDataList();
    for(int i=0;i<shader_count;i++)
    {
        sm=(*itp)->right;
        hgl_cpy(p,sm->GetCreateInfo());

        dsl_creater->Bind(sm->GetDescriptorList(),sm->GetStage());

        ++p;
        ++itp;
    }

    if(!dsl_creater->CreatePipelineLayout())
    {
        delete shader_stage_list;
        delete dsl_creater;
        delete shader_maps;
        return(nullptr);
    }

    return(new Material(shader_maps,shader_stage_list,dsl_creater));
}

Material *Device::CreateMaterial(const VertexShaderModule *vertex_shader_module,const ShaderModule *fragment_shader_module)
{
    if(!vertex_shader_module||!fragment_shader_module)
        return(nullptr);

    if(!vertex_shader_module->IsVertex())return(nullptr);
    if(!fragment_shader_module->IsFragment())return(nullptr);

    ShaderModuleMap *smm=new ShaderModuleMap;

    smm->Add(vertex_shader_module);
    smm->Add(fragment_shader_module);

    return CreateMaterial(smm);
}

Material *Device::CreateMaterial(const VertexShaderModule *vertex_shader_module,const ShaderModule *geometry_shader_module,const ShaderModule *fragment_shader_module)
{
    if(!vertex_shader_module
     ||!geometry_shader_module
     ||!fragment_shader_module)
        return(nullptr);

    if(!vertex_shader_module->IsVertex())return(nullptr);
    if(!geometry_shader_module->IsGeometry())return(nullptr);
    if(!fragment_shader_module->IsFragment())return(nullptr);

    ShaderModuleMap *smm=new ShaderModuleMap;

    smm->Add(vertex_shader_module);
    smm->Add(geometry_shader_module);
    smm->Add(fragment_shader_module);

    return CreateMaterial(smm);
}

Material::Material(ShaderModuleMap *smm,List<VkPipelineShaderStageCreateInfo> *psci_list,DescriptorSetLayoutCreater *dslc)
{
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

    if(vab)
    {
        vertex_sm->Release(vab);
        delete vab;
    }

    delete shader_stage_list;
    delete shader_maps;
}

const int Material::GetBinding(VkDescriptorType desc_type,const AnsiString &name)const
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

Renderable *Material::CreateRenderable(const uint32_t draw_count)
{
    return(new Renderable(vertex_sm,draw_count));
}
VK_NAMESPACE_END
