#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKMaterialDescriptorManager.h>
#include<hgl/vk/VKVertexInput.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineLayoutData.h>
#include<hgl/vk/VKMaterialResourceDomain.h>        // Phase 5: default_domain
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/vk/UBOAccessor.h>

namespace hgl::graph{

void ReleaseVertexInput(VertexInput *vi);

ShaderProgram::ShaderProgram(const AnsiString &n,const mtl::MaterialCreateInfo *mci)
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
    default_domain=nullptr;
    mi_max_count=0;

    has_l2w_matrix=mci->HasLocalToWorld();
}

ShaderProgram::~ShaderProgram()
{
    SAFE_CLEAR(default_domain);   // Phase 5: dtor frees lazy default domain

    ReleaseVertexInput(vertex_input);
    delete shader_maps;             //不用SAFE_CLEAR是因为这个一定会有
    SAFE_CLEAR(desc_manager);
    SAFE_CLEAR(pipeline_layout_data);

    for(auto &mp:mp_array)
        SAFE_CLEAR(mp);
}

const VkPipelineLayout ShaderProgram::GetPipelineLayout()const
{
    if(!pipeline_layout_data)
        return VK_NULL_HANDLE;
    return pipeline_layout_data->pipeline_layout;
}

const bool ShaderProgram::hasSet(const DescriptorSetType &dst)const
{
    if(!desc_manager)
        return false;
    return desc_manager->hasSet(dst);
}

const VIL *ShaderProgram::GetDefaultVIL()const
{
    if(!vertex_input)
        return nullptr;
    return vertex_input->GetDefaultVIL();
}

VIL *ShaderProgram::CreateVIL(const VILConfig *format_map)
{
    if(!vertex_input)
        return nullptr;
    return vertex_input->CreateVIL(format_map);
}

bool ShaderProgram::Release(VIL *vil)
{
    if(!vertex_input)
        return false;
    return vertex_input->Release(vil);
}

const uint ShaderProgram::GetVILCount()
{
    if(!vertex_input)
        return 0;
    return vertex_input->GetInstanceCount();
}

bool ShaderProgram::BindUBO(const mtl::UBODescriptorSemantic semantic,const IGPUBuffer *gpu,bool dynamic)
{
    const DescriptorSetType type = mtl::GetExpectedSetType(semantic);
    MaterialParameters *mp=GetMP(type);

    if(!mp)
        return(false);

    return mp->BindUBO(semantic,gpu,dynamic);
}

bool ShaderProgram::BindUBO(const UBOAccessorBase *ubo,bool dynamic)
{
    if(!ubo)
        return false;

    return BindUBO(ubo->GetSemantic(),ubo->GetGPUBuffer(),dynamic);
}

bool ShaderProgram::BindSSBO(const mtl::SSBODescriptorSemantic semantic,const IGPUBuffer *gpu,bool dynamic)
{
    const DescriptorSetType type = mtl::GetExpectedSetType(semantic);
    MaterialParameters *mp=GetMP(type);

    if(!mp)
        return(false);

    return mp->BindSSBO(semantic,gpu,dynamic);
}

bool ShaderProgram::BindTexture(const DescriptorSetType &type,mtl::SamplerSlot slot,Texture *tex)
{
    MaterialParameters *mp = GetMP(type);

    if(!mp)
        return(false);

    return mp->BindTexture(slot,tex);
}

bool ShaderProgram::BindTextureSampler(const DescriptorSetType &type,mtl::SamplerSlot slot,Texture *tex,Sampler *sampler)
{
    MaterialParameters *mp=GetMP(type);

    if(!mp)
        return(false);

    return mp->BindTextureSampler(slot,tex,sampler);
}

void ShaderProgram::Update()
{
    for(auto &mp:mp_array)
    {
        if(mp)
            mp->Update();
    }
}
}//namespace hgl::graph
