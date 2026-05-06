#include<hgl/shadergen/ShaderStageCompiler.h>
#include<hgl/shadergen/ShaderCreateInfo.h>

namespace hgl::graph::mtl
{
bool ShaderStageCompiler::Compile(ShaderCreateInfo *shader_create_info)
{
    if(!shader_create_info)
        return false;

    return shader_create_info->CompileFinalGLSLToSPV();
}
}//namespace hgl::graph::mtl
