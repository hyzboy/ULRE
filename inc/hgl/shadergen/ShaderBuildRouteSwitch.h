#pragma once

namespace hgl::graph
{
enum class ShaderBuildRoute
{
    LegacyMaterialCreateInfo = 0,
    Pipeline = 1
};

struct ShaderBuildSwitchConfig
{
    bool enable_pipeline = false;
    bool allow_fallback_to_legacy = true;
};

ShaderBuildRoute ResolveShaderBuildRoute(const ShaderBuildSwitchConfig *config=nullptr);
const char *GetShaderBuildRouteName(const ShaderBuildRoute route);
}//namespace hgl::graph
