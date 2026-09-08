#include<hgl/mtl/ShaderBuildContext.h>
#include<hgl/mtl/ShaderCreateInfo.h>
#include<hgl/mtl/contract/ShaderGenContract.h>
#include<string>
using namespace hgl;
using namespace hgl::graph;

namespace hgl::graph::mtl{
    using namespace hgl::graph::mtl;

ShaderBuildContext::ShaderBuildContext(const PrimitiveType primitive_type_value,const uint32_t shader_stage_bits)
    : primitive_type(primitive_type_value), shader_stage_flag_bits(shader_stage_bits)
{
    if(has_mesh      ())shader_map.Add(new ShaderCreateInfo(ShaderStage::Mesh));
    if(has_fragment  ())shader_map.Add(new ShaderCreateInfo(ShaderStage::Fragment));
}

ShaderBuildContext::~ShaderBuildContext()
{
    // Explicitly clear the shader_map to properly clean up ShaderCreateInfo objects
    // This ensures proper destructor ordering and prevents crashes with UnorderedMap
    for(auto [stage, sc] : shader_map)
    {
        if(sc)
            delete sc;
    }
    shader_map.Clear();
}

bool ShaderBuildContext::CreateShaderDirect()
{
    if(shader_map.IsEmpty())
        return(false);

    for(auto& kv : shader_map)
    {
        ShaderCreateInfo *sc = kv.second;

        if(!sc->CompileFinalGLSLToSPV())
            return(false);
    }

    return(true);
}
}//namespace hgl::graph::mtl
