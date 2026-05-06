#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderStageIO.h>
#include<hgl/shadergen/MaterialDescriptorDB.h>
#include<hgl/mtl/UBOCommon.h>
#include<string>
#include<cassert>

#include"GLSLCompiler.h"

namespace hgl{namespace graph{

void ShaderCreateInfo::SPVDataDeleter::operator()(SPVData *ptr) const noexcept
{
    if(ptr)
        FreeSPVData(ptr);
}

static const char *GetShaderStageNameByStage(const ShaderStage stage)
{
    switch (stage)
    {
        case ShaderStage::Vertex: return "Vertex";
        case ShaderStage::Fragment: return "Fragment";
        case ShaderStage::Compute: return "Compute";
        case ShaderStage::ClusterCulling: return "ClusterCulling";
        default: return "Unknown";
    }
}

ShaderCreateInfo::ShaderCreateInfo(ShaderStageIO *s,MaterialDescriptorDB *m)
    :ShaderCreateInfo(std::unique_ptr<ShaderStageIO>(s),m)
{
}

ShaderCreateInfo::ShaderCreateInfo(std::unique_ptr<ShaderStageIO> s,MaterialDescriptorDB *m)
    :shader_stage(s ? s->GetShaderStage() : ShaderStage::Vertex)
    ,stage_io(std::move(s))
    ,descriptor_db(m)
    ,spv_data(nullptr)
{
    assert(stage_io&&"ShaderCreateInfo requires non-null ShaderStageIO");
}

ShaderCreateInfo::~ShaderCreateInfo()=default;

bool ShaderCreateInfo::CompileFinalGLSLToSPV()
{
    if(final_shader.empty())
    {
        // Keep SPV cache coherent with shader text state.
        // Empty source means no valid SPV should remain observable.
        spv_data.reset();
        return(false);
    }

    if(!CompileToSPV())
        return(false);

    return(true);
}

bool ShaderCreateInfo::CompileToSPV()
{
    spv_data.reset();
    spv_data.reset(CompileShader(uint32_t(shader_stage),final_shader.c_str()));

    if(!spv_data.get())
        return(false);

    return(true);
}

const uint32 *ShaderCreateInfo::GetSPVData()const
{
    const SPVData *data=spv_data.get();
    return data?data->spv_data:nullptr;
}

const size_t ShaderCreateInfo::GetSPVSize()const
{
    const SPVData *data=spv_data.get();
    return data?data->spv_length:0;
}
}}//namespace hgl::graph
