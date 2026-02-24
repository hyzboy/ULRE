#pragma once

/// FixedMaterialDef.h — 编译期材质定义 API
///
/// 面向"极致性能 + 少量固定材质"架构设计。
/// 与 Unreal 材质图那种运行时灵活性相反，这套 API 要求：
///   - 材质的 descriptor set layout、顶点输入、shader 源码在编译期全部确定
///   - 无运行时 GLSL 字符串拼接
///   - 无运行时字符串查找（描述符通过有序数组在材质创建时一次性解析 binding 号）
///
/// 使用方式：
///   1. 在 M_Xxx.cpp 中定义 constexpr FixedMaterialDef MATERIAL_XXX_DEF { … }
///   2. 调用 CompileFixedMaterial(dev_attr, MATERIAL_XXX_DEF) 得到 MaterialCreateInfo*
///
/// 渐进迁移路径：
///   - 现有 Std2DMaterial / Std3DMaterial / ShaderCreateInfo 体系继续工作
///   - 新材质优先使用 FixedMaterialDef
///   - 旧材质逐步迁移；全部迁移完成后删除旧体系

#include<hgl/vk/VertexAttrib.h>
#include<hgl/vk/VKDescriptorSetType.h>
#include<hgl/graph/mtl/PrimitiveType.h>
#include<stdint.h>

namespace hgl::graph::mtl{

// ─────────────────────────────────────────────────────────────────────────────
// 描述符类别
// ─────────────────────────────────────────────────────────────────────────────
enum class DescriptorKind : uint8
{
    UBO,            ///<uniform buffer
    SSBO,           ///<storage buffer
    Texture,        ///<sampled image（无采样器）
    TextureSampler, ///<combined image sampler
};

// ─────────────────────────────────────────────────────────────────────────────
// 单个描述符项（编译期常量，sizeof=32 字节）
// ─────────────────────────────────────────────────────────────────────────────
struct FixedDescriptorEntry
{
    DescriptorSetType   set_type;       ///<所属 descriptor set 类型
    DescriptorKind      kind;           ///<UBO / SSBO / Texture / TextureSampler
    uint32_t            stage_flags;    ///<VkShaderStageFlagBits 组合
    const char *        name;           ///<绑定名称（binding=N 解析用，仅 debug 时字符串查找）
    const char *        struct_name;    ///<GLSL 结构体名称（UBO/SSBO 用），Texture 类型填 nullptr
    const char *        glsl_type;      ///<GLSL 采样器类型（Texture 用），UBO/SSBO 类型填 nullptr
};//struct FixedDescriptorEntry

// ─────────────────────────────────────────────────────────────────────────────
// 单个顶点输入项（编译期常量）
// ─────────────────────────────────────────────────────────────────────────────
struct FixedVertexEntry
{
    VAType              type;           ///<顶点属性类型（VAT_VEC3 等）
    VertexInputGroup    group;          ///<分组（Basic / TransformID / MaterialInstanceID 等）
    VkVertexInputRate   input_rate;     ///<VK_VERTEX_INPUT_RATE_VERTEX 或 _INSTANCE
    const char *        name;           ///<顶点属性名称（VAN::Position 等）
};//struct FixedVertexEntry

// ─────────────────────────────────────────────────────────────────────────────
// 完整材质定义（编译期常量结构体）
// ─────────────────────────────────────────────────────────────────────────────
struct FixedMaterialDef
{
    // ── 标识 ──────────────────────────────────────────────────────────────────
    const char *                name;                   ///<材质名称（调试用）

    // ── 图元类型 ──────────────────────────────────────────────────────────────
    PrimitiveType               primitive_type;

    // ── 顶点输入 ──────────────────────────────────────────────────────────────
    const FixedVertexEntry *    vertex_entries;         ///<顶点输入数组
    uint32_t                    vertex_entry_count;     ///<顶点输入数量

    // ── 描述符布局 ────────────────────────────────────────────────────────────
    const FixedDescriptorEntry *descriptor_entries;     ///<描述符数组（按 set_type 分组，组内按 binding 序）
    uint32_t                    descriptor_entry_count; ///<描述符数量

    // ── MaterialInstance 数据 ─────────────────────────────────────────────────
    const char *                mi_glsl_codes;          ///<GLSL struct 成员代码；nullptr = 无 MI
    uint32_t                    mi_struct_bytes;        ///<sizeof(MIData)；0 = 无 MI

    // ── GLSL 源码（完整 GLSL 文件，非片段）────────────────────────────────────
    const char *                vert_glsl;              ///<完整 vertex shader 源码
    const char *                geom_glsl;              ///<完整 geometry shader 源码；nullptr = 无 GS
    const char *                frag_glsl;              ///<完整 fragment shader 源码
};//struct FixedMaterialDef

}//namespace hgl::graph::mtl
