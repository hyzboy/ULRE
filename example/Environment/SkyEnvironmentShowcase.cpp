// SkyEnvironmentShowcase — 统一天空 + 天光托管 API 的**多模式综合演示**
//
// 引擎里能画"天"的办法有好几种（解析大气 / cubemap / 八面体展开 / Lambert 穹顶 /
// 纯 SH9 球谐），它们各自拆成两件**正交**的事：**视觉天空**（画什么几何体 + 用哪份
// 配方）与**天光**（`ambient_light_provider` 用哪个算法 + 喂什么数据）。
//
// 本示例一次演示这件事：数字键 `1`~`5` 在运行时热切五种组合，鼠标拖 Gizmo 旋转太阳，
// 观察每种模式如何自己跟着太阳走（大气的高光与地平线过渡、LDR IBL 的曝光调制、
// SH9 系数的重算都是引擎侧自动的，示例一行都不写）。
//
// 为什么切换是**一次 API 调用**：`SetupSkyEnvironment()` 会先拆掉上一套（天空实体、
// 句柄、provider 槽）再按新配置重建 ⇒ 换模式不会留下过期实体或悬空句柄。示例需要
// 描述的只是"要哪一份配方 + 要哪种天光"，几何体、顶点流、实体、Mobility、
// HDR/LDR 判定、SH9 系数合成、provider 名全在托管层里。
//
// 自检（`SKYSHOW_CYCLE=1`）：无窗口交互下跑一遍完整热切链 —— 每模式 4 帧，第 4 帧
// 取一次帧摘要；五份摘要必须**两两不同**（抓"切换悄悄失败、画面还停在上一个模式"）
// 且各自 `distinct >= 256`（抓"整帧只剩清屏色"）。任一不满足退出 1。
// `SKYSHOW_MODE=<0..4>` 可指定起始模式（交互用），与自检互斥。
#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/asset/GeometryPreset.h>
#include<hgl/graph/asset/GeometryAssetRegistry.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/ssbo/GlobalSSBOTypes.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/graph/gizmo/SunDirectionControlSystem.h>
#include<hgl/mtl/MaterialRecipe.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/systems/render/LightSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<glm/gtx/quaternion.hpp>

#include<hgl/vk/VKRenderTargetSwapchain.h>

#include "../Shared/FrameDigest.h"   // 帧摘要（自检判据共用件）

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    // 物体材质：材质数据行的唯一来源（一个 URI + 一个行号）
    constexpr char   MATERIAL_DATA_URI[] =
        "mtldata:file:ShaderLibrary/asset/material_data/atmosphere_sky_ambient.material_data.toml";
    constexpr uint32 MATERIAL_DATA_ROW = 0;
}

namespace
{
    /// 天光数据的来源。与视觉天空**互相独立**：同一个穹顶背景可以配 cube 天光，
    /// 同一个 cubemap 背景也可以配 SH9 天光。
    enum class AmbientSource : unsigned char
    {
        Auto = 0,   ///< 交给托管层按模式推断（`Atmosphere` → `ambient_light_sky`）
        CubeIBL,    ///< `.TexCube` 辐照度 + 预滤波 ⇒ `ambient_light_ibl`
        OctIBL,     ///< `.TexOct` 同一批数据 ⇒ `ambient_light_ibl_oct`
        SH,         ///< 零贴图 SH9 ⇒ `ambient_light_sh`（系数由解析合成器现算）
    };

    /// 一套可热切的"视觉天空 + 天光"组合。
    struct SkyModePreset
    {
        SkyMode       mode;
        const char  * label;         ///< 菜单名（仅诊断日志）
        const char  * recipe;        ///< 视觉天空配方（空 = 不建天空实体）
        const char  * geometry_uri;  ///< 几何体覆盖；nullptr = 按 `mode` 取默认
        float         scale;         ///< 0 = 托管层默认（全天球 1.0 / 穹顶 4.0）
        AmbientSource ambient;
    };

    constexpr int kModeCount = 5;

    /// `LambertDome` 的几何体是半球穹顶（基准半径 1），要当背景用必须放大到相机之外；
    /// 其余模式是半径 256 的全天球，scale 1.0 就够包住整个场景。
    constexpr float kDomeBackgroundScale = 100.0f;

    const SkyModePreset kPresets[kModeCount] =
    {
        { SkyMode::Atmosphere,    "大气（解析）",   "AtmosphereSkyAmbient.Sky",   nullptr, 1.0f,                  AmbientSource::Auto    },
        { SkyMode::CubeMapIBL,    "Cubemap IBL",    "IBLEnvironment.Sky",         nullptr, 1.0f,                  AmbientSource::CubeIBL },
        { SkyMode::OctahedralIBL, "八面体 IBL",     "IBLEnvironmentSkyOct.Sky",   nullptr, 1.0f,                  AmbientSource::OctIBL  },
        { SkyMode::LambertDome,   "Lambert 穹顶",   "SkyLambertDome.Unlit",       nullptr, kDomeBackgroundScale,    AmbientSource::CubeIBL },
        { SkyMode::SHLightOnly,   "SH9 球谐天光",   "IBLEnvironment.Sky",         "geo:builtin:hex_sphere_sky", 1.0f, AmbientSource::SH      },
    };

    /// 同一批烘焙数据的两种投影展开（只差目录后缀名）。
    constexpr os_char IBL_TEX_IRR_CUBE[] = OS_TEXT("res/image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_irradiance.TexCube");
    constexpr os_char IBL_TEX_IRR_OCT[]  = OS_TEXT("res/image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_irradiance.TexOct");
    constexpr os_char IBL_TEX_PRE_CUBE[] = OS_TEXT("res/image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_prefilter.TexCube");
    constexpr os_char IBL_TEX_PRE_OCT[]  = OS_TEXT("res/image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_prefilter.TexOct");

    /// 自检：每模式跑几帧才取摘要（第 1 帧在建实体 + 新配方首帧编译，画面还不稳）。
    constexpr uint32 kCycleFramesPerMode = 10;

    /// 自检采集到的各模式摘要（跨 Tick 保存，判据在最后一帧的帧内钩子里收尾）。
    ulre_example::FrameDigest g_cycle_digest[kModeCount];

    /**
     * `SKYSHOW_CYCLE=1`：无交互地跑完整热切链并自检。
     *
     * 为什么不能只靠交互手测：热切路径的真事故是"拆了旧的没建起新的"（天空实体
     * 消失 / provider 槽没换 / 句柄悬空），这些在日志里只差一行，眼睛很难盯住五连切。
     * 摘要两两不同即可证明"每一档都真的换了画面"，比逐个模式钉死像素更抗材质微调。
     */
    bool CycleSelfTest()
    {
        static const bool s_cycle = (getenv("SKYSHOW_CYCLE") != nullptr);
        return s_cycle;
    }

    int StartMode()
    {
        const char * v = getenv("SKYSHOW_MODE");
        if (!v)
            return 0;

        const int index = atoi(v);
        return (index >= 0 && index < kModeCount) ? index : 0;
    }

    /// 物体顶点流：Position V3F + UV（V2HF）+ 压缩法线（V2UN8，无切线）。
    GeometryVertexFormat CreateMeshGeometryVertexFormat()
    {
        return GeometryVertexFormat{
            { VertexSemantic::Position, VF_V3F   },
            { VertexSemantic::TexCoord, VF_V2HF  },
            { VertexSemantic::Normal,   VF_V2UN8 },
        };
    }
}

class SkyEnvironmentShowcaseApp : public WorkObject
{
private:

    ECSContext * ecs_context = nullptr;
    Entity     * camera_entity = nullptr;

    /// 天空实体 + 天光数据 + provider 全由它托管（含 IBL 句柄的 bindless 注册）
    std::shared_ptr<EnvironmentSystem> environment_system;
    /// 太阳方向住在**世界私有光源行 0**：天空与天光都从这一行取光
    std::shared_ptr<LightSystem> light_system;
    /// 鼠标拖动的太阳方向手柄（自己写光源行 0，示例无需干预）
    std::shared_ptr<SunDirectionControlSystem> sun_gizmo_system;

    struct MeshEntry
    {
        Geometry     * geometry = nullptr;
        PrimitiveAsset asset{};
    };

    /// 固定长度数组而不是动态容器：`PrimitiveAsset` 存的是自地址，扩容会打断引用。
    static constexpr int kMeshCount = 5;
    MeshEntry meshes[kMeshCount];

    graph::mtl::MaterialRecipe mesh_recipe{};
    graph::GlobalSSBOBinding   material_data_ssbo_accessor{};

    int  current_mode = -1;
    bool key_prev[kModeCount] = {};

    uint32 cycle_frame = 0;

private:

    bool InitMaterialDataSSBO()
    {
        return GetMaterialDataBinding(MATERIAL_DATA_URI, MATERIAL_DATA_ROW, material_data_ssbo_accessor);
    }

    bool InitMaterial()
    {
        if (!material_data_ssbo_accessor.IsValid())
            return false;

        const mtl::MaterialRecipe * loaded_recipe = GetRecipe("AtmosphereSkyAmbient.Lit");
        if (!loaded_recipe)
            return false;

        mesh_recipe = *loaded_recipe;

        return (mesh_recipe.material_ssbo_binding = material_data_ssbo_accessor).IsValid();
    }

    bool CreateRenderObjects()
    {
        // 天空几何体不在这里：它随模式由 `SetupSkyEnvironment()` 选（全天球 / 半球穹顶）。

        static const GeometryVertexFormat mesh_format = CreateMeshGeometryVertexFormat();

        static const char * mesh_uri[kMeshCount] =
        {
            "geo:builtin:sphere_48",
            "geo:builtin:cube",
            "geo:builtin:cone_low",
            "geo:builtin:torus_low",
            "geo:builtin:capsule",
        };

        for (int i = 0; i < kMeshCount; ++i)
        {
            Geometry * geom = GetGeometry(mesh_uri[i], &mesh_format);
            if (!geom)
                return false;

            meshes[i].geometry = geom;
            meshes[i].asset    = PrimitiveAsset(geom, &mesh_recipe, PrimitiveType::Triangles);
        }

        return true;
    }

    bool InitSceneEntities()
    {
        // 物体排一圈：法线朝向各不相同 ⇒ 天光的**方向性**（上亮下暗、有无太阳项）
        // 一眼可见。半径压在 Lambert 穹顶之内，切换模式时不被背景挡住。
        constexpr float ring_radius = 2.2f;

        for (int i = 0; i < kMeshCount; ++i)
        {
            char entity_name[32];
            snprintf(entity_name, sizeof(entity_name), "Mesh_%d", i);

            auto * entity = ecs_context->CreateEntity<Entity>(entity_name);
            if (!entity)
                return false;

            auto transform = ecs_context->GetTransform(
                ecs_context->CreateTransform(entity->GetEntityID(), Mobility::Static));

            const float angle = glm::radians(360.0f * float(i) / float(kMeshCount));
            const glm::vec3 pos = glm::vec3(ring_radius * glm::cos(angle),
                                            0.8f,
                                            ring_radius * glm::sin(angle));

            transform.SetLocalPosition(pos);
            transform.SetLocalRotation(glm::angleAxis(-angle, glm::vec3(0.0f, 1.0f, 0.0f)));
            transform.SetLocalScale(glm::vec3(1.0f));
            transform.SetMobility(Mobility::Static);

            auto * primitive_comp = ecs_context->GetOrCreateGeometryData(entity);
            auto * material_comp  = ecs_context->GetOrCreateMaterialData(entity);
            if (!primitive_comp || !material_comp)
                return false;

            primitive_comp->SetPrimitiveAsset(&meshes[i].asset);

            hgl::ecs::MaterialData::MaterialDataAuthoringResource mesh_struct{};
            mesh_struct = material_data_ssbo_accessor;
            material_comp->SetDataResource(mesh_struct);
        }

        return true;
    }

    /**
     * 把一档预设翻译成配置并交给托管层。
     *
     * 示例侧的唯一职责是"选配方 + 选天光来源"：投影形态（`.TexCube` / `.TexOct` /
     * `.Tex2D`）由配方里的输入文件决定，provider 名由托管层按 prefilter 后缀推断，
     * HDR/LDR 由装载出来的格式判定 —— 三者都不在这里写死。
     */
    bool ApplySkyMode(int index)
    {
        if (!environment_system || index < 0 || index >= kModeCount)
            return false;

        const SkyModePreset & preset = kPresets[index];

        SkyEnvironmentConfig config;

        config.mode         = preset.mode;
        config.sky_recipe   = preset.recipe;
        config.sky_scale    = preset.scale;
        config.track_camera = true;     // 无视差背景：天空实体逐帧跟随主相机

        if (preset.geometry_uri)
            config.sky_geometry_uri = preset.geometry_uri;

        // SH9 始终现算：这样在 IBL 档里临时把 provider 换成 `ambient_light_sh`
        // 立刻就有与太阳一致的低频天光，不会切到一组陈旧（甚至全 0）系数。
        config.sh_auto      = true;
        config.sh_track_sun = true;

        switch (preset.ambient)
        {
            case AmbientSource::CubeIBL:
                config.irradiance_texture = IBL_TEX_IRR_CUBE;
                config.prefilter_texture  = IBL_TEX_PRE_CUBE;
                break;

            case AmbientSource::OctIBL:
                config.irradiance_texture = IBL_TEX_IRR_OCT;
                config.prefilter_texture  = IBL_TEX_PRE_OCT;
                break;

            case AmbientSource::SH:
                config.ambient_provider = "ambient_light_sh";
                break;

            case AmbientSource::Auto:
            default:
                break;
        }

        if (preset.ambient == AmbientSource::CubeIBL || preset.ambient == AmbientSource::OctIBL)
        {
            // 这批烘焙图是 unorm 量化版：显式开启 LDR 曝光调制（与 `Auto` 的判定同值，
            // 写出来是为了让三个调制系数的口径可见）。HDR 烘焙图留 `Auto` 即可。
            config.ldr               = SkyAmbientLDR::On;
            config.ldr_exposure_base = 2.5f;
            config.ldr_sun_scale     = 1.0f;
            config.ldr_min_ambient   = 0.05f;
        }

        if (!environment_system->SetupSkyEnvironment(config))
        {
            GLogError(u8"[SkyShowcase] 模式 %d（%s）装配失败", index + 1, preset.label);
            return false;
        }

        current_mode = index;

        GLogInfo(u8"[SkyShowcase] 模式 %d/%d → %s（数字键 1~%d 切换，鼠标拖 Gizmo 旋转太阳）",
                 index + 1, kModeCount, preset.label, kModeCount);

        return true;
    }

    bool InitECS()
    {
        ecs_context = GetECSContext();
        if (!ecs_context)
            return false;

        environment_system = ecs_context->GetSystem<EnvironmentSystem>();
        if (!environment_system)
            environment_system = ecs_context->RegisterSystem<EnvironmentSystem>();
        if (!environment_system)
            return false;

        light_system = ecs_context->GetSystem<LightSystem>();
        if (!light_system)
            light_system = ecs_context->RegisterSystem<LightSystem>();
        if (!light_system)
            return false;

        // 太阳方向必须在**天空托管之前**写进光源行 0：SH9 系数是 CPU 侧按当前太阳
        // 方向现算的，装晚了会先用"正顶太阳"那一版烘一次天光。
        {
            const glm::vec3 sun_dir = glm::normalize(glm::vec3(0.35f, 0.55f, 0.76f));
            light_system->SetMainLightDirection(math::Vector3f(sun_dir.x, sun_dir.y, sun_dir.z));
        }

        sun_gizmo_system = ecs_context->GetSystem<SunDirectionControlSystem>();
        if (!sun_gizmo_system)
            sun_gizmo_system = ecs_context->RegisterSystem<SunDirectionControlSystem>();
        if (!sun_gizmo_system)
            return false;

        sun_gizmo_system->SetLightSystem(light_system.get());
        sun_gizmo_system->SetGizmoPosition(math::Vector3f(0.0f, 0.0f, 0.0f));

        if (!ApplySkyMode(StartMode()))
            return false;

        return InitSceneEntities();
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        auto * camera = ecs_context->GetOrCreateCamera(camera_entity);
        if (!camera)
            return false;

        camera->control_mode    = CameraControlMode::ViewModel;
        camera->target          = math::Vector3f(0.0f);
        camera->distance        = 8.0f;      // 在天空球（半径 256）/ 放大穹顶（半径 100）之内
        camera->yaw             = 30.0f;
        camera->pitch           = -18.0f;
        camera->is_main_camera  = true;
        camera->matrix_dirty    = true;

        camera->viewport_info = GetViewportInfo();

        return true;
    }

    /// 数字键 `1`~`5` 热切（边沿触发：按住不会反复重建天空实体）。
    void PollModeSwitch()
    {
        auto input_system = ecs_context->GetSystem<InputSystem>();
        if (!input_system)
            return;

        static const io::KeyboardButton keys[kModeCount] =
        {
            io::KeyboardButton::_1,
            io::KeyboardButton::_2,
            io::KeyboardButton::_3,
            io::KeyboardButton::_4,
            io::KeyboardButton::_5,
        };

        for (int i = 0; i < kModeCount; ++i)
        {
            const bool down = input_system->IsKeyDown(keys[i]);

            if (down && !key_prev[i] && i != current_mode)
                ApplySkyMode(i);

            key_prev[i] = down;
        }
    }

    /// 自检调度：第 1 帧建当前档，第 `kCycleFramesPerMode` 帧取帧摘要，跑完五档收尾。
    void RunCycleSelfTest()
    {
        const uint32 slot  = cycle_frame / kCycleFramesPerMode;
        const uint32 phase = cycle_frame % kCycleFramesPerMode;

        ++cycle_frame;

        if (slot >= kModeCount)
            return;

        if (phase == 0)
        {
            if (slot != current_mode)
                ApplySkyMode(int(slot));

            return;
        }

        if (phase != kCycleFramesPerMode - 1)
            return;

        auto * rt = dynamic_cast<SwapchainRenderTarget *>(ecs_context->GetRenderTarget());
        if (!rt)
        {
            GLogError(u8"[SKYSHOW] 没有 swapchain 渲染靶");
            std::exit(2);
        }

        // 钩子是**帧内**回读（渲染完、present 前）⇒ 每次都要重装，且一次性。
        rt->SetInFrameReadbackHook([rt, slot]()
        {
            ulre_example::FrameDigest digest;

            if (!ulre_example::ComputeFrameDigest(rt, digest))
            {
                GLogError(u8"[SKYSHOW] %s 帧摘要读回失败", kPresets[slot].label);
                std::exit(2);
            }

            g_cycle_digest[slot] = digest;

            GLogInfo(u8"[SKYSHOW] %d %s distinct=%u mean_luma=%.4f fnv1a64=0x%016llX",
                     slot + 1, kPresets[slot].label,
                     digest.distinct_byte_values, digest.mean_luma,
                     (unsigned long long)digest.fnv1a64);

            if (slot + 1 < uint32(kModeCount))
                return;

            // ── 收尾判据 ──
            bool ok = true;

            for (uint32 i = 0; i < uint32(kModeCount); ++i)
            {
                if (g_cycle_digest[i].distinct_byte_values < 256)
                {
                    GLogError(u8"[SKYSHOW] %s 画面过于单一（distinct=%u）",
                              kPresets[i].label, g_cycle_digest[i].distinct_byte_values);
                    ok = false;
                }

                for (uint32 j = i + 1; j < uint32(kModeCount); ++j)
                {
                    if (g_cycle_digest[i].fnv1a64 == g_cycle_digest[j].fnv1a64)
                    {
                        GLogError(u8"[SKYSHOW] %s 与 %s 画面完全相同 —— 热切无效",
                                  kPresets[i].label, kPresets[j].label);
                        ok = false;
                    }
                }
            }

            GLogInfo(u8"[SKYSHOW] CYCLE %s：%d 档全链热切完成",
                     ok ? "PASS" : "FAIL", kModeCount);

            std::exit(ok ? 0 : 1);
        });
    }

public:

    ~SkyEnvironmentShowcaseApp() override
    {
        // 几何由 GeometryAssetRegistry 持有
        if (auto * registry = GetGeometryAssetRegistry())
            registry->Clear();
    }

    bool Init() override
    {
        if (!InitMaterialDataSSBO())
            return false;

        if (!InitMaterial())
            return false;

        if (!CreateRenderObjects())
            return false;

        if (!InitECS())
            return false;

        if (!InitCamera())
            return false;

        return true;
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);

        if (!ecs_context)
            return;

        if (CycleSelfTest())
            RunCycleSelfTest();
        else
            PollModeSwitch();
    }
};

int os_main(int argc, os_char ** argv)
{
    return RunFramework<SkyEnvironmentShowcaseApp>(OS_TEXT("SkyEnvironmentShowcase"), argc, argv, 1280, 720);
}
