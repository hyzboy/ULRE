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
#include<hgl/vk/VertexAttrib.h>
#include<hgl/vk/VKDescriptorSetType.h>
#include<hgl/vk/VKPrimitiveType.h>
#include <string>

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
// Shader 排列轴（Permutation Axis）
//
// 每个轴对应一个可独立选择的 shader 功能维度。
// 轴的枚举値直接映射到 GLSL #define 宏名称，由各材质定义自己的 axis 表。
// ShaderPermutationKey 是所有轴的枚举値组合，唯一确定一个 SPV 变体。
// ─────────────────────────────────────────────────────────────────────────────────

// SkyLightAmbientModel 定义在 SkyLight.h（通过文件头 include 已引入）

/// 直接光照模型轴
enum class LightModel : uint8
{
    Unlit       = 0,    ///<无光照（UI、天空、Gizmo）
    Lambert,            ///<纯漫反射（手机低配）
    BlinnPhong,         ///<Blinn-Phong（手机高配 / PC 低配）
    PBR_Lite,           ///<简化 PBR（GGX diffuse + 近似 specular，无 split-sum）
    PBR_Full,           ///<完整 PBR（GGX + split-sum IBL，仅 PC 高配）
    CelShading,         ///<卡通渲染（threshold 漫反射 + rim light）

    ENUM_CLASS_RANGE(Unlit, CelShading)
};

/// 高光拆分轴（仅对 BlinnPhong / PBR 有意义）
enum class SpecularChannel : uint8
{
    Combined    = 0,    ///<漫反射 + 高光合并输出（单 RT，手机默认）
    Separated,          ///<漫反射 / 高光分离输出（双 RT，延迟渲染前向 G-Buffer pass）

    ENUM_CLASS_RANGE(Combined, Separated)
};

/// 阴影接收轴
enum class ShadowReceive : uint8
{
    None        = 0,    ///<不接收阴影
    PCF,                ///<PCF 软阴影
    PCSS,               ///<PCSS 接触软化（仅 PC 高配）

    ENUM_CLASS_RANGE(None, PCSS)
};

// ─────────────────────────────────────────────────────────────────────────────
// ShaderPermutationKey：所有轴的组合，唯一标识一个 SPV 变体
// ─────────────────────────────────────────────────────────────────────────────
struct ShaderPermutationKey
{
    SkyLightAmbientModel ambient = SkyLightAmbientModel::Simple;
    LightModel      light       = LightModel::BlinnPhong;
    SpecularChannel specular    = SpecularChannel::Combined;
    ShadowReceive   shadow      = ShadowReceive::None;

    /// 转换为 uint32_t 用于哈希/缓存 key
    uint32_t ToU32() const
    {
        return  (uint32_t(ambient)  <<  0) |
                (uint32_t(light)    <<  8) |
                (uint32_t(specular) << 16) |
                (uint32_t(shadow)   << 24);
    }

    bool operator==(const ShaderPermutationKey &o) const { return ToU32() == o.ToU32(); }
    bool operator!=(const ShaderPermutationKey &o) const { return !(*this == o); }
    bool operator< (const ShaderPermutationKey &o) const { return ToU32() <  o.ToU32(); }

    /// 生成对应的 GLSL #define 列表，写入 defines_out（格式："#define MACRO_NAME value\n"）
    void AppendGLSLDefines(std::string &defines_out) const;
};//struct ShaderPermutationKey

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
};//struct FixedMaterialDef

}//namespace hgl::graph::mtl

