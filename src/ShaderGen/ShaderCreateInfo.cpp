#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>
#include<hgl/shadergen/MaterialDescriptorInfo.h>
#include<hgl/graph/ShaderBufferSources.h>
#include<string>

#include"GLSLCompiler.h"
#include<hgl/shadergen/ShaderArtifactContract.h>
#include<cstring>
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
    cached_spv_data.Clear();
    if(spv_data)
    {
        FreeSPVData(spv_data);
        spv_data=nullptr;
    }

    spv_data=CompileShader(uint32_t(shader_stage),final_shader.c_str());

    if(!spv_data)
        return(false);

    return(true);
}

bool ShaderCreateInfo::SetCachedSPVData(const void *data,const size_t byte_size)
{
    uint32 magic=0;
    if(data&&byte_size>=sizeof(uint32))
        std::memcpy(&magic,data,sizeof(magic));
    if(!data
     ||byte_size<sizeof(uint32)
     ||byte_size%sizeof(uint32)!=0
     ||magic!=mtl::ShaderArtifactSPVMagic)
        return(false);

    if(spv_data)
    {
        FreeSPVData(spv_data);
        spv_data=nullptr;
    }

    cached_spv_data.Resize(
        static_cast<int>(byte_size/sizeof(uint32)));
    std::memcpy(cached_spv_data.GetData(),data,byte_size);
    return(true);
}

const uint32 *ShaderCreateInfo::GetSPVData()const
{
    if(spv_data)
        return(spv_data->spv_data);

    return cached_spv_data.IsEmpty()
        ?nullptr
        :cached_spv_data.GetData();
}

const size_t ShaderCreateInfo::GetSPVSize()const
{
    return spv_data
        ?spv_data->spv_length
        :static_cast<size_t>(cached_spv_data.GetCount())*sizeof(uint32);
}
}}//namespace hgl::graph
