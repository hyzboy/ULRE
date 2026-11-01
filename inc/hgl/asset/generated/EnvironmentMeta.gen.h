// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   属性元数据表：供检查器 GUI、文档导出、严格键校验与通用格式后端消费。
//   它是「结构长什么样」的**唯一运行期真源**，与格式无关。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <cstddef>

#include <hgl/asset/AssetObject.h>
#include <hgl/graph/asset/EnvironmentAssetSpec.h>

namespace hgl::asset::generated
{
    inline const char *AssetEnumName_SkyMode(const hgl::graph::SkyMode v)
    {
        switch (v)
        {
        case hgl::graph::SkyMode::Atmosphere: return "atmosphere";
        case hgl::graph::SkyMode::CubeMapIBL: return "cubemap_ibl";
        case hgl::graph::SkyMode::OctahedralIBL: return "octahedral_ibl";
        case hgl::graph::SkyMode::LambertDome: return "lambert_dome";
        case hgl::graph::SkyMode::SHLightOnly: return "sh9_only";
        case hgl::graph::SkyMode::None: return "none";
        }
        return nullptr;
    }

    inline bool AssetEnumParse_SkyMode(const char *s, hgl::graph::SkyMode &out)
    {
        if (!s) return false;
        if (hgl::stricmp(s, "atmosphere") == 0) { out = hgl::graph::SkyMode::Atmosphere; return true; }
        if (hgl::stricmp(s, "Atmosphere") == 0) { out = hgl::graph::SkyMode::Atmosphere; return true; }
        if (hgl::stricmp(s, "cubemap_ibl") == 0) { out = hgl::graph::SkyMode::CubeMapIBL; return true; }
        if (hgl::stricmp(s, "CubeMapIBL") == 0) { out = hgl::graph::SkyMode::CubeMapIBL; return true; }
        if (hgl::stricmp(s, "octahedral_ibl") == 0) { out = hgl::graph::SkyMode::OctahedralIBL; return true; }
        if (hgl::stricmp(s, "OctahedralIBL") == 0) { out = hgl::graph::SkyMode::OctahedralIBL; return true; }
        if (hgl::stricmp(s, "lambert_dome") == 0) { out = hgl::graph::SkyMode::LambertDome; return true; }
        if (hgl::stricmp(s, "LambertDome") == 0) { out = hgl::graph::SkyMode::LambertDome; return true; }
        if (hgl::stricmp(s, "sh9_only") == 0) { out = hgl::graph::SkyMode::SHLightOnly; return true; }
        if (hgl::stricmp(s, "SHLightOnly") == 0) { out = hgl::graph::SkyMode::SHLightOnly; return true; }
        if (hgl::stricmp(s, "none") == 0) { out = hgl::graph::SkyMode::None; return true; }
        if (hgl::stricmp(s, "None") == 0) { out = hgl::graph::SkyMode::None; return true; }
        return false;
    }

    inline const char *AssetEnumName_SkyAmbientLDR(const hgl::graph::SkyAmbientLDR v)
    {
        switch (v)
        {
        case hgl::graph::SkyAmbientLDR::Auto: return "auto";
        case hgl::graph::SkyAmbientLDR::Off: return "off";
        case hgl::graph::SkyAmbientLDR::On: return "on";
        }
        return nullptr;
    }

    inline bool AssetEnumParse_SkyAmbientLDR(const char *s, hgl::graph::SkyAmbientLDR &out)
    {
        if (!s) return false;
        if (hgl::stricmp(s, "auto") == 0) { out = hgl::graph::SkyAmbientLDR::Auto; return true; }
        if (hgl::stricmp(s, "Auto") == 0) { out = hgl::graph::SkyAmbientLDR::Auto; return true; }
        if (hgl::stricmp(s, "off") == 0) { out = hgl::graph::SkyAmbientLDR::Off; return true; }
        if (hgl::stricmp(s, "Off") == 0) { out = hgl::graph::SkyAmbientLDR::Off; return true; }
        if (hgl::stricmp(s, "on") == 0) { out = hgl::graph::SkyAmbientLDR::On; return true; }
        if (hgl::stricmp(s, "On") == 0) { out = hgl::graph::SkyAmbientLDR::On; return true; }
        return false;
    }

    inline const char *AssetEnumName64_SkyMode(const int64_t v)
    { return AssetEnumName_SkyMode(static_cast<hgl::graph::SkyMode>(v)); }

    inline bool AssetEnumParse64_SkyMode(const char *s, int64_t &out)
    {
        hgl::graph::SkyMode tmp{};
        if (!AssetEnumParse_SkyMode(s, tmp)) return false;
        out = static_cast<int64_t>(tmp);
        return true;
    }

    inline const char *AssetEnumName64_SkyAmbientLDR(const int64_t v)
    { return AssetEnumName_SkyAmbientLDR(static_cast<hgl::graph::SkyAmbientLDR>(v)); }

    inline bool AssetEnumParse64_SkyAmbientLDR(const char *s, int64_t &out)
    {
        hgl::graph::SkyAmbientLDR tmp{};
        if (!AssetEnumParse_SkyAmbientLDR(s, tmp)) return false;
        out = static_cast<int64_t>(tmp);
        return true;
    }

    extern const ClassMetaInfo Class_EnvironmentVisualSpec;
    extern const ClassMetaInfo Class_EnvironmentAmbientSpec;
    extern const ClassMetaInfo Class_EnvironmentIBLSpec;
    extern const ClassMetaInfo Class_EnvironmentLDRSpec;
    extern const ClassMetaInfo Class_EnvironmentSH9BandSpec;
    extern const ClassMetaInfo Class_EnvironmentSH9Spec;
    extern const ClassMetaInfo Class_EnvironmentSpec;

    inline const PropertyMetaInfo Meta_EnvironmentVisualSpec[] =
    {
        { "recipe", "recipe", "recipe", "Default", "", "hgl::serialize::Presence<AnsiString>", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::EnvironmentVisualSpec, recipe), (uint32_t)sizeof(AnsiString), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "geometry", "geometry", "geometry", "Default", "", "hgl::serialize::Presence<AnsiString>", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::EnvironmentVisualSpec, geometry), (uint32_t)sizeof(AnsiString), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "scale", "scale", "scale", "Default", "", "hgl::serialize::Presence<float>", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::EnvironmentVisualSpec, scale), (uint32_t)sizeof(float), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "track_camera", "track_camera", "track_camera", "Default", "", "hgl::serialize::Presence<bool>", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::EnvironmentVisualSpec, track_camera), (uint32_t)sizeof(bool), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_EnvironmentVisualSpec =
    {
        "EnvironmentVisualSpec", "", "`[visual]` 视觉天空。",
        Meta_EnvironmentVisualSpec, (uint32_t)(sizeof(Meta_EnvironmentVisualSpec) / sizeof(Meta_EnvironmentVisualSpec[0])),
        (uint32_t)sizeof(hgl::graph::EnvironmentVisualSpec), (uint32_t)alignof(hgl::graph::EnvironmentVisualSpec)
    };

    inline const PropertyMetaInfo Meta_EnvironmentAmbientSpec[] =
    {
        { "mode", "mode", "mode", "Default", "", "hgl::serialize::Presence<SkyMode>", "hgl::graph::SkyMode", nullptr, PropertyKind::Enum, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::EnvironmentAmbientSpec, mode), (uint32_t)sizeof(hgl::graph::SkyMode), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_SkyMode, &AssetEnumName64_SkyMode },
        { "provider", "provider", "provider", "Default", "", "hgl::serialize::Presence<AnsiString>", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::EnvironmentAmbientSpec, provider), (uint32_t)sizeof(AnsiString), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_EnvironmentAmbientSpec =
    {
        "EnvironmentAmbientSpec", "", "`[ambient]` 天光来源。",
        Meta_EnvironmentAmbientSpec, (uint32_t)(sizeof(Meta_EnvironmentAmbientSpec) / sizeof(Meta_EnvironmentAmbientSpec[0])),
        (uint32_t)sizeof(hgl::graph::EnvironmentAmbientSpec), (uint32_t)alignof(hgl::graph::EnvironmentAmbientSpec)
    };

    inline const PropertyMetaInfo Meta_EnvironmentIBLSpec[] =
    {
        { "irradiance", "irradiance", "irradiance", "Default", "", "hgl::serialize::Presence<OSString>", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::EnvironmentIBLSpec, irradiance), (uint32_t)sizeof(OSString), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "prefilter", "prefilter", "prefilter", "Default", "", "hgl::serialize::Presence<OSString>", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::EnvironmentIBLSpec, prefilter), (uint32_t)sizeof(OSString), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "brdf_lut", "brdf_lut", "brdf_lut", "Default", "", "hgl::serialize::Presence<OSString>", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::EnvironmentIBLSpec, brdf_lut), (uint32_t)sizeof(OSString), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_EnvironmentIBLSpec =
    {
        "EnvironmentIBLSpec", "", "`[ibl]` 烘焙贴图。后缀决定投影与装载方式（`.TexCube` 立方体 / `.TexOct` 八面体 / 其余 2D）。",
        Meta_EnvironmentIBLSpec, (uint32_t)(sizeof(Meta_EnvironmentIBLSpec) / sizeof(Meta_EnvironmentIBLSpec[0])),
        (uint32_t)sizeof(hgl::graph::EnvironmentIBLSpec), (uint32_t)alignof(hgl::graph::EnvironmentIBLSpec)
    };

    inline const PropertyMetaInfo Meta_EnvironmentLDRSpec[] =
    {
        { "mode", "mode", "mode", "Default", "", "hgl::serialize::Presence<SkyAmbientLDR>", "hgl::graph::SkyAmbientLDR", nullptr, PropertyKind::Enum, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::EnvironmentLDRSpec, mode), (uint32_t)sizeof(hgl::graph::SkyAmbientLDR), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_SkyAmbientLDR, &AssetEnumName64_SkyAmbientLDR },
        { "exposure_base", "exposure_base", "exposure_base", "Default", "", "hgl::serialize::Presence<float>", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::EnvironmentLDRSpec, exposure_base), (uint32_t)sizeof(float), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "sun_scale", "sun_scale", "sun_scale", "Default", "", "hgl::serialize::Presence<float>", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::EnvironmentLDRSpec, sun_scale), (uint32_t)sizeof(float), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "min_ambient", "min_ambient", "min_ambient", "Default", "", "hgl::serialize::Presence<float>", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::EnvironmentLDRSpec, min_ambient), (uint32_t)sizeof(float), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_EnvironmentLDRSpec =
    {
        "EnvironmentLDRSpec", "", "`[ldr]` LDR 曝光调制。",
        Meta_EnvironmentLDRSpec, (uint32_t)(sizeof(Meta_EnvironmentLDRSpec) / sizeof(Meta_EnvironmentLDRSpec[0])),
        (uint32_t)sizeof(hgl::graph::EnvironmentLDRSpec), (uint32_t)alignof(hgl::graph::EnvironmentLDRSpec)
    };

    inline const PropertyMetaInfo Meta_EnvironmentSH9BandSpec[] =
    {
        { "r", "r", "r", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::EnvironmentSH9BandSpec, r), (uint32_t)sizeof(decltype(hgl::graph::EnvironmentSH9BandSpec::r)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "g", "g", "g", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::EnvironmentSH9BandSpec, g), (uint32_t)sizeof(decltype(hgl::graph::EnvironmentSH9BandSpec::g)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "b", "b", "b", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::EnvironmentSH9BandSpec, b), (uint32_t)sizeof(decltype(hgl::graph::EnvironmentSH9BandSpec::b)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_EnvironmentSH9BandSpec =
    {
        "EnvironmentSH9BandSpec", "", "`[[sh9.bands]]` 中的一条 L2 球谐系数（线性辐亮度，`.a` 不参与计算故不存）。 数组顺序即基函数顺序（Z-up，与 `ambient_light_sh.glsl` 逐项一致）： ``` [0] Y₀₀ ∝ 1            [3] Y₁₁ ∝ x            [6] Y₂₀ ∝ 3z²-1 [1] Y₁₋₁ ∝ y           [4] Y₂₋₂ ∝ xy          [7] Y₂₁ ∝ xz [2] Y₁₀ ∝ z           [5] Y₂₋₁ ∝ yz          [8] Y₂₂ ∝ x²-y² ```",
        Meta_EnvironmentSH9BandSpec, (uint32_t)(sizeof(Meta_EnvironmentSH9BandSpec) / sizeof(Meta_EnvironmentSH9BandSpec[0])),
        (uint32_t)sizeof(hgl::graph::EnvironmentSH9BandSpec), (uint32_t)alignof(hgl::graph::EnvironmentSH9BandSpec)
    };

    inline const PropertyMetaInfo Meta_EnvironmentSH9Spec[] =
    {
        { "auto_generate", "auto_generate", "auto_generate", "Default", "", "hgl::serialize::Presence<bool>", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::EnvironmentSH9Spec, auto_generate), (uint32_t)sizeof(bool), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "track_sun", "track_sun", "track_sun", "Default", "", "hgl::serialize::Presence<bool>", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::EnvironmentSH9Spec, track_sun), (uint32_t)sizeof(bool), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "sun_irradiance_scale", "sun_irradiance_scale", "sun_irradiance_scale", "Default", "", "hgl::serialize::Presence<float>", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::EnvironmentSH9Spec, sun_irradiance_scale), (uint32_t)sizeof(float), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "exposure", "exposure", "exposure", "Default", "", "hgl::serialize::Presence<float>", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::EnvironmentSH9Spec, exposure), (uint32_t)sizeof(float), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "bands", "bands", "bands", "Default", "", "hgl::serialize::Presence<ValueArray<EnvironmentSH9BandSpec>>", nullptr, nullptr, PropertyKind::ValueArray, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers | PF_Presence, (uint32_t)offsetof(hgl::graph::EnvironmentSH9Spec, bands), (uint32_t)sizeof(hgl::ValueArray<hgl::graph::EnvironmentSH9BandSpec>), 0u, 0.0, 0.0, 0.0, &Class_EnvironmentSH9BandSpec, PropertyKind::Struct, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_EnvironmentSH9Spec =
    {
        "EnvironmentSH9Spec", "", "`[sh9]` 球谐天光。 两条互斥的来源： · `auto = true`  ⇒ 由 `graph::ComputeSkySH9()` 按**主光源**现算（日落时天光 自己变暖变暗），`bands` 被忽略； · `auto = false` ⇒ 用 `bands` 里离线算好的系数（必须正好 9 条）。 ⚠ 解析合成器的三段渐变色（天顶/地平线/地面）由 `EvaluateSkyGradientForSun()` 按太阳仰角插值，**不在**资产里开放 —— 那是\"跟着太阳走\"的实现细节， 写进文件只会得到一个永远被覆盖的键。",
        Meta_EnvironmentSH9Spec, (uint32_t)(sizeof(Meta_EnvironmentSH9Spec) / sizeof(Meta_EnvironmentSH9Spec[0])),
        (uint32_t)sizeof(hgl::graph::EnvironmentSH9Spec), (uint32_t)alignof(hgl::graph::EnvironmentSH9Spec)
    };

    inline const PropertyMetaInfo Meta_EnvironmentSpec[] =
    {
        { "schema", "schema", "schema", "Default", "", "uint32", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::EnvironmentSpec, schema), (uint32_t)sizeof(decltype(hgl::graph::EnvironmentSpec::schema)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "name", "name", "name", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::EnvironmentSpec, name), (uint32_t)sizeof(decltype(hgl::graph::EnvironmentSpec::name)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "description", "description", "description", "Default", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::EnvironmentSpec, description), (uint32_t)sizeof(decltype(hgl::graph::EnvironmentSpec::description)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "visual", "visual", "visual", "Default", "", "EnvironmentVisualSpec", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::EnvironmentSpec, visual), (uint32_t)sizeof(decltype(hgl::graph::EnvironmentSpec::visual)), 0u, 0.0, 0.0, 0.0, &Class_EnvironmentVisualSpec, PropertyKind::None, nullptr, nullptr },
        { "ambient", "ambient", "ambient", "Default", "", "EnvironmentAmbientSpec", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::EnvironmentSpec, ambient), (uint32_t)sizeof(decltype(hgl::graph::EnvironmentSpec::ambient)), 0u, 0.0, 0.0, 0.0, &Class_EnvironmentAmbientSpec, PropertyKind::None, nullptr, nullptr },
        { "ibl", "ibl", "ibl", "Default", "", "EnvironmentIBLSpec", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::EnvironmentSpec, ibl), (uint32_t)sizeof(decltype(hgl::graph::EnvironmentSpec::ibl)), 0u, 0.0, 0.0, 0.0, &Class_EnvironmentIBLSpec, PropertyKind::None, nullptr, nullptr },
        { "ldr", "ldr", "ldr", "Default", "", "EnvironmentLDRSpec", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::EnvironmentSpec, ldr), (uint32_t)sizeof(decltype(hgl::graph::EnvironmentSpec::ldr)), 0u, 0.0, 0.0, 0.0, &Class_EnvironmentLDRSpec, PropertyKind::None, nullptr, nullptr },
        { "sh9", "sh9", "sh9", "Default", "", "EnvironmentSH9Spec", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::EnvironmentSpec, sh9), (uint32_t)sizeof(decltype(hgl::graph::EnvironmentSpec::sh9)), 0u, 0.0, 0.0, 0.0, &Class_EnvironmentSH9Spec, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_EnvironmentSpec =
    {
        "EnvironmentSpec", "", "一份环境资产文件的完整字段规约。",
        Meta_EnvironmentSpec, (uint32_t)(sizeof(Meta_EnvironmentSpec) / sizeof(Meta_EnvironmentSpec[0])),
        (uint32_t)sizeof(hgl::graph::EnvironmentSpec), (uint32_t)alignof(hgl::graph::EnvironmentSpec)
    };

    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::EnvironmentVisualSpec *) { return &Class_EnvironmentVisualSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::EnvironmentAmbientSpec *) { return &Class_EnvironmentAmbientSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::EnvironmentIBLSpec *) { return &Class_EnvironmentIBLSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::EnvironmentLDRSpec *) { return &Class_EnvironmentLDRSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::EnvironmentSH9BandSpec *) { return &Class_EnvironmentSH9BandSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::EnvironmentSH9Spec *) { return &Class_EnvironmentSH9Spec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::EnvironmentSpec *) { return &Class_EnvironmentSpec; }
} // namespace hgl::asset::generated
