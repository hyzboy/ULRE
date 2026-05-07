#pragma once

#include<hgl/shadergen/ShaderGenDiagnostic.h>
#include<hgl/shadergen/ShaderStageMap.h>

namespace hgl::graph::mtl
{
class ShaderSetCompiler
{
public:
    static ShaderGenStatus TryCompile(ShaderStageMap &shader_map,std::vector<ShaderGenDiagnostic> *diagnostics=nullptr);
    static bool Compile(ShaderStageMap &shader_map);
};
}//namespace hgl::graph::mtl
