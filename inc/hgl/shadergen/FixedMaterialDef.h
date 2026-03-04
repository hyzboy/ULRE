#pragma once

/// FixedMaterialDef.h — 编译期材质定义 API
///
/// 面向"极致性能 + 少量固定材质"架构设计。
/// 与 Unreal 材质图那种运行时灵活性相反，这套 API 要求：
///   - 材质的 descriptor set layout、顶点输入、shader 源码在编译期全部确定
///   - 无运行时 GLSL 字符串拼接
///   - 无运行时字符串查找（描述符通过有序数组在材质创建时一次性解析 binding 号）
///
/// 着色器排列组合爆炸问题的解决方案：
///   - 每个可变维度（环境光模型、光照模型等）以 ShaderPermutationAxis 枚举描述
///   - 运行时按 ShaderPermutationKey 选择对应的 GLSL 源码字符串
///   - 每个排列在首次使用时编译一次，之后缓存——等同于现有的 AddDefine + 编译
///   - 排列数 = 各轴枚举值的乘积，但每个排列都是完整 SPV，无运行时分支
///
/// 使用方式：
///   1. 在 M_Xxx.cpp 中定义 constexpr FixedMaterialDef MATERIAL_XXX_DEF { … }
///   2. 调用 CompileComposedBusinessMaterial(dev_attr, DEF, COMPOSED_DEF, LOGIC, key, cfg) 得到 MaterialCreateInfo*
///      （若需直接传 GLSL 源码，则调用 CompileFixedMaterial(dev_attr, DEF, vert_glsl, frag_glsl, key)）
///
/// 渐进迁移路径：
///   - 现有 Std2DMaterial / Std3DMaterial / ShaderCreateInfo 体系继续工作
///   - 新材质优先使用 FixedMaterialDef
///   - 旧材质逐步迁移；全部迁移完成后删除旧体系

#include<hgl/graph/mtl/SkyLight.h>
#include<hgl/graph/mtl/FixedDescriptorEntry.h>
#include<hgl/graph/mtl/FixedVertexEntry.h>
#include<hgl/graph/mtl/FixedMaterialDef.h>
#include<hgl/graph/mtl/ShaderPermutationKey.h>
#include<hgl/vk/VertexAttrib.h>
#include <string>

namespace hgl::graph::mtl{

// ─────────────────────────────────────────────────────────────────────────────
// Shader 排列轴（Permutation Axis）
//
// 每个轴对应一个可独立选择的 shader 功能维度。
// 轴的枚举値直接映射到 GLSL #define 宏名称，由各材质定义自己的 axis 表。
// ShaderPermutationKey 是所有轴的枚举値组合，唯一确定一个 SPV 变体。
// ─────────────────────────────────────────────────────────────────────────────────

// SkyLightAmbientModel 定义在 SkyLight.h（通过文件头 include 已引入）

}//namespace hgl::graph::mtl

