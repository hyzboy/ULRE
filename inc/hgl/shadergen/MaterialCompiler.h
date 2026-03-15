#pragma once

/// MaterialCompiler.h — FixedMaterialDef → MaterialCreateInfo 编译器接口
///
/// 这是 FixedMaterialDef 体系的唯一运行时入口。
/// 内部流程：
///   1. 按 def.descriptor_entries[] 构建 MaterialDescriptorInfo（顺序固定，无动态排序）
///   2. key.AppendGLSLDefines(prefix) → #define 前缀（ULRE_SKYLIGHT_MODEL / LIGHT_MODEL 等）
///   3. prefix + vert_glsl, frag_glsl（参数传入）→ glslang 编译 → SPV
///   4. 填充并返回 MaterialCreateInfo*
///
/// 与现有 Std3DMaterial 体系互不干扰，可渐进迁移。

#include<hgl/mtl/FixedMaterialDef.h>
#include<hgl/mtl/ShaderPermutationKey.h>
#include<hgl/shadergen/contract/ShaderGenContract.h>
#include <string>

namespace hgl::graph::mtl{

struct Material3DCreateConfig;
struct Material2DCreateConfig;
struct ComposedMaterialDef;
struct MaterialLogicDef;
class MaterialCreateInfo;

/**
 * 编译一个 FixedMaterialDef 排列，返回 MaterialCreateInfo*。
 *
 * @param profile   设备能力 profile（限制与特性快照）
 * @param def       编译期材质定义（constexpr 常量，描述符布局 + 顶点输入）
 * @param vert_glsl 完整 vertex shader 源码（含 void main()）
 * @param frag_glsl 完整 fragment shader 源码（含 void main()）
 * @param geom_glsl 完整 geometry shader 源码；nullptr = 无 GS
 * @param key       排列键（ambient/light/specular/shadow 轴组合），默认 = 手机最低配
 * @param config    运行时配置（可选），若非空则 config->prim 覆盖 def.primitive_type
 * @return          编译好的 MaterialCreateInfo*，调用方负责 delete；失败返回 nullptr
 */
MaterialCreateInfo *CompileFixedMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const FixedMaterialDef &    def,
    const char *                vert_glsl,
    const char *                frag_glsl,
    const char *                geom_glsl     = nullptr,
    const ShaderPermutationKey &key           = ShaderPermutationKey{},
    const Material3DCreateConfig *config      = nullptr);

/**
 * 使用 ComposedMaterialDef + MaterialLogicDef 生成业务驱动的 VS/FS main，
 * 再复用 FixedMaterialDef 编译入口完成 SPV 编译。
 *
 * @param config    运行时配置（可选），若非空则 config->prim 覆盖 def.primitive_type
 * 失败返回 nullptr；调用方可回退到 legacy 材质路径。
 */
MaterialCreateInfo *CompileComposedBusinessMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const FixedMaterialDef &    base_fixed_def,
    const ComposedMaterialDef & base_composed_def,
    const MaterialLogicDef &    logic,
    const ShaderPermutationKey &key = ShaderPermutationKey{},
    const Material3DCreateConfig *config = nullptr);

/**
 * 校验生成阶段 FS 入口代码（fs_main）与 FragmentShaderBusiness 所需 helper 是否一致。
 *
 * @param def         Composed 材质定义（包含 fragment_business 及显式 helper 依赖）
 * @param generated_fs 生成后的 FS GLSL 文本
 * @return            一致返回 true；缺失 helper 返回 false（并输出诊断）
 */
bool ValidateFSMainBusinessHelperConsistency(
    const ComposedMaterialDef &def,
    const std::string &generated_fs);

/**
 * 编译 Compositor 模板产出的完整 GLSL → MaterialCreateInfo*。
 *
 * 与 CompileFixedMaterial 共享相同的 descriptor/vertex 元数据填充逻辑（Steps 1-5），
 * 但跳过 ProcXXX 管线，使用 SetFinalGLSL() + CreateShaderDirect() 直接编译。
 *
 * @param profile   设备能力 profile
 * @param def       材质定义（descriptor/vertex/MI 元数据，与 CompileFixedMaterial 共用 FixedMaterialDef）
 * @param vs_glsl   完整的 vertex shader GLSL（含 #version, layout, main）
 * @param fs_glsl   完整的 fragment shader GLSL（含 #version, layout, main）
 * @param config    运行时配置（可选）
 * @return          编译好的 MaterialCreateInfo*; 失败返回 nullptr
 */
MaterialCreateInfo *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const FixedMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const Material3DCreateConfig *config = nullptr);

/**
 * CompileCompositorMaterial — 2D 材质重载
 *
 * 将 Material2DCreateConfig 映射到内部 Material3DCreateConfig 后调用主版本。
 * camera/sky 默认关闭，其余字段从 2D 配置继承。
 */
MaterialCreateInfo *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const FixedMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const Material2DCreateConfig *config);

/**
 * 材质 fallback 工厂辅助宏。（待实现，参见任务 2.3）
 * 先尝试从 recipe 文件加载（file-driven），文件不存在时用 hardcode def。
 * 在 M_Xxx.cpp 工厂函数中使用：
 *
 *   COMPILE_WITH_RECIPE_FALLBACK(profile, BASIC_LIT_BASE_DEF, key,
 *                                "recipes/uber/uber_3d.json", "pc_high")
 *
 * 注：此宏的实现需要完成 ShaderTemplateEngine 与配方驱动编译入口。
 * 当前版本保留宏定义但注释掉，避免前向引用错误。
 */
// #define COMPILE_WITH_RECIPE_FALLBACK(profile_, def_, key_, recipe_path_, quality_) \
//     ([&]() -> MaterialCreateInfo* { \
//         MaterialCreateInfo *_mci = CreateMaterialFromRecipe(profile_, recipe_path_, quality_, def_); \
//         if(_mci) return _mci; \
//         return CompileFixedMaterial(profile_, def_, key_); \
//     }())

}//namespace hgl::graph::mtl
