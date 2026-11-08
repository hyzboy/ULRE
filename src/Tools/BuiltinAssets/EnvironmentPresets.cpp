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

        /// sky_34 那批烘焙图的公共路径主干（后缀决定投影：Cube 六面 / Oct 八面体）。
        /// 按新 URI 约定**不写扩展名**，装载时由前缀 + 磁盘探测补全。
        constexpr const os_char *kSky34CubeDir =
            OS_TEXT("texcube:pack(res):image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k");
        constexpr const os_char *kSky34OctDir =
            OS_TEXT("texoct:pack(res):image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k");

        /// `Presence<T>` 只认「整段赋值」才算显式给出（对外层写 `p.value.x = ...`
        /// 不会置外层 `present`，整段就不会序列化），所以技术段先在普通结构体上
        /// 逐字段赋值，最后一次性提交给 `spec.<段>`。
    }//namespace

    int ExportEnvironmentPresets(PresetWriter &writer)
    {
        int count = 0;

        // 1. Sky34 —— 统一天空资产（schema 2）：一份数据，五档技术。
        //
        //    取代旧的 `sky_atmosphere / sky34_cube_ibl / sky34_oct_ibl /
        //    sky34_lambert_dome / sky34_sh9` 五份分散预设：画质档**不在资产里**，
        //    由调用方在 `SetupSkyEnvironment(uri, SkyTechnique)` 外部选型；
        //    资产只把每种技术的配方 / 烘焙图 / LDR 曝光并列写全。
        //
        //    `[sun]` 刻意留空：sky_34 的烘焙图太阳是**固定**的（图上太阳 = 光影
        //    太阳，动了就不一致，那是作者的人为问题），而本预设同时要喂会随太阳
        //    走的 `atmosphere` / `sh9` 两档 —— 写死一个基准反而两头不讨好。需要
        //    资产托管太阳的作者场景请自己补 `[sun]` 段（装载链路会自动同步主光源
        //    行 0，见 `EnvironmentSystem::SyncSunToMainLight`）。
        {
            EnvironmentSpec spec;
            spec.schema = 2;
            spec.name = "Sky34";
            spec.description = "统一天空资产 sky_34：大气/立方体/八面体/Lambert 穹顶/SH9 五档技术并列，画质档在资产外选型";

            /* ── [ambient]：`SkyTechnique::Default` 时的档位（高配基准 = Cube IBL）── */
            spec.ambient.mode = SkyMode::CubeMapIBL;

            /* ── [base]：各技术共用的视觉身份 ──────────────────────────────── */
            {
                EnvironmentBaseSpec b;
                b.scale = 1.0f;
                b.track_camera = true;
                spec.base = b;
            }

            /* ── [cubemap]：高配立方体贴图 IBL（HDR/LDR 由 ldr_mode=auto 判定）── */
            {
                EnvironmentTechniqueSpec t;
                t.recipe = "IBLEnvironment.Sky";
                t.irradiance = OSString(kSky34CubeDir) + OS_TEXT("_irradiance");
                t.prefilter = OSString(kSky34CubeDir) + OS_TEXT("_prefilter");
                t.ldr_mode = SkyAmbientLDR::On;
                t.ldr_exposure_base = 2.5f;
                t.ldr_sun_scale = 1.0f;
                t.ldr_min_ambient = 0.05f;
                spec.cubemap = t;
            }

            /* ── [octahedral]：同一批数据换八面体打包（省六面接缝）─────────── */
            {
                EnvironmentTechniqueSpec t;
                t.recipe = "IBLEnvironmentSkyOct.Sky";
                t.irradiance = OSString(kSky34OctDir) + OS_TEXT("_irradiance");
                t.prefilter = OSString(kSky34OctDir) + OS_TEXT("_prefilter");
                t.ldr_mode = SkyAmbientLDR::On;
                t.ldr_exposure_base = 2.5f;
                t.ldr_sun_scale = 1.0f;
                t.ldr_min_ambient = 0.05f;
                spec.octahedral = t;
            }

            /* ── [lambert_dome]：穹顶视觉 + 同批 Cube IBL 天光 ─────────────── */
            {
                EnvironmentTechniqueSpec t;
                t.recipe = "SkyLambertDome.Unlit";
                t.scale = 100.0f;   // 穹顶半径（覆盖 [base] 的 1.0）
                t.irradiance = OSString(kSky34CubeDir) + OS_TEXT("_irradiance");
                t.prefilter = OSString(kSky34CubeDir) + OS_TEXT("_prefilter");
                t.ldr_mode = SkyAmbientLDR::On;
                t.ldr_exposure_base = 2.5f;
                t.ldr_sun_scale = 1.0f;
                t.ldr_min_ambient = 0.05f;
                spec.lambert_dome = t;
            }

            /* ── [atmosphere]：解析大气（天空 + 天光同源，无烘焙图）────────── */
            {
                EnvironmentTechniqueSpec t;
                t.recipe = "AtmosphereSkyAmbient.Sky";
                spec.atmosphere = t;
            }

            /* ── [sh9]：顶层共享（SH9 档没有视觉段，纯球谐天光）────────────── */
            spec.sh9.auto_generate = true;
            spec.sh9.track_sun = true;

            if (!SaveEnvironment(writer, spec, OS_TEXT("sky_34")))
                return count;
            ++count;
        }

        return count;
    }
}//namespace hgl::graph::builtin
