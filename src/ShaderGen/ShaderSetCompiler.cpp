#include<hgl/shadergen/ShaderSetCompiler.h>
#include<hgl/shadergen/ShaderStageCompiler.h>

namespace hgl::graph::mtl
{
ShaderGenStatus ShaderSetCompiler::TryCompile(ShaderStageMap &shader_map,std::vector<ShaderGenDiagnostic> *diagnostics)
{
    ShaderGenStatus result{};

    if(shader_map.IsEmpty())
    {
        result.success=false;
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::InvalidConfig,
                                      ShaderStage::Vertex,
                                      "ShaderSetCompiler",
                                      "shader_map is empty"});

        if(diagnostics)
            diagnostics->insert(diagnostics->end(),result.diagnostics.begin(),result.diagnostics.end());

        return result;
    }

    for(auto &kv:shader_map)
    {
        const ShaderStage stage=kv.first;
        ShaderCreateInfo *sc=kv.second;

        if(!sc)
        {
            result.success=false;
            result.diagnostics.push_back({ShaderGenSeverity::Error,
                                          ShaderGenErrorCode::InvalidShaderStage,
                                          stage,
                                          "ShaderSetCompiler",
                                          "shader create info is null"});

            if(diagnostics)
                diagnostics->insert(diagnostics->end(),result.diagnostics.begin(),result.diagnostics.end());

            return result;
        }

        if(!ShaderStageCompiler::Compile(sc))
        {
            result.success=false;
            result.diagnostics.push_back({ShaderGenSeverity::Error,
                                          ShaderGenErrorCode::CompileFailed,
                                          stage,
                                          "ShaderSetCompiler",
                                          "ShaderStageCompiler::Compile failed"});

            if(diagnostics)
                diagnostics->insert(diagnostics->end(),result.diagnostics.begin(),result.diagnostics.end());

            return result;
        }
    }

    result.success=true;

    if(diagnostics)
        diagnostics->insert(diagnostics->end(),result.diagnostics.begin(),result.diagnostics.end());

    return result;
}

bool ShaderSetCompiler::Compile(ShaderStageMap &shader_map)
{
    return TryCompile(shader_map,nullptr).success;
}
}//namespace hgl::graph::mtl
