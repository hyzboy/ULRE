#include<hgl/shadergen/ShaderBuildPipeline.h>

namespace hgl::graph
{
ShaderGenResult<ShaderBuildResult> ShaderBuildPipeline::Build(const mtl::MaterialCreateConfig &,
                                                              const mtl::contract::PhysicalDeviceProfileLite *)
{
    ShaderGenResult<ShaderBuildResult> result{};
    result.success=false;
    result.diagnostics.push_back({ShaderGenSeverity::Warning,
                                  ShaderGenErrorCode::InternalError,
                                  ShaderStage::Vertex,
                                  "ShaderBuildPipeline",
                                  "skeleton implementation: pipeline build path not wired yet"});
    return result;
}
}//namespace hgl::graph
