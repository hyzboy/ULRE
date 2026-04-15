#include<hgl/vk/VKMaterialTemplate.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKMaterialDescriptorManager.h>
#include<hgl/vk/VKVertexInput.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineLayoutData.h>
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/vk/UBOAccessor.h>
#include<hgl/graph/module/DescriptorTimingDiagnostics.h>
#include<cstdio>

namespace hgl::graph{

void ReleaseVertexInput(VertexInput *vi);

MaterialTemplate::MaterialTemplate(const std::string &n,const mtl::MaterialCreateInfo *mci)
{
    name=n;
    geometry=mci->GetPrimitiveType();
    binding_contract=mci->GetBindingContract();

    vertex_input=nullptr;
    shader_maps=new ShaderModuleMap;
    desc_manager=nullptr;
    pipeline_layout_data=nullptr;

    mem_zero(mp_array);

    required_instance_layout = mtl::InstanceDataLayout::None;
    instance_max_count=0;

    has_l2w_matrix=mci->HasLocalToWorld();
}

MaterialTemplate::~MaterialTemplate()
{
    ReleaseVertexInput(vertex_input);
    delete shader_maps;             //不用SAFE_CLEAR是因为这个一定会有
    SAFE_CLEAR(desc_manager);
    SAFE_CLEAR(pipeline_layout_data);

    for(auto &mp:mp_array)
        SAFE_CLEAR(mp);
}

const VkPipelineLayout MaterialTemplate::GetPipelineLayout()const
{
    if(!pipeline_layout_data)
        return VK_NULL_HANDLE;
    return pipeline_layout_data->pipeline_layout;
}

const bool MaterialTemplate::hasSet(const DescriptorSetType &dst)const
{
    if(!desc_manager)
        return false;
    return desc_manager->hasSet(dst);
}

const VIL *MaterialTemplate::GetDefaultVIL()const
{
    if(!vertex_input)
        return nullptr;
    return vertex_input->GetDefaultVIL();
}

VIL *MaterialTemplate::CreateVIL(const VILConfig *format_map)
{
    if(!vertex_input)
        return nullptr;
    return vertex_input->CreateVIL(format_map);
}

bool MaterialTemplate::Release(VIL *vil)
{
    if(!vertex_input)
        return false;
    return vertex_input->Release(vil);
}

const uint MaterialTemplate::GetVILCount()
{
    if(!vertex_input)
        return 0;
    return vertex_input->GetInstanceCount();
}

bool MaterialTemplate::BindUBO(const mtl::UBODescriptorSemantic semantic,const IGPUBuffer *gpu,bool dynamic)
{
    const DescriptorSetType type = mtl::GetExpectedSetType(semantic);
    MaterialParameters *mp=GetMP(type);

    if(!mp)
        return(false);

    return mp->BindUBO(semantic,gpu,dynamic);
}

bool MaterialTemplate::BindUBO(const UBOAccessorBase *ubo,bool dynamic)
{
    if(!ubo)
        return false;

    return BindUBO(ubo->GetSemantic(),ubo->GetGPUBuffer(),dynamic);
}

bool MaterialTemplate::BindSSBO(const mtl::SSBODescriptorSemantic semantic,const IGPUBuffer *gpu,bool dynamic)
{
    const DescriptorSetType type = mtl::GetExpectedSetType(semantic);
    MaterialParameters *mp=GetMP(type);

    if(!mp)
    {
        std::fprintf(stderr,
            "[MaterialTemplate::BindSSBO] FAILED: no MaterialParameters for set=%u semantic=%u material=%s mtl=0x%llX gpu=0x%llX dynamic=%d\n",
            static_cast<unsigned>(type),
            static_cast<unsigned>(semantic),
            GetName().c_str(),
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(this)),
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(gpu)),
            dynamic ? 1 : 0);
        return(false);
    }

    const bool ok = mp->BindSSBO(semantic,gpu,dynamic);
    if(!ok)
    {
        std::fprintf(stderr,
            "[MaterialTemplate::BindSSBO] FAILED: mp->BindSSBO returned false set=%u semantic=%u material=%s mtl=0x%llX mp=0x%llX gpu=0x%llX dynamic=%d\n",
            static_cast<unsigned>(type),
            static_cast<unsigned>(semantic),
            GetName().c_str(),
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(this)),
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(mp)),
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(gpu)),
            dynamic ? 1 : 0);
    }

    return ok;
}

bool MaterialTemplate::BindTexture(const DescriptorSetType &type,mtl::SamplerSlot slot,Texture *tex)
{
    MaterialParameters *mp = GetMP(type);

    if(!mp)
        return(false);

    return mp->BindTexture(slot,tex);
}

bool MaterialTemplate::BindTextureSampler(const DescriptorSetType &type,mtl::SamplerSlot slot,Texture *tex,Sampler *sampler)
{
    MaterialParameters *mp=GetMP(type);

    if(!mp)
        return(false);

    return mp->BindTextureSampler(slot,tex,sampler);
}

void MaterialTemplate::Update()
{
    LOG_DESC_TIMING("MaterialTemplate::Update() ENTRY mtl=%p name=%s", (void*)this, GetName().c_str());
    for(auto &mp:mp_array)
    {
        if(mp)
        {
            LOG_DESC_TIMING_VERBOSE("  Updating mp=%p", (void*)mp);
            mp->Update();
        }
    }
    LOG_DESC_TIMING("MaterialTemplate::Update() EXIT");
}
}//namespace hgl::graph
