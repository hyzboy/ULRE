#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>
#include<hgl/shadergen/MaterialDescriptorInfo.h>
#include<hgl/mtl/UBOCommon.h>
#include<string>

#include"GLSLCompiler.h"

namespace hgl{namespace graph{

static const char *GetShaderStageNameByStage(const ShaderStage stage)
{
    switch (stage)
    {
        case ShaderStage::Vertex: return "Vertex";
        case ShaderStage::Fragment: return "Fragment";
        case ShaderStage::Compute: return "Compute";
        case ShaderStage::Task: return "Task";
        case ShaderStage::Mesh: return "Mesh";
        case ShaderStage::ClusterCulling: return "ClusterCulling";
        default: return "Unknown";
    }
}

ShaderCreateInfo::ShaderCreateInfo(ShaderDescriptorInfo *s,MaterialDescriptorInfo *m)
{
    sdi=s;
    descriptor_db=m;
    shader_stage=s->GetShaderStage();
    spv_data=nullptr;
}

ShaderCreateInfo::~ShaderCreateInfo()
{
    delete sdi;

    if(spv_data)
        FreeSPVData(spv_data);
}

bool ShaderCreateInfo::CompileFinalGLSLToSPV()
{
    if(final_shader.empty())
        return(false);

#ifdef _DEBUG
    std::string log_text=GetShaderStageNameByStage(shader_stage);
    log_text+=" shader (direct): \n";
    log_text+=final_shader;
    LogInfo(log_text.c_str());
#endif

    if(!CompileToSPV())
        return(false);

    return(true);
}

bool ShaderCreateInfo::CompileToSPV()
{
    spv_data=CompileShader(uint32_t(shader_stage),final_shader.c_str());

    if(!spv_data)
        return(false);

    return(true);
}

const uint32 *ShaderCreateInfo::GetSPVData()const
{
    return spv_data?spv_data->spv_data:nullptr;
}

const size_t ShaderCreateInfo::GetSPVSize()const
{
    return spv_data?spv_data->spv_length:0;
}
}}//namespace hgl::graph
