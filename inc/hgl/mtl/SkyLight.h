#pragma once

/// SkyLight.h — 天光模型唯一权威定义
///
/// 将 C++ 排列枚举与 GLSL 宏数值统一到一处，消除 ShaderPermutationKey.cpp
/// 中的硬编码映射表，并作为 MFSkyLight.h 的公共依赖。
///
/// 文件用途：
///   - 公共头文件（inc/），C++ 侧均通过 #include<hgl/mtl/SkyLight.h> 引用
///   - GLSL 侧由合成器注入 SKYLIGHT_GLSL_HEADER + GetSkyLightModelImplGLSL() 提供宏常量与模型实现（详见 MFSkyLight.h）
///
/// GLSL 数值与 C++ 枚举的对应关系（保持两侧同步）：
///
///   C++ SkyLightAmbientModel          GLSL ULRE_SKYLIGHT_MODEL_*
///   ─────────────────────────────     ─────────────────────────
///   Simple             = 0  →  ULRE_SKYLIGHT_MODEL_SIMPLE    = 1  (仰角渐变)
///   FakeAtmosphere     = 1  →  ULRE_SKYLIGHT_MODEL_FAKE_ATM  = 2  (假大气渲染)
///   CubeMap            = 2  →  ULRE_SKYLIGHT_MODEL_CUBEMAP   = 3  (普通 CubeMap)
///   SphericalHarmonics = 3  →  ULRE_SKYLIGHT_MODEL_SH        = 4  (球谐)
///   IBL                = 4  →  ULRE_SKYLIGHT_MODEL_IBL       = 5  (IBL)
///
///   GLSL 值 = C++ 枚举値 + 1（可直接用 uint32_t(m)+1 计算，无需映射表）

#include<hgl/CoreType.h>            // uint8, uint32
#include<hgl/type/EnumUtil.h>        // ENUM_CLASS_RANGE

namespace hgl::graph::mtl{

// ─────────────────────────────────────────────────────────────────────────────
// C++ 排列轴枚举
// ─────────────────────────────────────────────────────────────────────────────

/// 天光环境光模型轴
/// 直接对应 GLSL 端 ULRE_SKYLIGHT_MODEL_* 常量（见下方 SKYLIGHT_GLSL_* 常量）
/// 规则：GLSL 小数层质越低，编号 = C++ 枚举値 + 1
enum class SkyLightAmbientModel : uint8
{
    Simple              = 0,    ///<仰角渐变 exp2(仰角)*天空基色（默认，最低开销）
    FakeAtmosphere      = 1,    ///<Shader 内联假大气散射渲染（中低配）
    CubeMap             = 2,    ///<普通 CubeMap 采样（静态或 ComputeShader 大气输出均可）
    SphericalHarmonics  = 3,    ///<球谐近似 SH（无 CubeMap lookup）
    IBL                 = 4,    ///<基于图像的光照 IBL（需要 CubeMap；多个 IBL 会由 ComputeShader 混合，此处只取一张）

    ENUM_CLASS_RANGE(Simple, IBL)
};

/// SkyLight 各模型的数据需求矩阵（框架层声明，不含具体实现）
/// 说明：
///   - need_sky_info_ubo: 该模型是否需要 SkyInfo（当前全部为 true）
///   - need_sky_cubemap: 是否需要单环境 CubeMap（CubeMap 模型）
///   - need_sh_ubo: 是否需要 SH 系数 UBO（SphericalHarmonics 模型）
///   - need_ibl_cubemap: 是否需要 IBL CubeMap（IBL 模型）
struct SkyLightDataRequirement
{
    bool need_sky_info_ubo = true;
    bool need_sky_cubemap = false;
    bool need_sh_ubo = false;
    bool need_ibl_cubemap = false;

    // 统一资源键名（为空表示当前模型不需要该资源）
    const char *sky_info_ubo_name = "sky";
    const char *sky_cubemap_name = nullptr;
    const char *sh_ubo_name = nullptr;
    const char *ibl_cubemap_name = nullptr;
};

// 统一资源键名常量（单一来源）
// 说明：
//   - *_LITERAL 供 C/C++ 预处理器拼接 GLSL 字符串时使用
//   - constexpr 变量供 C++ 运行时/结构体字段赋值使用
#define SKYLIGHT_RESOURCE_KEY_SKY_INFO_UBO_LITERAL "sky"
#define SKYLIGHT_RESOURCE_KEY_SKY_CUBEMAP_LITERAL  "SkyCubeMap"
#define SKYLIGHT_RESOURCE_KEY_SKY_SH_UBO_LITERAL   "SkySH"
#define SKYLIGHT_RESOURCE_KEY_SKY_IBL_LITERAL      "SkyIBL"

constexpr const char *SKYLIGHT_RESOURCE_KEY_SKY_INFO_UBO = SKYLIGHT_RESOURCE_KEY_SKY_INFO_UBO_LITERAL;
constexpr const char *SKYLIGHT_RESOURCE_KEY_SKY_CUBEMAP  = SKYLIGHT_RESOURCE_KEY_SKY_CUBEMAP_LITERAL;
constexpr const char *SKYLIGHT_RESOURCE_KEY_SKY_SH_UBO   = SKYLIGHT_RESOURCE_KEY_SKY_SH_UBO_LITERAL;
constexpr const char *SKYLIGHT_RESOURCE_KEY_SKY_IBL      = SKYLIGHT_RESOURCE_KEY_SKY_IBL_LITERAL;

inline SkyLightDataRequirement GetSkyLightDataRequirement(const SkyLightAmbientModel model)
{
    switch(model)
    {
        case SkyLightAmbientModel::CubeMap:
            return SkyLightDataRequirement{
                true, true, false, false,
                SKYLIGHT_RESOURCE_KEY_SKY_INFO_UBO,
                SKYLIGHT_RESOURCE_KEY_SKY_CUBEMAP,
                nullptr,
                nullptr
            };

        case SkyLightAmbientModel::SphericalHarmonics:
            return SkyLightDataRequirement{
                true, false, true, false,
                SKYLIGHT_RESOURCE_KEY_SKY_INFO_UBO,
                nullptr,
                SKYLIGHT_RESOURCE_KEY_SKY_SH_UBO,
                nullptr
            };

        case SkyLightAmbientModel::IBL:
            return SkyLightDataRequirement{
                true, false, false, true,
                SKYLIGHT_RESOURCE_KEY_SKY_INFO_UBO,
                nullptr,
                nullptr,
                SKYLIGHT_RESOURCE_KEY_SKY_IBL
            };

        case SkyLightAmbientModel::Simple:
        case SkyLightAmbientModel::FakeAtmosphere:
        default:
            return SkyLightDataRequirement{
                true, false, false, false,
                SKYLIGHT_RESOURCE_KEY_SKY_INFO_UBO,
                nullptr,
                nullptr,
                nullptr
            };
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// C++ 侧的 GLSL 模型数值常量
// 与 MFSkyLight.h 内 SKYLIGHT_GLSL_HEADER 的 #define ULRE_SKYLIGHT_MODEL_* 保持一致
// ShaderPermutationKey::AppendGLSLDefines 通过下表映射，避免 magic number
// ─────────────────────────────────────────────────────────────────────────────

// GLSL 层数将和 C++ 枚举値保持 +1 关系，修改枚举时必须同步修改以下局部常量和 MFSkyLight.h
constexpr uint32_t SKYLIGHT_GLSL_SIMPLE   = 1;   ///< ULRE_SKYLIGHT_MODEL_SIMPLE
constexpr uint32_t SKYLIGHT_GLSL_FAKE_ATM = 2;   ///< ULRE_SKYLIGHT_MODEL_FAKE_ATM
constexpr uint32_t SKYLIGHT_GLSL_CUBEMAP  = 3;   ///< ULRE_SKYLIGHT_MODEL_CUBEMAP
constexpr uint32_t SKYLIGHT_GLSL_SH       = 4;   ///< ULRE_SKYLIGHT_MODEL_SH
constexpr uint32_t SKYLIGHT_GLSL_IBL      = 5;   ///< ULRE_SKYLIGHT_MODEL_IBL

/// 将 SkyLightAmbientModel 枚举値转换为对应的 GLSL ULRE_SKYLIGHT_MODEL_* 数値
/// 规则：GLSL 小 = uint32_t(m) + 1，无需 switch 映射表
inline uint32_t SkyLightAmbientModelToGLSL(SkyLightAmbientModel m)
{
    return uint32_t(m) + 1;
}

}//namespace hgl::graph::mtl
