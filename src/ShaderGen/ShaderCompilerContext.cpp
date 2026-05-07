#include<hgl/shadergen/ShaderCompilerContext.h>
#include"GLSLCompiler.h"

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

    SPVData *spv=CompileShader(static_cast<uint32>(request.stage),request.source.c_str());
    if(!spv)
    {
        result.success=false;
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::CompileFailed,
                                      request.stage,
                                      "ShaderCompilerContext",
                                      "CompileShader returned null"});
        return result;
    }

    result.value.stage=request.stage;

    if(spv->spv_data&&spv->spv_length>0)
        result.value.spirv.assign(spv->spv_data,spv->spv_data+spv->spv_length);

    if(!spv->result)
    {
        std::string compile_log=spv->log?spv->log:"compile failed without log";
        FreeSPVData(spv);

        result.success=false;
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::CompileFailed,
                                      request.stage,
                                      "ShaderCompilerContext",
                                      compile_log});
        return result;
    }

    FreeSPVData(spv);
    result.success=true;
    return result;
}
}//namespace hgl::graph
