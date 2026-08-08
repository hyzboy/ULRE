#pragma once

#include <hgl/graph/glsl/GLSLCodeModule.h>

namespace hgl::graph::mtl
{
    class GLSLCodeModuleRegistry;
    constexpr uint32 MaxShaderResourceManifestCodeModules = 64u;
    constexpr uint32 MaxShaderResourceManifestUBOs = 64u;
    constexpr uint32 MaxShaderResourceManifestSSBOs = 64u;
    constexpr uint32 MaxShaderResourceManifestTextures = 64u;
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
        TextureCapacityExceeded,
        TextureLayerCapacityExceeded,
        ResourceConflict
    };

    struct ShaderResourceManifest
    {
        GLSLCodeModuleID code_modules[MaxShaderResourceManifestCodeModules]{};
        uint32 code_module_count = 0;

        GLSLCodeModuleUBORequirement ubos[MaxShaderResourceManifestUBOs]{};
        uint32 ubo_count = 0;

        GLSLCodeModuleSSBORequirement ssbos[MaxShaderResourceManifestSSBOs]{};
        uint32 ssbo_count = 0;

        GLSLCodeModuleTextureRequirement textures[MaxShaderResourceManifestTextures]{};
        uint32 texture_count = 0;

        GLSLCodeModuleTextureLayerRequirement texture_layers[MaxShaderResourceManifestTextureLayers]{};
        uint32 texture_layer_count = 0;

        uint64 stable_hash = 0;
        ShaderResourceManifestError error = ShaderResourceManifestError::None;
        GLSLCodeModuleID error_module = GLSLCodeModuleID::SkyLightHeader;

        bool IsValid() const noexcept
        {
            return error == ShaderResourceManifestError::None;
        }
    };

    bool BuildShaderResourceManifest(
        const GLSLCodeModuleID *root_modules,
        uint32 root_module_count,
        ShaderResourceManifest &manifest,
        const GLSLCodeModuleRegistry *registry = nullptr) noexcept;

    const char *GetShaderResourceManifestErrorName(ShaderResourceManifestError error) noexcept;
}
