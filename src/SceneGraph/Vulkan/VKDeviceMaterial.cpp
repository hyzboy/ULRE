#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialDescriptorManager.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKDescriptorSet.h>
#include<hgl/graph/VKShaderModuleMap.h>
#include<hgl/graph/VKVertexInputLayout.h>
#include"VKPipelineLayoutData.h"

VK_NAMESPACE_BEGIN
DescriptorSet *GPUDevice::CreateDS(const PipelineLayoutData *pld,const DescriptorSetType &type)const
{
    RANGE_CHECK_RETURN_NULLPTR(type);

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

    return(new DescriptorSet(attr->device,binding_count,pld->pipeline_layout,desc_set));
}

MaterialParameters *GPUDevice::CreateMP(const MaterialDescriptorManager *mds,const PipelineLayoutData *pld,const DescriptorSetType &desc_set_type)
{
    if(!mds||!pld)return(nullptr);
    if(!RangeCheck<DescriptorSetType>(desc_set_type))
        return(nullptr);

    DescriptorSet *ds=CreateDS(pld,desc_set_type);

    if(!ds)return(nullptr);

#ifdef _DEBUG
    const UTF8String addr_string=HexToString<char,uint64_t>((uint64_t)(ds->GetDescriptorSet()));

    LOG_INFO(U8_TEXT("Create [DescriptSets:")+addr_string+("] OK! Material Name: \"")+mds->GetMaterialName()+U8_TEXT("\" Type: ")+GetDescriptorSetTypeName(desc_set_type));
#endif//_DEBUG

    return(new MaterialParameters(mds,desc_set_type,ds));
}

MaterialParameters *GPUDevice::CreateMP(Material *mtl,const DescriptorSetType &desc_set_type)
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

Material *GPUDevice::CreateMaterial(const UTF8String &mtl_name,ShaderModuleMap *shader_maps,MaterialDescriptorManager *mds,VertexInput *vi)
{
    const int shader_count=shader_maps->GetCount();

    if(shader_count<1)
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
    data->vertex_input  =vi;

    CreateShaderStageList(data->shader_stage_list,shader_maps);

    data->pipeline_layout_data=pld;

    if(mds)
    {
        ENUM_CLASS_FOR(DescriptorSetType,int,dst)
        {
            if(mds->hasSet((DescriptorSetType)dst))
                data->mp_array[dst]=CreateMP(mds,pld,(DescriptorSetType)dst);
            else
                data->mp_array[dst]=nullptr;
        }
    }
    else
        hgl_zero(data->mp_array);

    return(new Material(data));
}
VK_NAMESPACE_END
