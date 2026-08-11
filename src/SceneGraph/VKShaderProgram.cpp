#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKMaterialDescriptorManager.h>
#include<hgl/vk/VKVertexInput.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/vk/pipeline/VKPipelineLayoutData.h>
#include<hgl/shadergen/ShaderBuildContext.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>

namespace hgl::graph{

void ReleaseVertexInput(VertexInput *vi);

ShaderProgram::ShaderProgram(const AnsiString &n,const shadergen::ShaderBuildContext *mci)
{
    name=n;
    geometry=mci->GetPrimitiveType();
    material_resource_layout=mci->GetShaderResourceSchema();
    program_key=mci->GetProgramLink().BuildKey();

    vertex_input=nullptr;
    shader_maps=new ShaderModuleMap;
    desc_manager=nullptr;
    pipeline_layout_data=nullptr;

    mem_zero(mp_array);

    has_l2w_matrix=mci->HasLocalToWorld();
}

ShaderProgram::~ShaderProgram()
{
    ReleaseVertexInput(vertex_input);
    delete shader_maps;             //不用SAFE_CLEAR是因为这个一定会有
    SAFE_CLEAR(desc_manager);
    SAFE_CLEAR(pipeline_layout_data);

    for(auto &mp:mp_array)
        SAFE_CLEAR(mp);
}

const VkPipelineLayout ShaderProgram::GetPipelineLayout()const
{
    return pipeline_layout_data->pipeline_layout;
}

const bool ShaderProgram::hasSet(const DescriptorSetType &dst)const
{
    return desc_manager->hasSet(dst);
}

const VIL *ShaderProgram::GetDefaultVIL()const
{
    return vertex_input->GetDefaultVIL();
}

VIL *ShaderProgram::CreateVIL(const VILConfig *format_map)
{
    return vertex_input->CreateVIL(format_map);
}

VIL *ShaderProgram::CreateVIL(const GeometryVertexFormat &geometry_vertex_format)
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

bool ShaderProgram::Release(VIL *vil)
{
    return vertex_input->Release(vil);
}

const uint ShaderProgram::GetVILCount()
{
    return vertex_input->GetInstanceCount();
}

bool ShaderProgram::BindUBO(const DescriptorSetType &type,const AnsiString &name,const IGPUBuffer *gpu,bool dynamic)
{
    MaterialParameters *mp=GetMP(type);

    if(!mp)
        return(false);

    return mp->BindUBO(name,gpu,dynamic);
}

bool ShaderProgram::BindSSBO(const DescriptorSetType &type,const AnsiString &name,const IGPUBuffer *gpu,bool dynamic)
{
    MaterialParameters *mp=GetMP(type);

    if(!mp)
        return(false);

    return mp->BindSSBO(name,gpu,dynamic);
}

bool ShaderProgram::BindTexture(const DescriptorSetType &type,const AnsiString &name,Texture *tex)
{
    MaterialParameters *mp = GetMP(type);

    if(!mp)
        return(false);

    return mp->BindTexture(name,tex);
}

bool ShaderProgram::BindTextureSampler(const DescriptorSetType &type,const AnsiString &name,Texture *tex,Sampler *sampler)
{
    MaterialParameters *mp=GetMP(type);

    if(!mp)
        return(false);

    return mp->BindTextureSampler(name,tex,sampler);
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
