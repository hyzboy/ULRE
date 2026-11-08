#include <hgl/graph/asset/EnvironmentAsset.h>
#include <hgl/asset/generated/EnvironmentMeta.gen.h>
#include <hgl/asset/generated/EnvironmentSerialize.gen.h>
#include <hgl/utf.h>

namespace hgl::graph
{
    namespace
    {
        /// UTF-8 字节串 ⇒ `U8String`（`HGL_SUPPORT_CHAR8_T` 未开时没有现成转换）
        U8String ToU8(const AnsiString &str)
        {
            return U8String(reinterpret_cast<const u8char *>(str.c_str()), str.Length());
        }

        /// `OSString` ⇒ UTF-8 字节串（`ToU8String` 给的是 `U8String`，这里要 `AnsiString`）
        AnsiString ToAnsi(const OSString &str)
        {
            const U8String u8 = ToU8String(str);
            return AnsiString(reinterpret_cast<const char *>(u8.c_str()), u8.Length());
        }

        /// 目标键缺失且来源键存在 ⇒ 整份拷贝（含 present 位）
        template<typename T>
        void FillIfAbsent(hgl::serialize::Presence<T> &dst, const hgl::serialize::Presence<T> &src)
        {
            if (!dst.present && src.present)
                dst = src;
        }

        /// 任一子键存在 ⇒ 段本身标记为「出现过」
        bool BaseHasAny(const EnvironmentBaseSpec &b)
        {
            return b.geometry.present || b.scale.present || b.track_camera.present;
        }

        bool TechHasAny(const EnvironmentTechniqueSpec &t)
        {
            return t.recipe.present || t.geometry.present || t.scale.present
                || t.irradiance.present || t.prefilter.present || t.brdf_lut.present
                || t.ldr_mode.present || t.ldr_exposure_base.present
                || t.ldr_sun_scale.present || t.ldr_min_ambient.present || t.provider.present;
        }
    }//namespace

    /* ── schema 2 归一化与段选择 ──────────────────────────────────────────── */

    void NormalizeEnvironmentSpec(EnvironmentSpec &spec)
    {
        // [base] ← [visual]：共用视觉身份
        FillIfAbsent(spec.base.value.geometry,       spec.visual.geometry);
        FillIfAbsent(spec.base.value.scale,          spec.visual.scale);
        FillIfAbsent(spec.base.value.track_camera,   spec.visual.track_camera);
        if (BaseHasAny(spec.base.value))
            spec.base.present = true;

        // 默认技术 = [ambient].mode；缺键取 SHLightOnly（与旧缺省一致，无视觉段）
        const SkyMode mode = spec.ambient.mode.present
                           ? spec.ambient.mode.value
                           : SkyMode::SHLightOnly;

        hgl::serialize::Presence<EnvironmentTechniqueSpec> *wsec = nullptr;
        switch (mode)
        {
        case SkyMode::Atmosphere:    wsec = &spec.atmosphere;  break;
        case SkyMode::CubeMapIBL:    wsec = &spec.cubemap;     break;
        case SkyMode::OctahedralIBL: wsec = &spec.octahedral;  break;
        case SkyMode::LambertDome:   wsec = &spec.lambert_dome; break;
        default: break; // SHLightOnly / None：没有视觉技术段
        }
        if (!wsec)
            return;

        EnvironmentTechniqueSpec &sec = wsec->value;

        // 旧字段镜像进默认技术段（只填不清；schema 2 手写的键优先）
        FillIfAbsent(sec.recipe,            spec.visual.recipe);
        FillIfAbsent(sec.geometry,          spec.visual.geometry);
        FillIfAbsent(sec.scale,             spec.visual.scale);
        FillIfAbsent(sec.irradiance,        spec.ibl.irradiance);
        FillIfAbsent(sec.prefilter,         spec.ibl.prefilter);
        FillIfAbsent(sec.brdf_lut,          spec.ibl.brdf_lut);
        FillIfAbsent(sec.ldr_mode,          spec.ldr.mode);
        FillIfAbsent(sec.ldr_exposure_base, spec.ldr.exposure_base);
        FillIfAbsent(sec.ldr_sun_scale,     spec.ldr.sun_scale);
        FillIfAbsent(sec.ldr_min_ambient,   spec.ldr.min_ambient);
        FillIfAbsent(sec.provider,          spec.ambient.provider);
        if (TechHasAny(sec))
            wsec->present = true;
    }

    const EnvironmentTechniqueSpec *FindTechniqueSection(const EnvironmentSpec &spec, SkyMode mode)
    {
        switch (mode)
        {
        case SkyMode::Atmosphere:    return spec.atmosphere.present ? &spec.atmosphere.value : nullptr;
        case SkyMode::CubeMapIBL:    return spec.cubemap.present    ? &spec.cubemap.value    : nullptr;
        case SkyMode::OctahedralIBL: return spec.octahedral.present ? &spec.octahedral.value : nullptr;
        case SkyMode::LambertDome:   return spec.lambert_dome.present ? &spec.lambert_dome.value : nullptr;
        default:                     return nullptr; // SHLightOnly / None 共用顶层 [sh9]
        }
    }

    EnvironmentAsset::EnvironmentAsset(const EnvironmentSpec &in_spec)
        : spec(in_spec)
    {
        asset_name = spec.name;
    }

    EnvironmentAsset::EnvironmentAsset(EnvironmentSpec &&in_spec)
        : spec(std::move(in_spec))
    {
        asset_name = spec.name;
    }

    void EnvironmentAsset::SetSpec(const EnvironmentSpec &in_spec)
    {
        spec = in_spec;
        asset_name = spec.name;
    }

    void EnvironmentAsset::SetSpec(EnvironmentSpec &&in_spec)
    {
        spec = std::move(in_spec);
        asset_name = spec.name;
    }

    const hgl::asset::ClassMetaInfo *EnvironmentAsset::GetClassMetaInfo() const
    {
        return &hgl::asset::generated::Class_EnvironmentSpec;
    }

    bool EnvironmentAsset::Serialize(hgl::serialize::Archive &ar)
    {
        hgl::serialize::StructSerializer<EnvironmentSpec>::Do(ar, spec);

        if (!ar.Ok())
            return false;

        // 读方向可能改掉 `name`；名字是资产身份的一部分，必须跟着同步。
        asset_name = spec.name;
        return true;
    }

    void EnvironmentAsset::SetSource(const os_char *label)
    {
        source_path = label ? label : OS_TEXT("");

        // 身份按「有名字优先」生成：名字是作者给的稳定键，路径只是它被发现的位置。
        // 两者都保留，因为名字查询命中后仍需知道原始文件（热重载 / 报错定位）。
        if (spec.name.IsEmpty())
            resource_id = ToU8(AnsiString("env:file:") + ToAnsi(source_path));
        else
            resource_id = ToU8(AnsiString("env:@") + spec.name);

        if (asset_name.IsEmpty())
            asset_name = source_path.IsEmpty() ? AnsiString("<memory>") : ToAnsi(source_path);
    }
}//namespace hgl::graph
