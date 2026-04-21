#pragma once

/// LightingModel.h — 光照模型唯一权威定义
///
/// C++ 枚举与 GLSL 光照实现文件的对应关系：
///
///   C++ LightingModel           GLSL 文件
///   ──────────────────────      ─────────────────────────────────
///   Lambert          = 0  →    common/lighting_lambert.glsl
///   BlinnPhong       = 1  →    common/lighting_blinn_phong.glsl
///   PBR              = 2  →    common/lighting_pbr.glsl
///
/// 由 CompositorAssembler 通过 GetLightingModelGLSLPath + ShaderWriter.EmitInclude 选择并拼接。

#include<hgl/CoreType.h>
#include<hgl/type/EnumUtil.h>

namespace hgl::graph::mtl{

enum class LightingModel : uint8
{
    Lambert     = 0,    ///< 纯漫反射 Lambert（最低开销）
    BlinnPhong  = 1,    ///< Blinn-Phong 高光（中低配）
    PBR         = 2,    ///< Cook-Torrance GGX PBR（高配）

    ENUM_CLASS_RANGE(Lambert, PBR)
};

/// 返回 LightingModel 对应的 GLSL 实现文件路径（相对于 ShaderLibrary）
inline const char *GetLightingModelGLSLPath(LightingModel model)
{
    switch (model)
    {
        case LightingModel::Lambert:    return "common/lighting_lambert.glsl";
        case LightingModel::BlinnPhong: return "common/lighting_blinn_phong.glsl";
        case LightingModel::PBR:        return "common/lighting_pbr.glsl";
        default:                         return "common/lighting_lambert.glsl";
    }
}

} // namespace hgl::graph::mtl
