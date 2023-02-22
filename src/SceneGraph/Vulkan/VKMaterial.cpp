#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterialDescriptorSets.h>
#include"VKPipelineLayoutData.h"
VK_NAMESPACE_BEGIN
MaterialData::~MaterialData()
{
    for(int i=0;i<int(DescriptorSetType::RANGE_SIZE);i++)
        if(mp_array[i])
            delete mp_array[i];

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

const bool Material::hasSet(const DescriptorSetType &dst)const
{
    return data->mds->hasSet(dst);
}
VK_NAMESPACE_END
