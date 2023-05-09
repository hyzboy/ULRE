#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterialDescriptorManager.h>
#include<hgl/graph/VKVertexInput.h>
#include"VKPipelineLayoutData.h"
VK_NAMESPACE_BEGIN
MaterialData::~MaterialData()
{
    for(int i=0;i<DESCRIPTOR_SET_TYPE_COUNT;i++)
        if(mp_array[i])
            delete mp_array[i];

    delete shader_maps;
    SAFE_CLEAR(desc_manager);

    delete vertex_input;
}

Material::Material(MaterialData *md):data(md)
{
    mi_size=0;
    mi_count=0;
}

Material::~Material()
{
    delete[] data->mi_data;

    delete data->pipeline_layout_data;
    delete data;
}

const VkPipelineLayout Material::GetPipelineLayout()const
{
    return data->pipeline_layout_data->pipeline_layout;
}

const bool Material::hasSet(const DescriptorSetType &dst)const
{
    return data->desc_manager->hasSet(dst);
}

VIL *Material::CreateVIL(const VILConfig *format_map)
{
    return data->vertex_input->CreateVIL(format_map);
}

bool Material::Release(VIL *vil)
{
    return data->vertex_input->Release(vil);
}

const uint Material::GetVILCount()
{
    return data->vertex_input->GetInstanceCount();
}
VK_NAMESPACE_END
