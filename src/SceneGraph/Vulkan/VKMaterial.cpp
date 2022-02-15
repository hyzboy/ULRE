#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterialDescriptorSets.h>
#include"VKPipelineLayoutData.h"
VK_NAMESPACE_BEGIN
MaterialData::~MaterialData()
{
    SAFE_CLEAR(mp.m);
    SAFE_CLEAR(mp.r);
    SAFE_CLEAR(mp.g);

    delete shader_maps;
    SAFE_CLEAR(mds);
}

Material::~Material()
{
    delete data->pipeline_layout_data;
    delete data;
}

const VkPipelineLayout Material::GetPipelineLayout()const
{
    return data->pipeline_layout_data->pipeline_layout;
}
VK_NAMESPACE_END
