#pragma once

#include <hgl/mtl/FixedMaterialDef.h>
#include <string>

namespace hgl::graph::mtl
{
    struct ShaderAutoRequirements
    {
        FixedUBODescriptors ubos;
        FixedSSBODescriptors ssbos;
        FixedTextureSamplerDescriptors samplers;
    };

    bool CollectShaderAutoRequirements(const std::string &shader_library_path,
                                       const std::string &vertex_glsl,
                                       const std::string &fragment_glsl,
                                       ShaderAutoRequirements &out_requirements,
                                       std::string *diagnostics = nullptr);

    void MergeShaderAutoRequirements(const FixedMaterialDef &base_def,
                                     const ShaderAutoRequirements &auto_requirements,
                                     FixedMaterialDef &out_def,
                                     FixedUBODescriptors &ubo_storage,
                                     FixedSSBODescriptors &ssbo_storage,
                                     FixedTextureSamplerDescriptors &sampler_storage);
}
