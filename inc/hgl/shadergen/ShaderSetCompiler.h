#pragma once

#include<hgl/shadergen/ShaderStageMap.h>

namespace hgl::graph::mtl
{
class ShaderSetCompiler
{
public:
    static bool Compile(ShaderStageMap &shader_map);
};
}//namespace hgl::graph::mtl
