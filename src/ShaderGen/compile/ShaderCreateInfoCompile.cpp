// ShaderCreateInfo 里唯一必须由生成器提供的部分：把 final GLSL 交给 glslang 插件。
// 其余成员（构造/析构/缓存 SPV 装载）在 src/ShaderMaterialData/ShaderCreateInfo.cpp，
// 属纯数据面，发行形态也要用。
#include<hgl/mtl/ShaderCreateInfo.h>
#include"GLSLCompiler.h"
namespace hgl{namespace graph::mtl{

namespace
{
    void FreeSPVDataReleaser(SPVData *spv_data)
    {
        FreeSPVData(spv_data);
    }
}//namespace

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

void InstallShaderCreateInfoSPVReleaser()
{
    ShaderCreateInfo::SetSPVReleaser(FreeSPVDataReleaser);
}

void UninstallShaderCreateInfoSPVReleaser()
{
    ShaderCreateInfo::SetSPVReleaser(nullptr);
}

}}//namespace hgl::graph::mtl
