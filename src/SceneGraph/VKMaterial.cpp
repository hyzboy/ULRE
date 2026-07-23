#include<hgl/vk/VKMaterial.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKMaterialDescriptorManager.h>
#include<hgl/vk/VKVertexInput.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/vk/pipeline/VKPipelineLayoutData.h>
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>

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

VIL *Material::CreateVIL(const GeometryVertexFormat &geometry_vertex_format)
{
    if(geometry_vertex_format.GetCount() <= 0)
        return CreateVIL((const VILConfig *)nullptr);

    VILConfig vil_config;
    for(uint32_t i=0;i<geometry_vertex_format.GetCount();++i)
    {
        const GeometryVertexAttributeFormat *attribute = geometry_vertex_format.Get(i);
        if(!attribute)
            continue;

        vil_config.Add(attribute->semantic, VAConfig(attribute->format, VK_VERTEX_INPUT_RATE_VERTEX));
    }

    return CreateVIL(&vil_config);
}

bool Material::Release(VIL *vil)
{
    return vertex_input->Release(vil);
}

const uint Material::GetVILCount()
{
    return vertex_input->GetInstanceCount();
}

bool Material::BindUBO(const DescriptorSetType &type,const AnsiString &name,const IGPUBuffer *gpu,bool dynamic)
{
    MaterialParameters *mp=GetMP(type);

    if(!mp)
        return(false);

    return mp->BindUBO(name,gpu,dynamic);
}

bool Material::BindSSBO(const DescriptorSetType &type,const AnsiString &name,const IGPUBuffer *gpu,bool dynamic)
{
    MaterialParameters *mp=GetMP(type);

    if(!mp)
        return(false);

    return mp->BindSSBO(name,gpu,dynamic);
}

bool Material::BindTexture(const DescriptorSetType &type,const AnsiString &name,Texture *tex)
{
    MaterialParameters *mp = GetMP(type);

    if(!mp)
        return(false);

    return mp->BindTexture(name,tex);
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
}//namespace hgl::graph
