#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterialDescriptorManager.h>
#include<hgl/graph/VKVertexInput.h>
#include"VKPipelineLayoutData.h"
#include<hgl/type/ActiveMemoryBlockManager.h>

VK_NAMESPACE_BEGIN

void ReleaseVertexInput(VertexInput *vi);

Material::Material(const AnsiString &n)
{
    name=n;

    vertex_input=nullptr;
    shader_maps=new ShaderModuleMap;
    desc_manager=nullptr;
    pipeline_layout_data=nullptr;

    hgl_zero(mp_array);

    mi_data_bytes=0;
    mi_data_manager=nullptr;
}

Material::~Material()
{
    SAFE_CLEAR(mi_data_manager);

    ReleaseVertexInput(vertex_input);
    delete shader_maps;             //不用SAFE_CLEAR是因为这个一定会有
    SAFE_CLEAR(desc_manager);
    SAFE_CLEAR(pipeline_layout_data);

    for(int i=0;i<DESCRIPTOR_SET_TYPE_COUNT;i++)
        SAFE_CLEAR(mp_array[i]);
}

const VkPipelineLayout Material::GetPipelineLayout()const
{
    return pipeline_layout_data->pipeline_layout;
}

const bool Material::hasSet(const DescriptorSetType &dst)const
{
    return desc_manager->hasSet(dst);
}

const VIL *Material::GetDefaultVIL()const
{
    return vertex_input->GetDefaultVIL();
}

VIL *Material::CreateVIL(const VILConfig *format_map)
{
    return vertex_input->CreateVIL(format_map);
}

bool Material::Release(VIL *vil)
{
    return vertex_input->Release(vil);
}

const uint Material::GetVILCount()
{
    return vertex_input->GetInstanceCount();
}

bool Material::BindUBO(const DescriptorSetType &type,const AnsiString &name,DeviceBuffer *ubo,bool dynamic)
{
    MaterialParameters *mp=GetMP(type);
        
    if(!mp)
        return(false);

    if(!mp->BindUBO(name,ubo,dynamic))return(false);

    mp->Update();
    return(true);
}

bool Material::BindSSBO(const DescriptorSetType &type,const AnsiString &name,DeviceBuffer *ubo,bool dynamic)
{
    MaterialParameters *mp=GetMP(type);
        
    if(!mp)
        return(false);

    if(!mp->BindSSBO(name,ubo,dynamic))return(false);

    mp->Update();
    return(true);
}

bool Material::BindImageSampler(const DescriptorSetType &type,const AnsiString &name,Texture *tex,Sampler *sampler)
{
    MaterialParameters *mp=GetMP(type);
        
    if(!mp)
        return(false);

    if(!mp->BindImageSampler(name,tex,sampler))return(false);

    mp->Update();
    return(true);
}
VK_NAMESPACE_END
