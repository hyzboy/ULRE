#include <hgl/graph/asset/EnvironmentAsset.h>
#include <hgl/graph/asset/EnvironmentAssetFlat.h>
#include <hgl/asset/generated/EnvironmentMeta.gen.h>
#include <hgl/asset/generated/EnvironmentSerialize.gen.h>
#include <hgl/serialize/TomlCodec.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/io/FileOutputStream.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/filesystem/Path.h>
#include <hgl/type/Smart.h>
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

    bool EnvironmentAsset::ExportToml(AnsiString &out_text) const
    {
        hgl::serialize::Archive archive;
        out_text = hgl::serialize::SaveToTomlText(spec, &archive);
        return !out_text.IsEmpty();
    }

    bool EnvironmentAsset::ExportFlat(ValueArray<uint8> &out_bytes) const
    {
        AnsiString error;
        return EncodeEnvironmentFlat(spec, out_bytes, &error);
    }

    bool EnvironmentAsset::ImportToml(const char *content, size_t size)
    {
        if (!content || size == 0)
            return false;

        EnvironmentSpec new_spec;
        hgl::serialize::Archive archive;
        hgl::serialize::TomlParseError toml_error;
        if (!hgl::serialize::LoadFromTomlText<EnvironmentSpec>(
                content, size, new_spec, &archive, &toml_error))
            return false;

        spec = std::move(new_spec);
        asset_name = spec.name;
        return true;
    }

    bool EnvironmentAsset::ImportFlat(const uint8 *data, size_t size)
    {
        if (!data || size < FLATBUFFERS_MIN_BUFFER_SIZE)
            return false;

        EnvironmentSpec new_spec;
        AnsiString error;
        if (!DecodeEnvironmentFlat(data, size, new_spec, &error))
            return false;

        spec = std::move(new_spec);
        asset_name = spec.name;
        return true;
    }

    bool EnvironmentAsset::SaveToToml(const OSString &filename) const
    {
        AnsiString text;
        if (!ExportToml(text))
            return false;

        hgl::io::FileOutputStream *fos = hgl::io::CreateFileOutputStream(filename);
        if (!fos)
            return false;

        const int64 written = fos->Write(text.c_str(), text.Length());
        delete fos;
        return written == text.Length();
    }

    bool EnvironmentAsset::SaveToFlat(const OSString &filename) const
    {
        return SaveEnvironmentSpecToFlatBuffers(spec, filename);
    }

    bool EnvironmentAsset::LoadFromToml(const OSString &filename)
    {
        hgl::io::OpenFileInputStream opener(filename);
        if (!opener)
            return false;

        const int64 size = opener->GetSize();
        if (size <= 0 || size > 16 * 1024 * 1024)
            return false;

        AutoDeleteArray<char> buffer(static_cast<size_t>(size) + 1);
        if (opener->Read(buffer.data(), size) != size)
            return false;
        buffer[static_cast<size_t>(size)] = '\0';

        if (!ImportToml(buffer.data(), static_cast<size_t>(size)))
            return false;

        SetSource(filename.c_str());
        return true;
    }

    bool EnvironmentAsset::LoadFromFlat(const OSString &filename)
    {
        hgl::io::OpenFileInputStream opener(filename);
        if (!opener)
            return false;

        const int64 size = opener->GetSize();
        if (size < FLATBUFFERS_MIN_BUFFER_SIZE || size > 64 * 1024 * 1024)
            return false;

        AutoDeleteArray<uint8> buffer(static_cast<size_t>(size));
        if (opener->Read(buffer.data(), size) != size)
            return false;

        if (!ImportFlat(buffer.data(), static_cast<size_t>(size)))
            return false;

        SetSource(filename.c_str());
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
            bool ok = true;
            if (export_toml)
                ok = ok && asset.SaveToToml(MakeFilePath(OS_TEXT("sky_atmosphere"), OS_TEXT(".env.toml")));
            if (export_flat)
                ok = ok && asset.SaveToFlat(MakeFilePath(OS_TEXT("sky_atmosphere"), OS_TEXT(".env")));
            if (ok) ++count;
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
            bool ok = true;
            if (export_toml)
                ok = ok && asset.SaveToToml(MakeFilePath(OS_TEXT("sky34_cube_ibl"), OS_TEXT(".env.toml")));
            if (export_flat)
                ok = ok && asset.SaveToFlat(MakeFilePath(OS_TEXT("sky34_cube_ibl"), OS_TEXT(".env")));
            if (ok) ++count;
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
            bool ok = true;
            if (export_toml)
                ok = ok && asset.SaveToToml(MakeFilePath(OS_TEXT("sky34_oct_ibl"), OS_TEXT(".env.toml")));
            if (export_flat)
                ok = ok && asset.SaveToFlat(MakeFilePath(OS_TEXT("sky34_oct_ibl"), OS_TEXT(".env")));
            if (ok) ++count;
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
            bool ok = true;
            if (export_toml)
                ok = ok && asset.SaveToToml(MakeFilePath(OS_TEXT("sky34_lambert_dome"), OS_TEXT(".env.toml")));
            if (export_flat)
                ok = ok && asset.SaveToFlat(MakeFilePath(OS_TEXT("sky34_lambert_dome"), OS_TEXT(".env")));
            if (ok) ++count;
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
            bool ok = true;
            if (export_toml)
                ok = ok && asset.SaveToToml(MakeFilePath(OS_TEXT("sky34_sh9"), OS_TEXT(".env.toml")));
            if (export_flat)
                ok = ok && asset.SaveToFlat(MakeFilePath(OS_TEXT("sky34_sh9"), OS_TEXT(".env")));
            if (ok) ++count;
        }

        return count;
    }
}//namespace hgl::graph
