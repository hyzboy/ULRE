#pragma once

#include <hgl/mtl/ShaderPermutationKey.h>
#include <string>

namespace hgl::graph::mtl {

struct ComposedMaterialDef;

namespace builtin_helpers {

// 生成指定 shader stage 可用的内置高级辅助函数
// 这些函数面向业务逻辑代码，避免在业务侧重复实现。
std::string GenStageHelpers(
    const ComposedMaterialDef &def,
    const ShaderPermutationKey &key,
    const char *shader_stage);

} // namespace builtin_helpers

} // namespace hgl::graph::mtl
