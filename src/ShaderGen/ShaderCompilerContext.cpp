#include<hgl/shadergen/ShaderCompilerContext.h>

namespace hgl::graph
{
ShaderCompilerContext::ShaderCompilerContext(const mtl::contract::PhysicalDeviceProfileLite &p)
    : profile(p)
{
}

ShaderGenResult<ShaderBinary> ShaderCompilerContext::Compile(const ShaderCompileRequest &request)
{
    ShaderGenResult<ShaderBinary> result{};

    if(request.source.empty())
    {
        result.success=false;
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::CompileFailed,
                                      request.stage,
                                      "ShaderCompilerContext",
                                      "shader source is empty"});
        return result;
    }

    result.success=false;
    result.diagnostics.push_back({ShaderGenSeverity::Warning,
                                  ShaderGenErrorCode::InternalError,
                                  request.stage,
                                  "ShaderCompilerContext",
                                  "skeleton implementation: compile path not wired yet"});
    return result;
}
}//namespace hgl::graph
