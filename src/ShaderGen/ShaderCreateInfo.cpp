#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/log/Logger.h>
#include<hgl/shadergen/ShaderStageIO.h>
#include<hgl/shadergen/MaterialDescriptorDB.h>
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

ShaderCreateInfo::ShaderCreateInfo(ShaderStageIO *s,MaterialDescriptorDB *m)
{
    stage_io=s;
    descriptor_db=m;
    shader_stage=s->GetShaderStage();
    spv_data=nullptr;
}

ShaderCreateInfo::~ShaderCreateInfo()
{
    delete stage_io;

    if(spv_data)
        FreeSPVData(spv_data);
}

bool ShaderCreateInfo::CompileFinalGLSLToSPV()
{
    if(final_shader.empty())
        return(false);

    if(!CompileToSPV())
        return(false);

    return(true);
}

bool ShaderCreateInfo::CompileToSPV()
{
    if(spv_data)
    {
        FreeSPVData(spv_data);
        spv_data=nullptr;
    }

    spv_data=CompileShader(uint32_t(shader_stage),final_shader.c_str());

    if(!spv_data)
    {
        GLogError("[ShaderGen][ShaderCreateInfo] CompileToSPV failed: stage=%s",GetShaderStageNameByStage(shader_stage));
        return(false);
    }

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
