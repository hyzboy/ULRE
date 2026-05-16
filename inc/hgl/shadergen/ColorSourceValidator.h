#pragma once

/// ColorSourceValidator.h — G1/G4 校验闸门
///
/// G1（ValidateColorSources）: ColorSource 列表进入 BindingAllocator 之前：
///   - 槽位有效且不重复
///   - kind != None
///   - bindings 数量与 kind 语义一致
///
/// G4（G4ValidateReflectedSamplers）: SPIR-V 编译后：
///   - declared 表：从 MaterialDescriptorDB 取 COMBINED_IMAGE_SAMPLER (name, set, binding)
///   - reflected 表：从 FS SPIR-V 取 COMBINED_IMAGE_SAMPLER (name, set, binding)
///   - 校验：reflected ⊆ declared（按名字匹配）
///   - 任何 reflected 中出现而 declared 中没有的 sampler → [G4][FATAL]，同时输出两份表
///   - 已声明但 SPIR-V 中未出现（被优化掉）→ [G4][INFO]（不报错）

#include <hgl/shadergen/ColorSource.h>
#include <hgl/shadergen/ShaderGenDiagnostic.h>
#include <string>
#include <vector>

namespace hgl::graph
{

struct ColorSourceValidationError
{
    size_t      source_index = 0;  ///< 出错的 ColorSource 在列表中的下标
    std::string message;
};

struct ColorSourceValidationResult
{
    bool                                  ok = true;
    std::vector<ColorSourceValidationError> errors;

    explicit operator bool() const noexcept { return ok; }
};

/// G1 校验：仅检查结构合法性，不访问 GPU 资源。
/// 建议在 MaterialRecipe -> MaterialVariantRow 转换前调用。
ColorSourceValidationResult ValidateColorSources(const std::vector<ColorSource> &sources);

} // namespace hgl::graph

namespace hgl::graph::mtl
{
class MaterialCreateInfo;
}

namespace hgl::graph
{

/// G4 校验：SPIR-V 反射后，验证 FS 实际引用的 sampler 名 ⊆ 已声明的 binding。
///
/// declared 表来自 MaterialDescriptorDB（Resort() 后 set/binding 已确定）。
/// reflected 表来自 FS SPIR-V COMBINED_IMAGE_SAMPLER 反射结果。
/// 任何 mismatch → [G4][FATAL]（不再区分 slot_known WARN vs FATAL）。
/// 诊断消息包含两份 (name, set, binding) 表。
///
/// @param mci       已完成 CompileShaderStagesToSPV() 的 MaterialCreateInfo
/// @param diag      诊断输出列表（追加，不清零）
/// @returns         true = 无 FATAL；false = 发现未声明的 sampler（需要中止材质创建）
bool G4ValidateReflectedSamplers(const mtl::MaterialCreateInfo &mci,
                                 std::vector<ShaderGenDiagnostic> &diag);

} // namespace hgl::graph
