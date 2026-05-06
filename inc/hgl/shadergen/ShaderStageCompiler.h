#pragma once

namespace hgl::graph
{
class ShaderCreateInfo;
}

namespace hgl::graph::mtl
{
class ShaderStageCompiler
{
public:
    static bool Compile(ShaderCreateInfo *shader_create_info);
};
}//namespace hgl::graph::mtl
