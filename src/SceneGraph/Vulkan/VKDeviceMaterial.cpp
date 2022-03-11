#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialDescriptorSets.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKDescriptorSets.h>
#include<hgl/graph/VKShaderModuleMap.h>
#include<hgl/graph/VKVertexAttributeBinding.h>
#include"VKPipelineLayoutData.h"

VK_NAMESPACE_BEGIN
DescriptorSets *GPUDevice::CreateDescriptorSets(const PipelineLayoutData *pld,const DescriptorSetsType &type)const
{
    ENUM_CLASS_RANGE_ERROR_RETURN_NULLPTR(type);

    const uint32_t binding_count=pld->binding_count[size_t(type)];

    if(!binding_count)
        return(nullptr);

    DescriptorSetAllocateInfo alloc_info;

    alloc_info.descriptorPool       = attr->desc_pool;
    alloc_info.descriptorSetCount   = 1;
    alloc_info.pSetLayouts          = pld->layouts+size_t(type);

    VkDescriptorSet desc_set;

    if(vkAllocateDescriptorSets(attr->device,&alloc_info,&desc_set)!=VK_SUCCESS)
        return(nullptr);

    return(new DescriptorSets(attr->device,binding_count,pld->pipeline_layout,desc_set));
}

MaterialParameters *GPUDevice::CreateMP(const MaterialDescriptorSets *mds,const PipelineLayoutData *pld,const DescriptorSetsType &desc_set_type)
{
    if(!mds||!pld)return(nullptr);
    if(!RangeCheck<DescriptorSetsType>(desc_set_type))
        return(nullptr);

    DescriptorSets *ds=CreateDescriptorSets(pld,desc_set_type);

    if(!ds)return(nullptr);

#ifdef _DEBUG
    const UTF8String addr_string=HexToString<char,uint64_t>((uint64_t)(ds->GetDescriptorSet()));

    LOG_INFO(U8_TEXT("Create [DescriptSets:")+addr_string+("] OK! Material Name: \"")+mds->GetMaterialName()+U8_TEXT("\" Type: ")+GetDescriptorSetsTypeName(desc_set_type));
#endif//_DEBUG

    return(new MaterialParameters(mds,desc_set_type,ds));
}

MaterialParameters *GPUDevice::CreateMP(Material *mtl,const DescriptorSetsType &desc_set_type)
{
    if(!mtl)return(nullptr);

    return CreateMP(mtl->GetDescriptorSets(),mtl->GetPipelineLayoutData(),desc_set_type);
}

void CreateShaderStageList(List<VkPipelineShaderStageCreateInfo> &shader_stage_list,ShaderModuleMap *shader_maps)
{
    const ShaderModule *sm;

    const int shader_count=shader_maps->GetCount();
    shader_stage_list.SetCount(shader_count);
    
    VkPipelineShaderStageCreateInfo *p=shader_stage_list.GetData();        

    auto **itp=shader_maps->GetDataList();
    for(int i=0;i<shader_count;i++)
    {
        sm=(*itp)->right;
        hgl_cpy(p,sm->GetCreateInfo(),1);

        ++p;
        ++itp;
    }
}

Material *GPUDevice::CreateMaterial(const UTF8String &mtl_name,ShaderModuleMap *shader_maps,MaterialDescriptorSets *mds)
{
    const int shader_count=shader_maps->GetCount();

    if(shader_count<2)
        return(nullptr);

    const ShaderModule *vsm;

    if(!shader_maps->Get(VK_SHADER_STAGE_VERTEX_BIT,vsm))
        return(nullptr);

    PipelineLayoutData *pld=CreatePipelineLayoutData(mds);
    
    if(!pld)
    {
        delete shader_maps;
        SAFE_CLEAR(mds);
        return(nullptr);
    }

    MaterialData *data=new MaterialData;

    data->name          =mtl_name;
    data->shader_maps   =shader_maps;
    data->mds           =mds;
    data->vertex_sm     =(VertexShaderModule *)vsm;

    CreateShaderStageList(data->shader_stage_list,shader_maps);

    data->pipeline_layout_data=pld;
    data->mp.m=CreateMP(mds,pld,DescriptorSetsType::Material     );
    data->mp.r=CreateMP(mds,pld,DescriptorSetsType::Renderable   );
    data->mp.g=CreateMP(mds,pld,DescriptorSetsType::Global       );

    return(new Material(data));
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
