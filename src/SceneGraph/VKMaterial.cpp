#include<hgl/vk/VKMaterial.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKMaterialDescriptorManager.h>
#include<hgl/vk/VKVertexInput.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineLayoutData.h>
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/vk/UBOAccessor.h>

namespace hgl::graph{

void ReleaseVertexInput(VertexInput *vi);

Material::Material(const AnsiString &n,const mtl::MaterialCreateInfo *mci)
{
    name=n;
    geometry=mci->GetPrimitiveType();
    binding_contract=mci->GetBindingContract();

    vertex_input=nullptr;
    shader_maps=new ShaderModuleMap;
    desc_manager=nullptr;
    pipeline_layout_data=nullptr;

    mem_zero(mp_array);

    mi_data_bytes=0;
    mi_max_count=0;
    mi_schema=mtl::ShaderDataSchema::None;

    has_l2w_matrix=mci->HasLocalToWorld();
}

Material::~Material()
{
    ReleaseVertexInput(vertex_input);
    delete shader_maps;             //不用SAFE_CLEAR是因为这个一定会有
    SAFE_CLEAR(desc_manager);
    SAFE_CLEAR(pipeline_layout_data);

    for(auto &mp:mp_array)
        SAFE_CLEAR(mp);
}

const VkPipelineLayout Material::GetPipelineLayout()const
{
    if(!pipeline_layout_data)
        return VK_NULL_HANDLE;
    return pipeline_layout_data->pipeline_layout;
}

const bool Material::hasSet(const DescriptorSetType &dst)const
{
    if(!desc_manager)
        return false;
    return desc_manager->hasSet(dst);
}

const VIL *Material::GetDefaultVIL()const
{
    if(!vertex_input)
        return nullptr;
    return vertex_input->GetDefaultVIL();
}

VIL *Material::CreateVIL(const VILConfig *format_map)
{
    if(!vertex_input)
        return nullptr;
    return vertex_input->CreateVIL(format_map);
}

bool Material::Release(VIL *vil)
{
    if(!vertex_input)
        return false;
    return vertex_input->Release(vil);
}

const uint Material::GetVILCount()
{
    if(!vertex_input)
        return 0;
    return vertex_input->GetInstanceCount();
}

bool Material::BindUBO(const mtl::UBODescriptorSemantic semantic,const IGPUBuffer *gpu,bool dynamic)
{
    const DescriptorSetType type = mtl::GetExpectedSetType(semantic);
    MaterialParameters *mp=GetMP(type);

    if(!mp)
        return(false);

    return mp->BindUBO(semantic,gpu,dynamic);
}

bool Material::BindUBO(const UBOAccessorBase *ubo,bool dynamic)
{
    if(!ubo)
        return false;

    return BindUBO(ubo->GetSemantic(),ubo->GetGPUBuffer(),dynamic);
}

bool Material::BindSSBO(const mtl::SSBODescriptorSemantic semantic,const IGPUBuffer *gpu,bool dynamic)
{
    const DescriptorSetType type = mtl::GetExpectedSetType(semantic);
    MaterialParameters *mp=GetMP(type);

    if(!mp)
        return(false);

    return mp->BindSSBO(semantic,gpu,dynamic);
}

bool Material::BindTexture(const DescriptorSetType &type,mtl::SamplerSlot slot,Texture *tex)
{
    MaterialParameters *mp = GetMP(type);

    if(!mp)
        return(false);

    return mp->BindTexture(slot,tex);
}

bool Material::BindTextureSampler(const DescriptorSetType &type,mtl::SamplerSlot slot,Texture *tex,Sampler *sampler)
{
    MaterialParameters *mp=GetMP(type);

    if(!mp)
        return(false);

    return mp->BindTextureSampler(slot,tex,sampler);
}

void Material::Update()
{
    for(auto &mp:mp_array)
    {
        if(mp)
            mp->Update();
    }
}
}//namespace hgl::graph
