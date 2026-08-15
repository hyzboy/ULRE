#pragma once

#include <hgl/graph/glsl/GLSLCodeModuleRegistry.h>

namespace hgl::graph::mtl
{
    enum class GLSLCodeModuleMetadataValidationError : uint8
    {
        None = 0,
        InvalidDefinition,
        UnsupportedVersion,
        InvalidArray,
        InvalidRequirement,
        DuplicateRequirement,
        DuplicateProvide,
        InvalidDependencyVersion,
        DuplicateDependency,
        SelfDependency,
        MissingDependency,
        DependencyVersionMismatch,
        InvalidCondition,
        DuplicateCondition,
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
        GLSLCodeModuleID module_id = GLSLCodeModuleID::TestProviderA;
        GLSLCodeModuleID related_module_id = GLSLCodeModuleID::TestProviderA;
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
