#pragma once

#include <hgl/mtl/GLSLCodeModule.h>

namespace hgl::graph::mtl
{
    class GLSLCodeModuleRegistry;
    constexpr uint32 MaxModuleResourceManifestCodeModules = 64u;
    constexpr uint32 MaxModuleResourceManifestUBOs = 64u;
    constexpr uint32 MaxModuleResourceManifestSSBOs = 64u;
    constexpr uint32 MaxModuleResourceManifestTextureLayers = 16u;

    enum class ModuleResourceManifestError : uint8
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

    struct ModuleResourceManifest
    {
        // Names of the code modules contributing to this material, in
        // dependency order. Pointers reference registry-owned module names.
        const char *code_module_names[MaxModuleResourceManifestCodeModules]{};
        uint32 code_module_count = 0;

        GLSLCodeModuleUBORequirement ubos[MaxModuleResourceManifestUBOs]{};
        uint32 ubo_count = 0;

        GLSLCodeModuleSSBORequirement ssbos[MaxModuleResourceManifestSSBOs]{};
        uint32 ssbo_count = 0;

        GLSLCodeModuleTextureLayerRequirement texture_layers[MaxModuleResourceManifestTextureLayers]{};
        uint32 texture_layer_count = 0;

        uint64 stable_hash = 0;
        ModuleResourceManifestError error = ModuleResourceManifestError::None;
        const char *error_module_name = nullptr;

        bool IsValid() const noexcept
        {
            return error == ModuleResourceManifestError::None;
        }
    };

    bool BuildModuleResourceManifest(
        const char *const *root_module_names,
        uint32 root_module_count,
        ModuleResourceManifest &manifest,
        const GLSLCodeModuleRegistry *registry = nullptr) noexcept;

    const char *GetModuleResourceManifestErrorName(ModuleResourceManifestError error) noexcept;
}
