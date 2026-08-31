#pragma once

#include <hgl/mtl/GLSLCodeModule.h>

namespace hgl::graph::mtl
{
    class GLSLCodeModuleRegistry;
    constexpr uint32 MaxModuleResourceManifestCodeModules = 64u;
    constexpr uint32 MaxModuleResourceManifestSSBOs = 64u;
    constexpr uint32 MaxModuleResourceManifestTextureLayers = 16u;

    // 契约错误 X 列表（单一真源——枚举与 GetXxxErrorName 同源，新增错误只改此处）
#define HGL_MODULE_RESOURCE_MANIFEST_ERROR_LIST \
    HGL_ERROR(None) \
    HGL_ERROR(NullRootList) \
    HGL_ERROR(UnknownCodeModule) \
    HGL_ERROR(CodeModuleCycle) \
    HGL_ERROR(CodeModuleCapacityExceeded) \
    HGL_ERROR(SSBOCapacityExceeded) \
    HGL_ERROR(TextureLayerCapacityExceeded) \
    HGL_ERROR(ResourceConflict)

    enum class ModuleResourceManifestError : uint8
    {
#define HGL_ERROR(name) name,
        HGL_MODULE_RESOURCE_MANIFEST_ERROR_LIST
#undef HGL_ERROR
    };

    struct ModuleResourceManifest
    {
        // Names of the code modules contributing to this material, in
        // dependency order. Pointers reference registry-owned module names.
        const char *code_module_names[MaxModuleResourceManifestCodeModules]{};
        uint32 code_module_count = 0;

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
