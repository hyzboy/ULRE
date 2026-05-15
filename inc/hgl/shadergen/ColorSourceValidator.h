#pragma once

/// ColorSourceValidator.h — G1 签名/槽位有效性校验
///
/// 在 ColorSource 列表进入 BindingAllocator 之前做快速检查：
///   - 槽位有效（不超出 SamplerSlot 范围）
///   - 槽位不重复
///   - kind != None（即没有未完成的占位 ColorSource 误入管线）
///   - bindings 数量与 kind 语义一致（内置 sampler 恰好 1 个 binding）

#include <hgl/shadergen/ColorSource.h>
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
