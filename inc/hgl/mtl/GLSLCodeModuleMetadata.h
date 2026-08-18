#pragma once

#include <hgl/mtl/GLSLCodeModuleRegistry.h>

namespace hgl::graph::mtl
{
    enum class GLSLCodeModuleMetadataValidationError : uint8
    {
        None = 0,
        InvalidDefinition,
        InvalidArray,
        InvalidRequirement,
        DuplicateRequirement,
        DuplicateProvide,
        DuplicateDependency,
        SelfDependency,
        MissingDependency,
        DuplicateConflict,
        SelfConflict,
        MissingConflictTarget,
        DependencyCycle,
        AmbiguousProviderPriority
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
