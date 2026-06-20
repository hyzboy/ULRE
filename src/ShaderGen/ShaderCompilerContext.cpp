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

    SPVData *spv=CompileShader(static_cast<uint32>(request.stage),
                               request.source.c_str(),
                               request.debug_context.empty()?nullptr:request.debug_context.c_str());
    if(!spv)
    {
        result.success=false;
        std::string message = "CompileShader returned null";
        if(!request.debug_context.empty())
        {
            message += " | context=";
            message += request.debug_context;
        }

        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::CompileFailed,
                                      request.stage,
                                      "ShaderCompilerContext",
                                      std::move(message)});
        return result;
    }

    result.value.stage=request.stage;

    if(spv->spv_data&&spv->spv_length>0)
    {
        if((spv->spv_length%sizeof(uint32))!=0)
        {
            FreeSPVData(spv);

            result.success=false;
            result.diagnostics.push_back({ShaderGenSeverity::Error,
                                          ShaderGenErrorCode::CompileFailed,
                                          request.stage,
                                          "ShaderCompilerContext",
                                          "invalid SPIR-V byte length (not uint32-aligned)"});
            return result;
        }

        const size_t word_count=spv->spv_length/sizeof(uint32);
        result.value.spirv.assign(spv->spv_data,spv->spv_data+word_count);
    }

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
