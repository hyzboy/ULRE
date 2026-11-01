// TestEnvironmentCodec.cpp —— 环境资产规约（`*.env.toml`）的字段契约门禁：
//
//   1. 五组字段（visual / ambient / ibl / ldr / sh9）完整「文本 → 规约 → 文本」往返稳定；
//   2. `Presence<T>` 三态：缺键 / 显式写缺省值 / 显式写非缺省值；
//   3. 枚举拼写：`cubemap_ibl` 与 `CubeMapIBL` 都接受，写出统一用别名，非法拼写必须失败；
//   4. `[[sh9.bands]]` 数组顺序即基函数顺序，`[]` ≠ 缺键；
//   5. 类型不符必须失败（不能静默吞掉）；
//   6. 元数据：`struct_meta` 递归接线 + 容器 `elem_kind` + `PF_Presence` 旗标。
//
// 只测「规约 ↔ 文本」这一层：跨字段业务规则（`schema` 只接受 1、`auto_generate == false`
// 时 `bands` 必须正好 9 条、空字符串回落缺省贴图）属于解析层，不在这里断言。
//
// 纯数据往返，不依赖设备 / 窗口，因此在 CI 里也能跑。
#include <hgl/asset/AssetObject.h>
#include <hgl/asset/generated/EnvironmentMeta.gen.h>
#include <hgl/asset/generated/EnvironmentSerialize.gen.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/graph/asset/EnvironmentAsset.h>
#include <hgl/graph/asset/EnvironmentAssetFlat.h>
#include <hgl/graph/asset/EnvironmentAssetSpec.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/serialize/TomlCodec.h>

using namespace hgl;
using namespace hgl::asset;
using namespace hgl::graph;
namespace codec = hgl::asset::generated;

namespace
{
    int g_failed = 0;

    void Check(const bool condition, const char *what)
    {
        if (condition)
            return;

        GLogError(u8"TestEnvironmentCodec 失败: %s", what);
        ++g_failed;
    }

    bool Nearly(const float a, const float b)
    {
        return fabsf(a - b) < 1.0e-5f;
    }

    /// 文本 → 规约。走与运行时资产加载完全相同的路径（值树 + Archive）。
    bool ReadText(const char *text, EnvironmentSpec &out)
    {
        return hgl::serialize::LoadFromTomlText<EnvironmentSpec>(text, out);
    }

    // ── 1. 五组字段完整往返 ─────────────────────────────────────────────────
    constexpr char kFullEnvironment[] =
        "schema = 1\n"
        "name = \"Sky34.CubeIBL\"\n"
        "description = \"sky_34 立方体贴图 + 同一批 IBL 天光\"\n"
        "\n"
        "[visual]\n"
        "recipe = \"IBLEnvironment.Sky\"\n"
        "geometry = \"geo:builtin:hex_sphere_sky\"\n"
        "scale = 1.5\n"
        "track_camera = true\n"
        "\n"
        "[ambient]\n"
        "mode = \"cubemap_ibl\"\n"
        "provider = \"ambient_light_ibl\"\n"
        "\n"
        "[ibl]\n"
        "irradiance = \"res/image/Environments/sky_34_2k/sky_34_cubemap_2k.TexCube\"\n"
        "prefilter = \"res/image/Environments/sky_34_2k/sky_34_prefilter_2k.TexCube\"\n"
        "brdf_lut = \"res/image/brdflut.Tex2D\"\n"
        "\n"
        "[ldr]\n"
        "mode = \"on\"\n"
        "exposure_base = 3.0\n"
        "sun_scale = 0.75\n"
        "min_ambient = 0.1\n"
        "\n"
        "[sh9]\n"
        "auto_generate = false\n"
        "track_sun = false\n"
        "sun_irradiance_scale = 0.35\n"
        "exposure = 1.5\n";

    bool TestFullTextRoundTrip()
    {
        EnvironmentSpec spec;
        if (!ReadText(kFullEnvironment, spec))
            return false;

        if (spec.schema != 1)
            return false;
        if (spec.name != "Sky34.CubeIBL")
            return false;
        if (spec.description.Length() == 0)
            return false;

        if (!spec.visual.recipe.present || spec.visual.recipe.value != "IBLEnvironment.Sky")
            return false;
        if (!spec.visual.geometry.present
            || spec.visual.geometry.value != "geo:builtin:hex_sphere_sky")
            return false;
        if (!spec.visual.scale.present || !Nearly(spec.visual.scale.value, 1.5f))
            return false;
        if (!spec.visual.track_camera.present || !spec.visual.track_camera.value)
            return false;

        if (!spec.ambient.mode.present || spec.ambient.mode.value != SkyMode::CubeMapIBL)
            return false;
        if (!spec.ambient.provider.present || spec.ambient.provider.value != "ambient_light_ibl")
            return false;

        if (!spec.ibl.irradiance.present)
            return false;
        if (spec.ibl.irradiance.value != OS_TEXT(
                "res/image/Environments/sky_34_2k/sky_34_cubemap_2k.TexCube"))
            return false;
        if (!spec.ibl.brdf_lut.present)
            return false;

        if (!spec.ldr.mode.present || spec.ldr.mode.value != SkyAmbientLDR::On)
            return false;
        if (!spec.ldr.exposure_base.present || !Nearly(spec.ldr.exposure_base.value, 3.0f))
            return false;
        if (!spec.ldr.sun_scale.present || !Nearly(spec.ldr.sun_scale.value, 0.75f))
            return false;
        if (!spec.ldr.min_ambient.present || !Nearly(spec.ldr.min_ambient.value, 0.1f))
            return false;

        if (!spec.sh9.auto_generate.present || spec.sh9.auto_generate.value)
            return false;
        if (!spec.sh9.track_sun.present || spec.sh9.track_sun.value)
            return false;
        if (!spec.sh9.sun_irradiance_scale.present
            || !Nearly(spec.sh9.sun_irradiance_scale.value, 0.35f))
            return false;
        if (!spec.sh9.exposure.present || !Nearly(spec.sh9.exposure.value, 1.5f))
            return false;
        // 没写 `[[sh9.bands]]` ⇒ 保留类内缺省（空数组）且 present 为假。
        if (spec.sh9.bands.present || spec.sh9.bands.value.GetCount() != 0)
            return false;

        EnvironmentSpec back;
        if (!hgl::serialize::LoadFromTomlText<EnvironmentSpec>(kFullEnvironment, back))
            return false;

        const AnsiString once = hgl::serialize::SaveToTomlText<EnvironmentSpec>(spec);
        const AnsiString twice = hgl::serialize::SaveToTomlText<EnvironmentSpec>(back);
        if (once != twice)
            return false;

        // 写出的文本必须能被重新读回同样的值（往返闭环，而不只是两次排版相同）。
        EnvironmentSpec third;
        return ReadText(once.c_str(), third)
            && third.ambient.mode.value == SkyMode::CubeMapIBL
            && Nearly(third.ldr.exposure_base.value, 3.0f);
    }

    // ── 2. Presence 三态 ────────────────────────────────────────────────────
    bool TestPresenceTriState()
    {
        // (a) 只写顶层字段：所有组内字段都必须「没出现」。
        constexpr char kMinimal[] =
            "schema = 1\n"
            "name = \"Env.Absent\"\n";

        EnvironmentSpec absent;
        if (!ReadText(kMinimal, absent))
            return false;
        if (absent.ambient.mode.present
            || absent.ambient.provider.present
            || absent.visual.recipe.present
            || absent.ibl.irradiance.present
            || absent.ldr.mode.present
            || absent.sh9.auto_generate.present
            || absent.sh9.bands.present)
            return false;

        // (b) 显式写成与类内缺省相同的值 ⇒ present 为真、值仍是缺省值。
        //     这正是「资产想强调：这里就是要 Sh9Only」与「根本没这回事」的区别。
        constexpr char kExplicitDefault[] =
            "schema = 1\n"
            "name = \"Env.ExplicitDefault\"\n"
            "[ambient]\n"
            "mode = \"sh9_only\"\n"
            "[visual]\n"
            "scale = 0.0\n";

        EnvironmentSpec explicit_default;
        if (!ReadText(kExplicitDefault, explicit_default))
            return false;
        if (!explicit_default.ambient.mode.present
            || explicit_default.ambient.mode.value != SkyMode::SHLightOnly)
            return false;
        // `0` 对 `scale` 是「用模式内置默认」，同时也是 `Presence<float>` 的合法取值。
        if (!explicit_default.visual.scale.present || !Nearly(explicit_default.visual.scale.value, 0.0f))
            return false;

        // (c) 显式写非缺省值 ⇒ present 为真、值被覆盖。
        constexpr char kOverridden[] =
            "schema = 1\n"
            "name = \"Env.Overridden\"\n"
            "[ambient]\n"
            "mode = \"octahedral_ibl\"\n"
            "[ldr]\n"
            "mode = \"off\"\n";

        EnvironmentSpec overridden;
        if (!ReadText(kOverridden, overridden))
            return false;

        return overridden.ambient.mode.present
            && overridden.ambient.mode.value == SkyMode::OctahedralIBL
            && overridden.ldr.mode.present
            && overridden.ldr.mode.value == SkyAmbientLDR::Off;
    }

    // ── 3. 枚举拼写 ────────────────────────────────────────────────────────
    bool TestEnumSpelling()
    {
        // 别名与 C++ 名都接受（`stricmp`），写出统一用别名 —— 资产文件里不出现
        // `CubeMapIBL` 这种内部拼写。
        constexpr char kInternalName[] =
            "schema = 1\n"
            "name = \"Env.InternalName\"\n"
            "[ambient]\n"
            "mode = \"CubeMapIBL\"\n"
            "[ldr]\n"
            "mode = \"AUTO\"\n";

        EnvironmentSpec from_internal;
        if (!ReadText(kInternalName, from_internal))
            return false;
        if (from_internal.ambient.mode.value != SkyMode::CubeMapIBL)
            return false;
        if (from_internal.ldr.mode.value != SkyAmbientLDR::Auto)
            return false;

        const AnsiString written = hgl::serialize::SaveToTomlText<EnvironmentSpec>(from_internal);
        if (!strstr(written.c_str(), "\"cubemap_ibl\""))
            return false;
        if (!strstr(written.c_str(), "\"auto\""))
            return false;
        if (strstr(written.c_str(), "CubeMapIBL"))
            return false;

        // 拼写表里没有的名字必须整份拒绝，不能静默退化成 0 号枚举。
        constexpr char kUnknown[] =
            "schema = 1\n"
            "name = \"Env.Unknown\"\n"
            "[ambient]\n"
            "mode = \"octahedral\"\n";

        EnvironmentSpec rejected;
        return !ReadText(kUnknown, rejected);
    }

    // ── 4. `[[sh9.bands]]` 数组 ────────────────────────────────────────────
    bool TestBandsArray()
    {
        constexpr char kBands[] =
            "schema = 1\n"
            "name = \"Env.Bands\"\n"
            "[sh9]\n"
            "auto_generate = false\n"
            "[[sh9.bands]]\n"
            "r = 0.1\n"
            "g = 0.2\n"
            "b = 0.3\n"
            "[[sh9.bands]]\n"
            "r = 0.4\n"
            "g = 0.5\n"
            "b = 0.6\n"
            "[[sh9.bands]]\n"
            "r = 0.7\n"
            "g = 0.8\n"
            "b = 0.9\n";

        EnvironmentSpec spec;
        if (!ReadText(kBands, spec))
            return false;

        // 顺序即基函数顺序（`[0] Y₀₀` … `[8] Y₂₂`），不允许重排。
        if (!spec.sh9.bands.present || spec.sh9.bands.value.GetCount() != 3)
            return false;
        const EnvironmentSH9BandSpec *bands = spec.sh9.bands.value.GetData();
        if (!bands)
            return false;
        if (!Nearly(bands[0].r, 0.1f) || !Nearly(bands[1].g, 0.5f) || !Nearly(bands[2].b, 0.9f))
            return false;

        // 空数组：出现过、内容为空 ⇒ 与「不写这一行」语义不同。
        constexpr char kEmptyBands[] =
            "schema = 1\n"
            "name = \"Env.EmptyBands\"\n"
            "[sh9]\n"
            "bands = []\n";

        EnvironmentSpec empty;
        if (!ReadText(kEmptyBands, empty))
            return false;
        if (!empty.sh9.bands.present || empty.sh9.bands.value.GetCount() != 0)
            return false;

        // 往返后条数与数值不变。
        EnvironmentSpec back;
        return hgl::serialize::LoadFromTomlText<EnvironmentSpec>(kBands, back)
            && back.sh9.bands.present
            && back.sh9.bands.value.GetCount() == 3
            && Nearly(back.sh9.bands.value.GetData()[1].r, 0.4f);
    }

    // ── 5. 类型不符必须失败 ────────────────────────────────────────────────
    bool TestTypeMismatchRejected()
    {
        constexpr char kBadScale[] =
            "schema = 1\n"
            "name = \"Env.BadScale\"\n"
            "[visual]\n"
            "scale = \"big\"\n";

        constexpr char kBadBool[] =
            "schema = 1\n"
            "name = \"Env.BadBool\"\n"
            "[sh9]\n"
            "auto_generate = 3\n";

        EnvironmentSpec a;
        EnvironmentSpec b;
        return !ReadText(kBadScale, a) && !ReadText(kBadBool, b);
    }

    // ── 6. 元数据接线 ──────────────────────────────────────────────────────
    const PropertyMetaInfo *FindProperty(const ClassMetaInfo &meta, const char *name)
    {
        for (uint32_t i = 0; i < meta.property_count; ++i)
        {
            if (meta.properties[i].name && AnsiString(meta.properties[i].name) == name)
                return &meta.properties[i];
        }
        return nullptr;
    }

    bool TestMetaStructLinks()
    {
#define REQUIRE(cond) do { if (!(cond)) { GLogError(u8"[meta] 断言失败: %s", #cond); return false; } } while (0)

        const ClassMetaInfo &env = codec::Class_EnvironmentSpec;
        REQUIRE(env.property_count == 8);
        REQUIRE(env.struct_size == sizeof(EnvironmentSpec));
        REQUIRE(env.struct_align == alignof(EnvironmentSpec));

        const PropertyMetaInfo *schema = FindProperty(env, "schema");
        const PropertyMetaInfo *visual = FindProperty(env, "visual");
        const PropertyMetaInfo *ambient = FindProperty(env, "ambient");
        const PropertyMetaInfo *ibl = FindProperty(env, "ibl");
        const PropertyMetaInfo *ldr = FindProperty(env, "ldr");
        const PropertyMetaInfo *sh9 = FindProperty(env, "sh9");
        REQUIRE(schema && visual && ambient && ibl && ldr && sh9);

        // 顶层标量：没有子结构，不是 presence（版本号必须始终写出来）。
        REQUIRE(schema->kind == PropertyKind::UInt32);
        REQUIRE(schema->struct_meta == nullptr);
        REQUIRE(!IsPresenceProperty(*schema));

        REQUIRE(visual->struct_meta == &codec::Class_EnvironmentVisualSpec);
        REQUIRE(ambient->struct_meta == &codec::Class_EnvironmentAmbientSpec);
        REQUIRE(ibl->struct_meta == &codec::Class_EnvironmentIBLSpec);
        REQUIRE(ldr->struct_meta == &codec::Class_EnvironmentLDRSpec);
        REQUIRE(sh9->struct_meta == &codec::Class_EnvironmentSH9Spec);

        // 组内枚举字段：必须带名字表（TOML 里出现的是字符串而非数字）且是 presence。
        const PropertyMetaInfo *mode = FindProperty(*ambient->struct_meta, "mode");
        REQUIRE(mode != nullptr);
        REQUIRE(mode->kind == PropertyKind::Enum);
        REQUIRE(mode->enum_name != nullptr);
        REQUIRE(AnsiString(mode->enum_name) == "hgl::graph::SkyMode");
        REQUIRE(mode->enum_parse != nullptr);
        REQUIRE(mode->enum_to_name != nullptr);
        REQUIRE(IsPresenceProperty(*mode));

        // `bands`：值语义动态数组，元素是子结构，且整体被 `Presence` 包着。
        const PropertyMetaInfo *bands = FindProperty(*sh9->struct_meta, "bands");
        REQUIRE(bands != nullptr);
        REQUIRE(bands->kind == PropertyKind::ValueArray);
        REQUIRE(bands->elem_kind == PropertyKind::Struct);
        REQUIRE(bands->struct_meta == &codec::Class_EnvironmentSH9BandSpec);
        REQUIRE(bands->fixed_length == 0);
        REQUIRE(IsPresenceProperty(*bands));

        // 元素结构自己的元数据也要接上（检查器靠它展开 `[[sh9.bands]]` 的每一行）。
        const ClassMetaInfo &band = codec::Class_EnvironmentSH9BandSpec;
        REQUIRE(band.property_count == 3);
        REQUIRE(band.struct_size == sizeof(EnvironmentSH9BandSpec));
        REQUIRE(FindProperty(band, "r") != nullptr);

        // 偏移必须是当前编译器算出来的真值：用真实对象的地址反推，跨编译配置都要成立。
        EnvironmentSpec probe;
        const char *base = reinterpret_cast<const char *>(&probe);
        REQUIRE(reinterpret_cast<const char *>(&probe.schema) - base == (ptrdiff_t)schema->offset);
        REQUIRE(reinterpret_cast<const char *>(&probe.visual) - base == (ptrdiff_t)visual->offset);
        REQUIRE(reinterpret_cast<const char *>(&probe.ambient) - base == (ptrdiff_t)ambient->offset);
        REQUIRE(reinterpret_cast<const char *>(&probe.ibl) - base == (ptrdiff_t)ibl->offset);
        REQUIRE(reinterpret_cast<const char *>(&probe.ldr) - base == (ptrdiff_t)ldr->offset);
        REQUIRE(reinterpret_cast<const char *>(&probe.sh9) - base == (ptrdiff_t)sh9->offset);

#undef REQUIRE
        return true;
    }

    // ── 7. FlatBuffers 往返 ────────────────────────────────────────────────
    bool TestFlatBuffersRoundTrip()
    {
        EnvironmentSpec spec;
        if (!ReadText(kFullEnvironment, spec))
            return false;

        // 加 3 条 band 验证结构数组
        spec.sh9.bands.present = true;
        EnvironmentSH9BandSpec b0{0.1f, 0.2f, 0.3f};
        EnvironmentSH9BandSpec b1{0.4f, 0.5f, 0.6f};
        EnvironmentSH9BandSpec b2{0.7f, 0.8f, 0.9f};
        spec.sh9.bands.value.Add(b0);
        spec.sh9.bands.value.Add(b1);
        spec.sh9.bands.value.Add(b2);

        ValueArray<uint8> buffer;
        AnsiString error;

        if (!EncodeEnvironmentFlat(spec, buffer, &error))
        {
            GLogError(u8"EncodeEnvironmentFlat 失败: %s", error.c_str());
            return false;
        }

        if (buffer.IsEmpty())
            return false;

        EnvironmentSpec back;
        if (!DecodeEnvironmentFlat(buffer.GetData(), buffer.GetCount(), back, &error))
        {
            GLogError(u8"DecodeEnvironmentFlat 失败: %s", error.c_str());
            return false;
        }

        if (back.schema != spec.schema)
            return false;
        if (back.name != spec.name)
            return false;
        if (back.description != spec.description)
            return false;
        if (back.visual.recipe.value != spec.visual.recipe.value)
            return false;
        if (back.visual.geometry.value != spec.visual.geometry.value)
            return false;
        if (!Nearly(back.visual.scale.value, spec.visual.scale.value))
            return false;
        if (back.visual.track_camera.value != spec.visual.track_camera.value)
            return false;
        if (back.ambient.mode.value != spec.ambient.mode.value)
            return false;
        if (back.ambient.provider.value != spec.ambient.provider.value)
            return false;
        if (back.ibl.irradiance.value != spec.ibl.irradiance.value)
            return false;
        if (back.ibl.prefilter.value != spec.ibl.prefilter.value)
            return false;
        if (back.ibl.brdf_lut.value != spec.ibl.brdf_lut.value)
            return false;
        if (back.ldr.mode.value != spec.ldr.mode.value)
            return false;
        if (!Nearly(back.ldr.exposure_base.value, spec.ldr.exposure_base.value))
            return false;
        if (!Nearly(back.ldr.sun_scale.value, spec.ldr.sun_scale.value))
            return false;
        if (!Nearly(back.ldr.min_ambient.value, spec.ldr.min_ambient.value))
            return false;
        if (back.sh9.auto_generate.value != spec.sh9.auto_generate.value)
            return false;
        if (back.sh9.track_sun.value != spec.sh9.track_sun.value)
            return false;
        if (!Nearly(back.sh9.sun_irradiance_scale.value, spec.sh9.sun_irradiance_scale.value))
            return false;
        if (!Nearly(back.sh9.exposure.value, spec.sh9.exposure.value))
            return false;
        if (!back.sh9.bands.present || back.sh9.bands.value.GetCount() != 3)
            return false;
        if (!Nearly(back.sh9.bands.value[1].r, 0.4f))
            return false;

        // 文件读写往返
        const OSString tmp_path = OS_TEXT("build/test_env_roundtrip.env");
        if (!SaveEnvironmentSpecToFlatBuffers(spec, tmp_path))
        {
            GLogError(u8"SaveEnvironmentSpecToFlatBuffers 失败");
            return false;
        }

        EnvironmentSpec file_back;
        if (!LoadEnvironmentSpecFromFlatBuffers(tmp_path, file_back))
        {
            GLogError(u8"LoadEnvironmentSpecFromFlatBuffers 失败");
            return false;
        }

        const bool ok = file_back.name == spec.name
                     && file_back.ambient.mode.value == spec.ambient.mode.value
                     && file_back.sh9.bands.value.GetCount() == 3;

        hgl::filesystem::FileDelete(tmp_path);
        return ok;
    }

    // ── 8. AssetObject 多态序列化虚接口验证 ────────────────────────────────
    bool TestAssetObjectPolymorphicIO()
    {
        EnvironmentAsset env_asset;
        AssetObject *obj = &env_asset;

        if (!obj->ImportToml(kFullEnvironment, sizeof(kFullEnvironment) - 1))
        {
            GLogError(u8"AssetObject::ImportToml 失败");
            return false;
        }

        if (obj->GetAssetName() != "Sky34.CubeIBL")
        {
            GLogError(u8"AssetObject::GetAssetName 不符: %s", obj->GetAssetName().c_str());
            return false;
        }

        AnsiString toml_text;
        if (!obj->ExportToml(toml_text) || toml_text.IsEmpty())
        {
            GLogError(u8"AssetObject::ExportToml 失败");
            return false;
        }

        ValueArray<uint8> flat_bytes;
        if (!obj->ExportFlat(flat_bytes) || flat_bytes.IsEmpty())
        {
            GLogError(u8"AssetObject::ExportFlat 失败");
            return false;
        }

        EnvironmentAsset back_asset;
        AssetObject *back_obj = &back_asset;
        if (!back_obj->ImportFlat(flat_bytes.GetData(), flat_bytes.GetCount()))
        {
            GLogError(u8"AssetObject::ImportFlat 失败");
            return false;
        }

        if (back_asset.GetSpec().name != "Sky34.CubeIBL"
         || back_asset.GetSpec().ambient.mode.value != SkyMode::CubeMapIBL)
        {
            GLogError(u8"AssetObject 反序列化数据不匹配");
            return false;
        }

        const OSString tmp_toml = OS_TEXT("build/test_poly.env.toml");
        const OSString tmp_flat = OS_TEXT("build/test_poly.env");

        if (!obj->SaveToFile(tmp_toml))
        {
            GLogError(u8"AssetObject::SaveToFile(.env.toml) 失败");
            return false;
        }

        EnvironmentAsset from_toml_asset;
        if (!from_toml_asset.LoadFromFile(tmp_toml) || from_toml_asset.GetSpec().name != "Sky34.CubeIBL")
        {
            GLogError(u8"AssetObject::LoadFromFile(.env.toml) 失败");
            return false;
        }

        if (!obj->SaveToFile(tmp_flat))
        {
            GLogError(u8"AssetObject::SaveToFile(.env) 失败");
            return false;
        }

        EnvironmentAsset from_flat_asset;
        if (!from_flat_asset.LoadFromFile(tmp_flat) || from_flat_asset.GetSpec().name != "Sky34.CubeIBL")
        {
            GLogError(u8"AssetObject::LoadFromFile(.env) 失败");
            return false;
        }

        hgl::filesystem::FileDelete(tmp_toml);
        hgl::filesystem::FileDelete(tmp_flat);
        return true;
    }

    // ── 9. 通过 AssetObject 导出与全量双格式校验默认预设 ─────────────────────
    bool TestExportAndVerifyDefaultPresets()
    {
        const OSString preset_dir = OS_TEXT("ShaderLibrary/asset/environment");
        const int exported_count = ExportDefaultEnvironmentPresets(preset_dir, true, true);
        if (exported_count != 5)
        {
            GLogError(u8"ExportDefaultEnvironmentPresets 导出数量不符: 实际 %d, 预期 5", exported_count);
            return false;
        }

        const os_char *preset_names[] = {
            OS_TEXT("sky_atmosphere"),
            OS_TEXT("sky34_cube_ibl"),
            OS_TEXT("sky34_oct_ibl"),
            OS_TEXT("sky34_lambert_dome"),
            OS_TEXT("sky34_sh9"),
        };

        for (const os_char *base_name : preset_names)
        {
            const OSString toml_path = preset_dir + OS_TEXT("/") + OSString(base_name) + OS_TEXT(".env.toml");
            const OSString flat_path = preset_dir + OS_TEXT("/") + OSString(base_name) + OS_TEXT(".env");

            EnvironmentAsset toml_asset;
            if (!toml_asset.LoadFromFile(toml_path))
            {
                GLogError(u8"加载预设 .env.toml 失败: %ls", toml_path.c_str());
                return false;
            }

            EnvironmentAsset flat_asset;
            if (!flat_asset.LoadFromFile(flat_path))
            {
                GLogError(u8"加载预设 .env 失败: %ls", flat_path.c_str());
                return false;
            }

            if (toml_asset.GetSpec().schema != 1 || flat_asset.GetSpec().schema != 1)
            {
                GLogError(u8"预设 schema 校验失败: %ls", base_name);
                return false;
            }

            if (toml_asset.GetSpec().name != flat_asset.GetSpec().name)
            {
                GLogError(u8"预设 TOML 与 FlatBuffers 名字不一致: toml=%s flat=%s",
                          toml_asset.GetSpec().name.c_str(), flat_asset.GetSpec().name.c_str());
                return false;
            }

            if (toml_asset.GetSpec().ambient.mode.value != flat_asset.GetSpec().ambient.mode.value)
            {
                GLogError(u8"预设 TOML 与 FlatBuffers ambient.mode 不一致: %ls", base_name);
                return false;
            }

            GLogInfo(u8"预设双格式校验通过: %ls (name=%s)",
                     base_name, toml_asset.GetSpec().name.c_str());
        }

        return true;
    }
}//namespace

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestEnvironmentCodec"));

    Check(TestFullTextRoundTrip(), "五组字段完整往返");
    Check(TestPresenceTriState(), "Presence 三态");
    Check(TestEnumSpelling(), "枚举拼写");
    Check(TestBandsArray(), "SH9 bands 数组");
    Check(TestTypeMismatchRejected(), "类型不符必须失败");
    Check(TestMetaStructLinks(), "元数据接线");
    Check(TestFlatBuffersRoundTrip(), "FlatBuffers 往返");
    Check(TestAssetObjectPolymorphicIO(), "AssetObject 多态序列化虚接口");
    Check(TestExportAndVerifyDefaultPresets(), "预设资产通过 AssetObject 导出与双格式校验");

    if (g_failed > 0)
    {
        GLogError(u8"=== TestEnvironmentCodec: %d 项失败 ===", g_failed);
        return 1;
    }

    GLogInfo(u8"=== TestEnvironmentCodec: 全部测试通过 ===");
    return 0;
}
