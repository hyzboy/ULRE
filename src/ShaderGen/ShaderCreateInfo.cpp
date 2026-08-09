#include<hgl/shadergen/ShaderCreateInfo.h>
#include"GLSLCompiler.h"
#include<hgl/shadergen/ShaderArtifactContract.h>
#include<cstring>
namespace hgl{namespace graph{

ShaderCreateInfo::ShaderCreateInfo(const ShaderStage stage)
{
    shader_stage=stage;
    spv_data=nullptr;
}

ShaderCreateInfo::~ShaderCreateInfo()
{
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
