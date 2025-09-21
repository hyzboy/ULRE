#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterialDescriptorManager.h>
#include<hgl/graph/VKVertexInput.h>
#include"pipeline/VKPipelineLayoutData.h"
#include<hgl/type/ActiveMemoryBlockManager.h>
#include<hgl/shadergen/MaterialCreateInfo.h>

VK_NAMESPACE_BEGIN

void ReleaseVertexInput(VertexInput *vi);

Material::Material(const AnsiString &n,const mtl::MaterialCreateInfo *mci)
{
    name=n;
    prim=mci->GetPrimitiveType();

    vertex_input=nullptr;
    shader_maps=new ShaderModuleMap;
    desc_manager=nullptr;
    pipeline_layout_data=nullptr;

    hgl_zero(mp_array);

    mi_data_bytes=0;
    mi_data_manager=nullptr;
    mi_max_count=0;

    has_l2w_matrix=mci->hasLocalToWorld();
}

Material::~Material()
{
    SAFE_CLEAR(mi_data_manager);

    ReleaseVertexInput(vertex_input);
    delete shader_maps;             //不用SAFE_CLEAR是因为这个一定会有
    SAFE_CLEAR(desc_manager);
    SAFE_CLEAR(pipeline_layout_data);

    for(auto &mp:mp_array)
        SAFE_CLEAR(mp);
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

    return mp->BindUBO(name,ubo,dynamic);
}

bool Material::BindSSBO(const DescriptorSetType &type,const AnsiString &name,DeviceBuffer *ubo,bool dynamic)
{
    MaterialParameters *mp=GetMP(type);
        
    if(!mp)
        return(false);

    return mp->BindSSBO(name,ubo,dynamic);
}

bool Material::BindTextureSampler(const DescriptorSetType &type,const AnsiString &name,Texture *tex,Sampler *sampler)
{
    MaterialParameters *mp=GetMP(type);
        
    if(!mp)
        return(false);

    return mp->BindTextureSampler(name,tex,sampler);
}

void Material::Update()
{
    for(auto &mp:mp_array)
    {
        if(mp)
            mp->Update();
    }
}
VK_NAMESPACE_END
