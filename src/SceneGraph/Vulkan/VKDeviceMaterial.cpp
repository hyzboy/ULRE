#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialDescriptorSets.h>
#include"VKDescriptorSetLayoutCreater.h"

VK_NAMESPACE_BEGIN
Material *GPUDevice::CreateMaterial(const UTF8String &mtl_name,ShaderModuleMap *shader_maps,MaterialDescriptorSets *mds)
{
    const int shader_count=shader_maps->GetCount();

    if(shader_count<2)
        return(nullptr);

    const ShaderModule *sm;

    if(!shader_maps->Get(VK_SHADER_STAGE_VERTEX_BIT,sm))
        return(nullptr);

    DescriptorSetLayoutCreater *dsl_creater=CreateDescriptorSetLayoutCreater(mds);
    
    if(!dsl_creater->CreatePipelineLayout())
    {
        delete shader_maps;
        SAFE_CLEAR(mds);
        return(nullptr);
    }

    return(new Material(mtl_name,shader_maps,mds,dsl_creater));
}

Material *GPUDevice::CreateMaterial(const UTF8String &mtl_name,const VertexShaderModule *vertex_shader_module,const ShaderModule *fragment_shader_module,MaterialDescriptorSets *mds)
{
    if(!vertex_shader_module||!fragment_shader_module)
        return(nullptr);

    if(!vertex_shader_module->IsVertex())return(nullptr);
    if(!fragment_shader_module->IsFragment())return(nullptr);

    ShaderModuleMap *smm=new ShaderModuleMap;

    smm->Add(vertex_shader_module);
    smm->Add(fragment_shader_module);

    return CreateMaterial(mtl_name,smm,mds);
}

Material *GPUDevice::CreateMaterial(const UTF8String &mtl_name,const VertexShaderModule *vertex_shader_module,const ShaderModule *geometry_shader_module,const ShaderModule *fragment_shader_module,MaterialDescriptorSets *mds)
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

    return CreateMaterial(mtl_name,smm,mds);
}
VK_NAMESPACE_END