#pragma once

namespace hgl::graph::mtl { struct MaterialDefinitionBuildRequest; }

#include <hgl/mtl/GLSLCodeModuleRegistry.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/CanonicalShaderContract.h>
#include <hgl/type/String.h>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;

    // 契约错误 X 列表（单一真源——枚举与 GetXxxErrorName 同源，新增错误只改此处）
#define HGL_RESOLVED_MODULE_GRAPH_BUILD_ERROR_LIST \
    HGL_ERROR(None) \
    HGL_ERROR(MissingRootModule) \
    HGL_ERROR(MissingDependency) \
    HGL_ERROR(DependencyCycle) \
    HGL_ERROR(ModuleConflict) \
    HGL_ERROR(RequirementConflict) \
    HGL_ERROR(StableIDConflict) \
    HGL_ERROR(InvalidCanonicalGraph)

    enum class ResolvedModuleGraphBuildError : uint8
    {
#define HGL_ERROR(name) name,
        HGL_RESOLVED_MODULE_GRAPH_BUILD_ERROR_LIST
#undef HGL_ERROR
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
        const mtl::MaterialDefinition &definition,
        const GLSLCodeModuleRegistry &registry,
        ResolvedModuleGraph &out_graph,
        ResolvedModuleGraphBuildDiagnostic &out_diagnostic,
        const mtl::MaterialDefinitionBuildRequest *request = nullptr);
}
