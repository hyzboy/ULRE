﻿#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialDescriptorManager.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKDescriptorSet.h>
#include<hgl/graph/VKShaderModuleMap.h>
#include<hgl/graph/VKVertexInputLayout.h>
#include"VKPipelineLayoutData.h"

VK_NAMESPACE_BEGIN

PipelineLayoutData *CreatePipelineLayoutData(VkDevice device,const MaterialDescriptorManager *desc_manager);

namespace
{
    DescriptorSet *CreateDS(VkDevice device,VkDescriptorPool desc_pool,const PipelineLayoutData *pld,const DescriptorSetType &type)
    {
        RANGE_CHECK_RETURN_NULLPTR(type);

        const uint32_t binding_count=pld->binding_count[size_t(type)];

        if(!binding_count)
            return(nullptr);

        DescriptorSetAllocateInfo alloc_info;

        alloc_info.descriptorPool       = desc_pool;
        alloc_info.descriptorSetCount   = 1;
        alloc_info.pSetLayouts          = pld->layouts+size_t(type);

        VkDescriptorSet desc_set;

        if(vkAllocateDescriptorSets(device,&alloc_info,&desc_set)!=VK_SUCCESS)
            return(nullptr);

        return(new DescriptorSet(device,binding_count,pld->pipeline_layout,desc_set));
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
            sm=(*itp)->value;
            hgl_cpy(p,sm->GetCreateInfo(),1);

            ++p;
            ++itp;
        }
    }

    MaterialParameters *CreateMP(VkDevice device,VkDescriptorPool desc_pool,const MaterialDescriptorManager *desc_manager,const PipelineLayoutData *pld,const DescriptorSetType &desc_set_type)
    {
        if(!desc_manager||!pld)return(nullptr);
        RANGE_CHECK_RETURN_NULLPTR(desc_set_type)

        DescriptorSet *ds=CreateDS(device,desc_pool,pld,desc_set_type);

        if(!ds)return(nullptr);

    #ifdef _DEBUG
        const UTF8String addr_string=HexToString<char,uint64_t>((uint64_t)(ds->GetDescriptorSet()));

        LOG_INFO(U8_TEXT("Create [DescriptSets:")+addr_string+("] OK! Material Name: \"")+desc_manager->GetMaterialName()+U8_TEXT("\" Type: ")+GetDescriptorSetTypeName(desc_set_type));
    #endif//_DEBUG

        return(new MaterialParameters(desc_manager,desc_set_type,ds));
    }
}//namespace

Material *GPUDevice::CreateMaterial(const UTF8String &mtl_name,ShaderModuleMap *shader_maps,MaterialDescriptorManager *desc_manager,VertexInput *vi)
{
    const int shader_count=shader_maps->GetCount();

    if(shader_count<1)
        return(nullptr);

    PipelineLayoutData *pld=CreatePipelineLayoutData(attr->device,desc_manager);
    
    if(!pld)
    {
        delete shader_maps;
        SAFE_CLEAR(desc_manager);
        return(nullptr);
    }

    MaterialData *data=new MaterialData;

    data->name          =mtl_name;
    data->shader_maps   =shader_maps;
    data->desc_manager  =desc_manager;
    data->vertex_input  =vi;

    CreateShaderStageList(data->shader_stage_list,shader_maps);

    data->pipeline_layout_data=pld;

    if(desc_manager)
    {
        ENUM_CLASS_FOR(DescriptorSetType,int,dst)
        {
            if(desc_manager->hasSet((DescriptorSetType)dst))
                data->mp_array[dst]=CreateMP(attr->device,attr->desc_pool,desc_manager,pld,(DescriptorSetType)dst);
            else
                data->mp_array[dst]=nullptr;
        }
    }
    else
        hgl_zero(data->mp_array);

    return(new Material(data));
}
VK_NAMESPACE_END
