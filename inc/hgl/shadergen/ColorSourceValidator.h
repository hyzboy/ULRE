#pragma once

/// ColorSourceValidator.h — G1/G4 校验闸门
///
/// G1（ValidateColorSources）: ColorSource 列表进入 BindingAllocator 之前：
///   - 槽位有效且不重复
///   - kind != None
///   - bindings 数量与 kind 语义一致
///
/// G4（G4ValidateReflectedSamplers）: SPIR-V 编译后：
///   - FS 实际引用的 sampler 名 ⊆ MaterialDescriptorDB 已声明的 sampler 名
///   - 任何未能反查到 SamplerSlot 的 sampler 名 → [G4][FATAL]
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
/// @param mci       已完成 CompileShaderStagesToSPV() 的 MaterialCreateInfo
/// @param diag      诊断输出列表（追加，不清零）
/// @returns         true = 无 FATAL；false = 发现未声明的 sampler（需要中止材质创建）
bool G4ValidateReflectedSamplers(const mtl::MaterialCreateInfo &mci,
                                 std::vector<ShaderGenDiagnostic> &diag);

} // namespace hgl::graph
