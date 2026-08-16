#pragma once

#include <hgl/graph/glsl/GLSLCodeModule.h>

namespace hgl::graph::mtl
{
    class GLSLCodeModuleRegistry;
    constexpr uint32 MaxShaderResourceManifestCodeModules = 64u;
    constexpr uint32 MaxShaderResourceManifestUBOs = 64u;
    constexpr uint32 MaxShaderResourceManifestSSBOs = 64u;
    constexpr uint32 MaxShaderResourceManifestTextureLayers = 16u;

    enum class ShaderResourceManifestError : uint8
    {
        None = 0,
        NullRootList,
        UnknownCodeModule,
        CodeModuleCycle,
        CodeModuleCapacityExceeded,
        UBOCapacityExceeded,
        SSBOCapacityExceeded,
        TextureLayerCapacityExceeded,
        ResourceConflict
    };

    struct ShaderResourceManifest
    {
        // Names of the code modules contributing to this material, in
        // dependency order. Pointers reference registry-owned module names.
        const char *code_module_names[MaxShaderResourceManifestCodeModules]{};
        uint32 code_module_count = 0;

        GLSLCodeModuleUBORequirement ubos[MaxShaderResourceManifestUBOs]{};
        uint32 ubo_count = 0;

        GLSLCodeModuleSSBORequirement ssbos[MaxShaderResourceManifestSSBOs]{};
        uint32 ssbo_count = 0;

        GLSLCodeModuleTextureLayerRequirement texture_layers[MaxShaderResourceManifestTextureLayers]{};
        uint32 texture_layer_count = 0;

        uint64 stable_hash = 0;
        ShaderResourceManifestError error = ShaderResourceManifestError::None;
        const char *error_module_name = nullptr;

        bool IsValid() const noexcept
        {
            return error == ShaderResourceManifestError::None;
        }
    };

    bool BuildShaderResourceManifest(
        const char *const *root_module_names,
        uint32 root_module_count,
        ShaderResourceManifest &manifest,
        const GLSLCodeModuleRegistry *registry = nullptr) noexcept;

    const char *GetShaderResourceManifestErrorName(ShaderResourceManifestError error) noexcept;
}
