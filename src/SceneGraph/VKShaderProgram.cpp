#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKMaterialDescriptorManager.h>
#include<hgl/vk/VKVertexInput.h>
#include<hgl/vk/pipeline/VKPipelineLayoutData.h>
#include<hgl/mtl/ShaderBuildContext.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>

namespace hgl::graph{

ShaderProgram::ShaderProgram(const AnsiString &n,const mtl::ShaderBuildContext *ctx)
{
    name=n;
    geometry=ctx->GetPrimitiveType();
    shader_resource_schema=ctx->GetShaderResourceSchema();
    program_key=ctx->GetProgramLink().BuildKey();

    // mesh 化后无 VBO 顶点输入布局（VS 遗留 vertex_input 已删）
    shader_maps=new ShaderModuleMap;
    desc_manager=nullptr;
    pipeline_layout_data=nullptr;

    mem_zero(mp_array);

    has_l2w_matrix=ctx->HasLocalToWorld();
}

ShaderProgram::~ShaderProgram()
{
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

bool ShaderProgram::BindSSBO(const DescriptorSetType &type,const AnsiString &name,const VkBuffer buf,const VkDeviceSize offset,const VkDeviceSize range,bool dynamic)
{
    MaterialParameters *mp=GetMP(type);

    if(!mp)
        return(false);

    return mp->BindSSBO(name,buf,offset,range,dynamic);
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
