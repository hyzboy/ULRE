#include<hgl/shadergen/ShaderSetCompiler.h>
#include<hgl/shadergen/ShaderStageCompiler.h>

namespace hgl::graph::mtl
{
bool ShaderSetCompiler::Compile(ShaderStageMap &shader_map)
{
    if(shader_map.IsEmpty())
        return false;

    for(auto &kv:shader_map)
    {
        if(!ShaderStageCompiler::Compile(kv.second))
            return false;
    }

    return true;
}
}//namespace hgl::graph::mtl
