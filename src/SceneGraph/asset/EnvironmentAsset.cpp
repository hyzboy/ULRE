#include <hgl/graph/asset/EnvironmentAsset.h>
#include <hgl/asset/generated/EnvironmentMeta.gen.h>
#include <hgl/asset/generated/EnvironmentSerialize.gen.h>
#include <hgl/serialize/ArchiveFile.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/filesystem/Path.h>
#include <hgl/log/Log.h>
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
    }//namespace

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

    int ExportDefaultEnvironmentPresets(const OSString &output_dir,
                                        bool export_toml,
                                        bool export_flat)
    {
        if (output_dir.IsEmpty())
            return 0;

        hgl::filesystem::Path dir_path(output_dir);
        if (!dir_path.IsDirectory())
            hgl::filesystem::MakePath(output_dir);

        auto MakeFilePath = [&](const os_char *stem, const os_char *ext) -> OSString {
            OSString p = output_dir;
            if (!p.EndsWith(OS_TEXT("/")) && !p.EndsWith(OS_TEXT("\\")))
                p += OS_TEXT("/");
            p += stem;
            p += ext;
            return p;
        };

        int count = 0;

        // 保存一律走统一序列化门面（`ArchiveFile.h`）：格式由文件名后缀决定，
        // 这里既不认识 TOML 也不认识 FlatBuffers。加新后端时本函数不用改。
        auto SavePreset = [&](EnvironmentAsset &asset, const os_char *stem) -> bool
        {
            AnsiString error;

            if (export_toml
             && !hgl::serialize::SaveAssetObjectToFile(
                    asset, MakeFilePath(stem, OS_TEXT(".env.toml")),
                    hgl::serialize::ArchiveFormat::Auto, &error))
            {
                GLogError(u8"[EnvironmentPreset] 导出 TOML 失败: %s", error.c_str());
                return false;
            }

            if (export_flat
             && !hgl::serialize::SaveAssetObjectToFile(
                    asset, MakeFilePath(stem, OS_TEXT(".env")),
                    hgl::serialize::ArchiveFormat::Auto, &error))
            {
                GLogError(u8"[EnvironmentPreset] 导出 FlatBuffers 失败: %s", error.c_str());
                return false;
            }

            return true;
        };

        // 1. Sky.Atmosphere
        {
            EnvironmentSpec spec;
            spec.schema = 1;
            spec.name = "Sky.Atmosphere";
            spec.description = "廉价解析大气（天空 + 天光同源，无烘焙图）";
            spec.visual.recipe = "AtmosphereSkyAmbient.Sky";
            spec.visual.scale = 1.0f;
            spec.visual.track_camera = true;
            spec.ambient.mode = SkyMode::Atmosphere;
            spec.sh9.auto_generate = true;
            spec.sh9.track_sun = true;

            EnvironmentAsset asset(std::move(spec));
            if (SavePreset(asset, OS_TEXT("sky_atmosphere"))) ++count;
        }

        // 2. Sky34.CubeMapIBL
        {
            EnvironmentSpec spec;
            spec.schema = 1;
            spec.name = "Sky34.CubeMapIBL";
            spec.description = "sky_34 立方体贴图天空 + 同批 IBL 天光（Cube 投影）";
            spec.visual.recipe = "IBLEnvironment.Sky";
            spec.visual.scale = 1.0f;
            spec.visual.track_camera = true;
            spec.ambient.mode = SkyMode::CubeMapIBL;
            spec.ibl.irradiance = OS_TEXT("res/image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_irradiance.TexCube");
            spec.ibl.prefilter = OS_TEXT("res/image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_prefilter.TexCube");
            spec.ldr.mode = SkyAmbientLDR::On;
            spec.ldr.exposure_base = 2.5f;
            spec.ldr.sun_scale = 1.0f;
            spec.ldr.min_ambient = 0.05f;
            spec.sh9.auto_generate = true;
            spec.sh9.track_sun = true;

            EnvironmentAsset asset(std::move(spec));
            if (SavePreset(asset, OS_TEXT("sky34_cube_ibl"))) ++count;
        }

        // 3. Sky34.OctahedralIBL
        {
            EnvironmentSpec spec;
            spec.schema = 1;
            spec.name = "Sky34.OctahedralIBL";
            spec.description = "sky_34 八面体贴图天空 + 同批 IBL 天光（Octahedral 投影）";
            spec.visual.recipe = "IBLEnvironmentSkyOct.Sky";
            spec.visual.scale = 1.0f;
            spec.visual.track_camera = true;
            spec.ambient.mode = SkyMode::OctahedralIBL;
            spec.ibl.irradiance = OS_TEXT("res/image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_irradiance.TexOct");
            spec.ibl.prefilter = OS_TEXT("res/image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_prefilter.TexOct");
            spec.ldr.mode = SkyAmbientLDR::On;
            spec.ldr.exposure_base = 2.5f;
            spec.ldr.sun_scale = 1.0f;
            spec.ldr.min_ambient = 0.05f;
            spec.sh9.auto_generate = true;
            spec.sh9.track_sun = true;

            EnvironmentAsset asset(std::move(spec));
            if (SavePreset(asset, OS_TEXT("sky34_oct_ibl"))) ++count;
        }

        // 4. Sky34.LambertDome
        {
            EnvironmentSpec spec;
            spec.schema = 1;
            spec.name = "Sky34.LambertDome";
            spec.description = "sky_34 Lambert 方位等积穹顶天空 + Cube 投影 IBL 天光";
            spec.visual.recipe = "SkyLambertDome.Unlit";
            spec.visual.scale = 100.0f;
            spec.visual.track_camera = true;
            spec.ambient.mode = SkyMode::LambertDome;
            spec.ibl.irradiance = OS_TEXT("res/image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_irradiance.TexCube");
            spec.ibl.prefilter = OS_TEXT("res/image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_prefilter.TexCube");
            spec.ldr.mode = SkyAmbientLDR::On;
            spec.ldr.exposure_base = 2.5f;
            spec.ldr.sun_scale = 1.0f;
            spec.ldr.min_ambient = 0.05f;
            spec.sh9.auto_generate = true;
            spec.sh9.track_sun = true;

            EnvironmentAsset asset(std::move(spec));
            if (SavePreset(asset, OS_TEXT("sky34_lambert_dome"))) ++count;
        }

        // 5. Sky34.SH9
        {
            EnvironmentSpec spec;
            spec.schema = 1;
            spec.name = "Sky34.SH9";
            spec.description = "sky_34 立方体贴图天空 + SH9 球谐天光（无烘焙图依赖）";
            spec.visual.recipe = "IBLEnvironment.Sky";
            spec.visual.geometry = "geo:builtin:hex_sphere_sky";
            spec.visual.scale = 1.0f;
            spec.visual.track_camera = true;
            spec.ambient.mode = SkyMode::SHLightOnly;
            spec.ambient.provider = "ambient_light_sh";
            spec.sh9.auto_generate = true;
            spec.sh9.track_sun = true;

            EnvironmentAsset asset(std::move(spec));
            if (SavePreset(asset, OS_TEXT("sky34_sh9"))) ++count;
        }

        return count;
    }
}//namespace hgl::graph
