#pragma once

#include <hgl/graph/glsl/GLSLCodeModuleRegistry.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/shadergen/CanonicalShaderContract.h>
#include <hgl/type/String.h>

namespace hgl::graph::mtl
{
    struct MaterialDefinitionBuildRequest;

    enum class ResolvedModuleGraphBuildError : uint8
    {
        None = 0,
        MissingRootModule,
        MissingDependency,
        DependencyCycle,
        ModuleConflict,
        RequirementConflict,
        StableIDConflict,
        InvalidCanonicalGraph
    };

    struct ResolvedModuleGraphBuildDiagnostic
    {
        ResolvedModuleGraphBuildError error =
            ResolvedModuleGraphBuildError::None;
        AnsiString module_name;
        AnsiString related_module_name;
        GLSLCodeModuleSemantic semantic =
            GLSLCodeModuleSemantic::Unknown;
    };

    const char *GetResolvedModuleGraphBuildErrorName(
        ResolvedModuleGraphBuildError error) noexcept;

    ShaderContractStableID GetGLSLCodeModuleStableID(
        const GLSLCodeModuleDefinition &definition) noexcept;

    uint64 GetCanonicalGLSLCodeModuleContentHash(
        const GLSLCodeModuleDefinition &definition,
        const GLSLCodeModuleRegistry &registry) noexcept;

    bool BuildMaterialResolvedModuleGraph(
        const MaterialDefinition &definition,
        const GLSLCodeModuleRegistry &registry,
        ResolvedModuleGraph &out_graph,
        ResolvedModuleGraphBuildDiagnostic &out_diagnostic,
        const MaterialDefinitionBuildRequest *request = nullptr);
}
