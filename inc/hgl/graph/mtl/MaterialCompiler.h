#pragma once

/// MaterialCompiler.h — FixedMaterialDef → MaterialCreateInfo 编译器接口
///
/// 这是 FixedMaterialDef 体系的唯一运行时入口。
/// 内部流程：
///   1. 按 def.descriptor_entries[] 构建 MaterialDescriptorInfo（顺序固定，无动态排序）
///   2. key.AppendGLSLDefines(prefix) → 4 行 #define 前缀
///   3. prefix + def.vert_glsl / frag_glsl → glslang 编译 → SPV
///   4. 填充并返回 MaterialCreateInfo*
///
/// 与现有 Std3DMaterial 体系互不干扰，可渐进迁移。

#include<hgl/graph/mtl/FixedMaterialDef.h>
#include<hgl/graph/mtl/ShaderComposition.h>
#include<hgl/graph/mtl/ShaderLogic.h>
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/vk/VKDeviceAttribute.h>

namespace hgl::graph::mtl{

struct Material3DCreateConfig;

/**
 * 编译一个 FixedMaterialDef 排列，返回 MaterialCreateInfo*。
 *
 * @param dev_attr  Vulkan 设备能力（用于判断 SSBO 对齐等限制）
 * @param def       编译期材质定义（constexpr 常量，描述符布局 + 顶点输入 + GLSL 源码）
 * @param key       排列键（ambient/light/specular/shadow 轴组合），默认 = 手机最低配
 * @param config    运行时配置（可选），若非空则 config->prim 覆盖 def.primitive_type
 * @return          编译好的 MaterialCreateInfo*，调用方负责 delete；失败返回 nullptr
 */
MaterialCreateInfo *CompileFixedMaterial(
    const VulkanDevAttr *       dev_attr,
    const FixedMaterialDef &    def,
    const ShaderPermutationKey &key = ShaderPermutationKey{},
    const Material3DCreateConfig *config = nullptr);

/**
 * 使用 ComposedMaterialDef + MaterialLogicDef 生成业务驱动的 VS/FS main，
 * 再复用 FixedMaterialDef 编译入口完成 SPV 编译。
 *
 * @param config    运行时配置（可选），若非空则 config->prim 覆盖 def.primitive_type
 * 失败返回 nullptr；调用方可回退到 legacy 材质路径。
 */
MaterialCreateInfo *CompileComposedBusinessMaterial(
    const VulkanDevAttr *       dev_attr,
    const FixedMaterialDef &    base_fixed_def,
    const ComposedMaterialDef & base_composed_def,
    const MaterialLogicDef &    logic,
    const ShaderPermutationKey &key = ShaderPermutationKey{},
    const Material3DCreateConfig *config = nullptr);

/**
 * 材质 fallback 工厂辅助宏。（待实现，参见任务 2.3）
 * 先尝试从 recipe 文件加载（file-driven），文件不存在时用 hardcode def。
 * 在 M_Xxx.cpp 工厂函数中使用：
 *
 *   COMPILE_WITH_RECIPE_FALLBACK(dev_attr, BASIC_LIT_BASE_DEF, key,
 *                                "recipes/uber/uber_3d.json", "pc_high")
 *
 * 注：此宏的实现需要完成 ShaderTemplateEngine 和 TemplateBasedMaterialFactory。
 * 当前版本保留宏定义但注释掉，避免前向引用错误。
 */
// #define COMPILE_WITH_RECIPE_FALLBACK(dev_attr_, def_, key_, recipe_path_, quality_) \
//     ([&]() -> MaterialCreateInfo* { \
//         MaterialCreateInfo *_mci = CreateMaterialFromRecipe(dev_attr_, recipe_path_, quality_, def_); \
//         if(_mci) return _mci; \
//         return CompileFixedMaterial(dev_attr_, def_, key_); \
//     }())

}//namespace hgl::graph::mtl
