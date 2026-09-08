#include<hgl/vk/VKShaderProgram.h>
#include<hgl/common/ShaderStageDef.h>
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
    pipeline_layout_data=nullptr;
}

ShaderProgram::~ShaderProgram()
{
    delete shader_maps;             //不用SAFE_CLEAR是因为这个一定会有
    SAFE_CLEAR(pipeline_layout_data);
}

const VkPipelineLayout ShaderProgram::GetPipelineLayout()const
{
    return pipeline_layout_data->pipeline_layout;
}

}//namespace hgl::graph
