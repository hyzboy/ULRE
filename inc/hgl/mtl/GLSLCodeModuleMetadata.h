#pragma once

#include <hgl/mtl/GLSLCodeModuleRegistry.h>

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

    enum class GLSLCodeModuleMetadataValidationError : uint8
    {
#define HGL_ERROR(name) name,
        HGL_GLSL_CODE_MODULE_METADATA_VALIDATION_ERROR_LIST
#undef HGL_ERROR
    };

    struct GLSLCodeModuleMetadataValidationDiagnostic
    {
        GLSLCodeModuleMetadataValidationError error =
            GLSLCodeModuleMetadataValidationError::None;
        AnsiString module_name;
        AnsiString related_module_name;
        GLSLCodeModuleSemantic semantic = GLSLCodeModuleSemantic::Unknown;
        uint32 item_index = 0;
    };

    const char *GetGLSLCodeModuleMetadataValidationErrorName(
        GLSLCodeModuleMetadataValidationError error) noexcept;

    uint32 GetNormalizedGLSLCodeModuleDependencyCount(
        const GLSLCodeModuleDefinition &definition) noexcept;

    bool GetNormalizedGLSLCodeModuleDependency(
        const GLSLCodeModuleDefinition &definition,
        uint32 index,
        GLSLCodeModuleDependency &out_dependency) noexcept;

    bool AreGLSLCodeModulesConflicting(
        const GLSLCodeModuleDefinition &lhs,
        const GLSLCodeModuleDefinition &rhs) noexcept;

    bool ValidateGLSLCodeModuleMetadata(
        const GLSLCodeModuleDefinition &definition,
        GLSLCodeModuleMetadataValidationDiagnostic &out_diagnostic) noexcept;

    bool ValidateGLSLCodeModuleRegistryMetadata(
        const GLSLCodeModuleRegistry &registry,
        GLSLCodeModuleMetadataValidationDiagnostic &out_diagnostic) noexcept;
}
