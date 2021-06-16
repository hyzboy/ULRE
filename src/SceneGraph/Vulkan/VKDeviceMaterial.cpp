#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterial.h>
#include"VKDescriptorSetLayoutCreater.h"

VK_NAMESPACE_BEGIN
Material *GPUDevice::CreateMaterial(const UTF8String &mtl_name,ShaderModuleMap *shader_maps)
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

    return(new Material(mtl_name,shader_maps,shader_stage_list,dsl_creater));
}

Material *GPUDevice::CreateMaterial(const UTF8String &mtl_name,const VertexShaderModule *vertex_shader_module,const ShaderModule *fragment_shader_module)
{
    if(!vertex_shader_module||!fragment_shader_module)
        return(nullptr);

    if(!vertex_shader_module->IsVertex())return(nullptr);
    if(!fragment_shader_module->IsFragment())return(nullptr);

    ShaderModuleMap *smm=new ShaderModuleMap;

    smm->Add(vertex_shader_module);
    smm->Add(fragment_shader_module);

    return CreateMaterial(mtl_name,smm);
}

Material *GPUDevice::CreateMaterial(const UTF8String &mtl_name,const VertexShaderModule *vertex_shader_module,const ShaderModule *geometry_shader_module,const ShaderModule *fragment_shader_module)
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

    return CreateMaterial(mtl_name,smm);
}
VK_NAMESPACE_END