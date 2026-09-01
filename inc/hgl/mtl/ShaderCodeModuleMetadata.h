#pragma once

#include <hgl/mtl/ShaderCodeModuleRegistry.h>

namespace hgl::graph::mtl
{
    // 契约错误 X 列表（单一真源——枚举与 GetXxxErrorName 同源，新增错误只改此处）
#define HGL_GLSL_CODE_MODULE_METADATA_VALIDATION_ERROR_LIST \
    HGL_ERROR(None) \
    HGL_ERROR(InvalidDefinition) \
    HGL_ERROR(InvalidArray) \
    HGL_ERROR(InvalidRequirement) \
    HGL_ERROR(DuplicateRequirement) \
    HGL_ERROR(DuplicateProvide) \
    HGL_ERROR(DuplicateDependency) \
    HGL_ERROR(SelfDependency) \
    HGL_ERROR(MissingDependency) \
    HGL_ERROR(DuplicateConflict) \
    HGL_ERROR(SelfConflict) \
    HGL_ERROR(MissingConflictTarget) \
    HGL_ERROR(DependencyCycle) \
    HGL_ERROR(AmbiguousProviderPriority)

    enum class ShaderCodeModuleMetadataValidationError : uint8
    {
#define HGL_ERROR(name) name,
        HGL_GLSL_CODE_MODULE_METADATA_VALIDATION_ERROR_LIST
#undef HGL_ERROR
    };

    struct ShaderCodeModuleMetadataValidationDiagnostic
    {
        ShaderCodeModuleMetadataValidationError error =
            ShaderCodeModuleMetadataValidationError::None;
        AnsiString module_name;
        AnsiString related_module_name;
        ShaderCodeModuleSemantic semantic = ShaderCodeModuleSemantic::Unknown;
        uint32 item_index = 0;
    };

    const char *GetShaderCodeModuleMetadataValidationErrorName(
        ShaderCodeModuleMetadataValidationError error) noexcept;

    uint32 GetNormalizedShaderCodeModuleDependencyCount(
        const ShaderCodeModuleDefinition &definition) noexcept;

    bool GetNormalizedShaderCodeModuleDependency(
        const ShaderCodeModuleDefinition &definition,
        uint32 index,
        ShaderCodeModuleDependency &out_dependency) noexcept;

    bool AreShaderCodeModulesConflicting(
        const ShaderCodeModuleDefinition &lhs,
        const ShaderCodeModuleDefinition &rhs) noexcept;

    bool ValidateShaderCodeModuleMetadata(
        const ShaderCodeModuleDefinition &definition,
        ShaderCodeModuleMetadataValidationDiagnostic &out_diagnostic) noexcept;

    bool ValidateShaderCodeModuleRegistryMetadata(
        const ShaderCodeModuleRegistry &registry,
        ShaderCodeModuleMetadataValidationDiagnostic &out_diagnostic) noexcept;
}
