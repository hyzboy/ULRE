#pragma once

/// SkyLight.h — 天光模型唯一权威定义
///
/// 将 C++ 排列枚举与 GLSL 宏数值统一到一处，消除 ShaderPermutationKey.cpp
/// 中的硬编码映射表，并作为 MFSkyLight.h 的公共依赖。
///
/// 文件用途：
///   - 公共头文件（inc/），C++ 侧均通过 #include<hgl/graph/mtl/SkyLight.h> 引用
///   - GLSL 侧通过注入 ULRE_SKYLIGHT_GLSL_COMMON 字符串获得宏定义与辅助函数
///
/// GLSL 数值与 C++ 枚举的对应关系（保持两侧同步）：
///
///   C++ SkyLightAmbientModel          GLSL ULRE_SKYLIGHT_MODEL_*
///   ─────────────────────────────     ─────────────────────────
///   Simple              = 0     →     ULRE_SKYLIGHT_MODEL_SIMPLE   = 1
///   IBL                 = 1     →     ULRE_SKYLIGHT_MODEL_IBL      = 2
///   ENVMAP              (预留)  →     ULRE_SKYLIGHT_MODEL_ENVMAP   = 3
///   SphericalHarmonics  = 2     →     ULRE_SKYLIGHT_MODEL_SH       = 4

#include<hgl/CoreType.h>            // uint8, uint32
#include<hgl/type/EnumUtil.h>        // ENUM_CLASS_RANGE

namespace hgl::graph::mtl{

// ─────────────────────────────────────────────────────────────────────────────
// C++ 排列轴枚举
// ─────────────────────────────────────────────────────────────────────────────

/// 天光环境光模型轴
/// 直接对应 GLSL 端 ULRE_SKYLIGHT_MODEL_* 常量（见下方 SKYLIGHT_GLSL_* 常量）
enum class SkyLightAmbientModel : uint8
{
    Simple              = 0,    ///<简单半球环境光（默认），→ ULRE_SKYLIGHT_MODEL_SIMPLE(1)
    IBL                 = 1,    ///<基于图像的光照（IBL，需要 CubeMap），→ ULRE_SKYLIGHT_MODEL_IBL(2)
    SphericalHarmonics  = 2,    ///<球谐近似（低频 IBL，无 CubeMap lookup），→ ULRE_SKYLIGHT_MODEL_SH(4)

    ENUM_CLASS_RANGE(Simple, SphericalHarmonics)
};

// ─────────────────────────────────────────────────────────────────────────────
// C++ 侧的 GLSL 模型数值常量
// 与 ULRE_SKYLIGHT_GLSL_COMMON 内 #define ULRE_SKYLIGHT_MODEL_* 保持一致
// ShaderPermutationKey::AppendGLSLDefines 通过下表映射，避免 magic number
// ─────────────────────────────────────────────────────────────────────────────

constexpr uint32_t SKYLIGHT_GLSL_SIMPLE = 1;   ///< ULRE_SKYLIGHT_MODEL_SIMPLE
constexpr uint32_t SKYLIGHT_GLSL_IBL    = 2;   ///< ULRE_SKYLIGHT_MODEL_IBL
constexpr uint32_t SKYLIGHT_GLSL_ENVMAP = 3;   ///< ULRE_SKYLIGHT_MODEL_ENVMAP（接口预留）
constexpr uint32_t SKYLIGHT_GLSL_SH     = 4;   ///< ULRE_SKYLIGHT_MODEL_SH

/// 将 SkyLightAmbientModel 枚举值转换为对应的 GLSL ULRE_SKYLIGHT_MODEL_* 数值
inline uint32_t SkyLightAmbientModelToGLSL(SkyLightAmbientModel m)
{
    switch(m)
    {
        case SkyLightAmbientModel::IBL:                return SKYLIGHT_GLSL_IBL;
        case SkyLightAmbientModel::SphericalHarmonics: return SKYLIGHT_GLSL_SH;
        case SkyLightAmbientModel::Simple:
        default:                                       return SKYLIGHT_GLSL_SIMPLE;
    }
}

}//namespace hgl::graph::mtl
