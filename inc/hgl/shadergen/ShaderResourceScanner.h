#pragma once

#include <hgl/mtl/StaticMaterialDef.h>
#include <string>

namespace hgl::graph::mtl
{
    struct ShaderResourceDependencies
    {
        UBOSemanticSet ubos;
        SSBOSemanticSet ssbos;
        StaticTextureSamplerDescriptors samplers;
    };

    bool CollectShaderAutoRequirements(const StaticMaterialDef &base_def,
                                       const std::string &shader_library_path,
                                       const std::string &vertex_glsl,
                                       const std::string &fragment_glsl,
                                       ShaderResourceDependencies &out_requirements,
                                       std::string *diagnostics = nullptr);

    void MergeShaderAutoRequirements(const StaticMaterialDef &base_def,
                                     const ShaderResourceDependencies &auto_requirements,
                                     StaticMaterialDef &out_def,
                                     UBOSemanticSet &ubo_storage,
                                     SSBOSemanticSet &ssbo_storage,
                                     StaticTextureSamplerDescriptors &sampler_storage);
}
