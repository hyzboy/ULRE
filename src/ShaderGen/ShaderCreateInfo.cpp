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
        case ShaderStage::TessControl: return "TessControl";
        case ShaderStage::TessEval: return "TessEval";
        case ShaderStage::Geometry: return "Geometry";
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

void ShaderCreateInfo::AddStruct(const std::string &name)
{
    return GetShaderDescriptorInfo()->AddStruct(name.c_str());
}

bool ShaderCreateInfo::AddUBO(DescriptorSetType type,const UBODescriptor *sd)
{
    return GetShaderDescriptorInfo()->AddUBO(type,sd);
}

bool ShaderCreateInfo::AddSSBO(DescriptorSetType type,const SSBODescriptor *sd)
{
    return GetShaderDescriptorInfo()->AddSSBO(type,sd);
}

bool ShaderCreateInfo::AddTexture(DescriptorSetType type,const TextureDescriptor *sd)
{
    return GetShaderDescriptorInfo()->AddTexture(type,sd);
}

bool ShaderCreateInfo::AddTextureSampler(DescriptorSetType type,const TextureSamplerDescriptor *sd)
{
    return GetShaderDescriptorInfo()->AddTextureSampler(type,sd);
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
