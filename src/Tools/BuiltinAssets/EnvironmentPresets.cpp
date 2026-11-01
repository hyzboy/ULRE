#include "EnvironmentPresets.h"

#include <hgl/graph/asset/EnvironmentAsset.h>

namespace hgl::graph::builtin
{
    namespace
    {
        /// 全部环境预设共用的落盘路径：`<out>/environment/<stem>.env[.toml]`
        constexpr const os_char *kEnvironmentSubDir = OS_TEXT("environment");
        constexpr const os_char *kEnvironmentExt = OS_TEXT(".env");

        bool SaveEnvironment(PresetWriter &writer, const EnvironmentSpec &spec, const os_char *stem)
        {
            EnvironmentAsset asset(spec);
            return writer.Save(asset, kEnvironmentSubDir, stem, kEnvironmentExt);
        }
    }//namespace

    int ExportEnvironmentPresets(PresetWriter &writer)
    {
        int count = 0;

        // 1. Sky.Atmosphere —— 廉价解析大气（天空 + 天光同源，无烘焙图）
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

            if (!SaveEnvironment(writer, spec, OS_TEXT("sky_atmosphere")))
                return count;
            ++count;
        }

        // 2. Sky34.CubeMapIBL —— sky_34 立方体贴图天空 + 同批 IBL 天光（Cube 投影）
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

            if (!SaveEnvironment(writer, spec, OS_TEXT("sky34_cube_ibl")))
                return count;
            ++count;
        }

        // 3. Sky34.OctahedralIBL —— sky_34 八面体贴图天空 + 同批 IBL 天光（Octahedral 投影）
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

            if (!SaveEnvironment(writer, spec, OS_TEXT("sky34_oct_ibl")))
                return count;
            ++count;
        }

        // 4. Sky34.LambertDome —— Lambert 方位等积穹顶天空 + Cube 投影 IBL 天光
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

            if (!SaveEnvironment(writer, spec, OS_TEXT("sky34_lambert_dome")))
                return count;
            ++count;
        }

        // 5. Sky34.SH9 —— sky_34 立方体贴图天空 + SH9 球谐天光（无烘焙图依赖）
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

            if (!SaveEnvironment(writer, spec, OS_TEXT("sky34_sh9")))
                return count;
            ++count;
        }

        return count;
    }
}//namespace hgl::graph::builtin
