#pragma once

#include<hgl/CoreType.h>
#include<hgl/type/EnumUtil.h>    // ENUM_CLASS_RANGE

namespace hgl::graph::mtl
{

/**
 * InstanceDataLayout — MaterialInstance 数据格式的语义级契约
 *
 * 用于替代隐式 byte-stride 匹配，作为 Domain（供方）和 MaterialTemplate（需方）
 * 之间的语义契约。DomainMaterialBinding 校验 enum 相等而非字节长度相等，
 * 从而避免不同语义布局碰巧 stride 相同导致的静默错误。
 *
 * 对应 GLSL 权威结构体文件：ShaderLibrary/instance_data/*.glsl
 */
enum class InstanceDataLayout : uint8
{
    None = 0,           ///< 无实例数据 (stride = 0)
    Color4f,            ///< vec4 color — Gizmo3D, Unlit 着色 (16B)
    PBRColor,           ///< uint base_color + float metallic + float roughness — PBR 打包色 (12B)
    PBRStandard,        ///< uint base_color + float metallic + float roughness + float normal_scale — PBR 标准 (16B)
    TextureBlinnPhong,  ///< float normal_strength — 纹理 Blinn-Phong (4B)
    Text2D,             ///< uint TextColor — 文字 packed RGBA (4B)
    BillboardFixed,     ///< uvec2 BillboardSize — 广告牌固定/动态尺寸 (8B)

    ENUM_CLASS_RANGE(None, BillboardFixed)
};

struct InstanceDataLayoutInfo
{
    uint32 stride;          ///< 单个实例数据字节数
    const char *name;       ///< 调试名
    const char *glsl_name;  ///< 对应 GLSL struct 注释名（用于文档/校验）
};

inline constexpr InstanceDataLayoutInfo kInstanceDataLayouts[] =
{
    /* None              */ { 0,  "None",              nullptr },
    /* Color4f           */ { 16, "Color4f",           "MaterialInstance_Color4f" },
    /* PBRColor          */ { 12, "PBRColor",          "MaterialInstance_PBRColor" },
    /* PBRStandard       */ { 16, "PBRStandard",       "MaterialInstance_PBRStandard" },
    /* TextureBlinnPhong */ { 4,  "TextureBlinnPhong", "MaterialInstance_TextureBlinnPhong" },
    /* Text2D            */ { 4,  "Text2D",            "MaterialInstance_Text2D" },
    /* BillboardFixed    */ { 8,  "BillboardFixed",    "MaterialInstance_BillboardFixed" },
};

static_assert(sizeof(kInstanceDataLayouts) / sizeof(kInstanceDataLayouts[0])
              == size_t(InstanceDataLayout::RANGE_SIZE),
              "kInstanceDataLayouts must match InstanceDataLayout enum");

inline constexpr uint32 GetInstanceDataStride(InstanceDataLayout layout)
{
    return kInstanceDataLayouts[uint8(layout)].stride;
}

inline constexpr const char *GetInstanceDataName(InstanceDataLayout layout)
{
    return kInstanceDataLayouts[uint8(layout)].name;
}

/**
 * 从字节步长反向查找对应的 InstanceDataLayout 枚举值。
 *
 * 注意：Color4f(16B) 与 PBRStandard(16B) stride 相同，该函数返回第一个匹配项 (Color4f)。
 * 此碰撞问题将在 MCI 层提供显式 layout 标识后消除（Phase C 后续）。
 */
inline constexpr InstanceDataLayout ResolveInstanceDataLayout(uint32 stride)
{
    if(stride == 0)
        return InstanceDataLayout::None;
    for(uint8 i = 1; i < uint8(InstanceDataLayout::RANGE_SIZE); ++i)
    {
        if(kInstanceDataLayouts[i].stride == stride)
            return InstanceDataLayout(i);
    }
    return InstanceDataLayout::None;
}

} // namespace hgl::graph::mtl
