#pragma once

#include <hgl/mtl/GlslModuleRegistry.h>

namespace hgl::graph::mtl
{
    // 契约错误 X 列表（单一真源——枚举与 GetXxxErrorName 同源，新增错误只改此处）
    //
    // 这里只校验**单条定义的自述完整性**（表自身的卫生）。
    // 「依赖满足性」三项（MissingDependency / MissingConflictTarget /
    // DependencyCycle）与模块冲突两项（SelfConflict / DuplicateConflict）已删除：
    // 它们与真实装配路径重复，权威判据是
    //   ① `GlslModuleRegistry` 加载期 incomplete 模块剔除（发现缺失即丢弃）
    //   ② `BuildShaderCodeResourceManifest` 的 DFS（UnknownCodeModule / CodeModuleCycle）
    //   ③ 最终「拼出的 GLSL 能否编译」（`SPV.generated-interface` 16 变体断言）
#define HGL_GLSL_CODE_MODULE_METADATA_VALIDATION_ERROR_LIST \
    HGL_ERROR(None) \
    HGL_ERROR(InvalidDefinition) \
    HGL_ERROR(InvalidArray) \
    HGL_ERROR(InvalidRequirement) \
    HGL_ERROR(DuplicateRequirement) \
    HGL_ERROR(DuplicateProvide) \
    HGL_ERROR(DuplicateDependency) \
    HGL_ERROR(SelfDependency) \
    HGL_ERROR(AmbiguousProviderPriority)

    enum class GlslModuleMetadataValidationError : uint8
    {
#define HGL_ERROR(name) name,
        HGL_GLSL_CODE_MODULE_METADATA_VALIDATION_ERROR_LIST
#undef HGL_ERROR
    };

    struct GlslModuleMetadataValidationDiagnostic
    {
        GlslModuleMetadataValidationError error =
            GlslModuleMetadataValidationError::None;
        AnsiString module_name;
        AnsiString related_module_name;
        GlslModuleSemantic semantic = GlslModuleSemantic::Unknown;
    };

    const char *GetGlslModuleMetadataValidationErrorName(
        GlslModuleMetadataValidationError error) noexcept;

    bool ValidateGlslModuleMetadata(
        const GlslModuleDefinition &definition,
        GlslModuleMetadataValidationDiagnostic &out_diagnostic) noexcept;

    bool ValidateGlslModuleRegistryMetadata(
        const GlslModuleRegistry &registry,
        GlslModuleMetadataValidationDiagnostic &out_diagnostic) noexcept;
}
