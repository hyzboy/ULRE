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
// 五种组合**不在代码里**：它们是 `ShaderLibrary/asset/environment/` 下的五份
// `*.env.toml`（`name` 分别是 `Sky.Atmosphere` / `Sky34.CubeMapIBL` /
// `Sky34.OctahedralIBL` / `Sky34.LambertDome` / `Sky34.SH9`）。示例开机扫一次目录，
// 切换时只传一个 `env:@<name>` —— 这就是"环境配置资产化"：加一套新天空不用改代码。
//
// 为什么切换是**一次 API 调用**：`SetupSkyEnvironment()` 会先拆掉上一套（天空实体、
// 句柄、provider 槽）再按新配置重建 ⇒ 换模式不会留下过期实体或悬空句柄。示例需要
// 描述的只是一个 URI，几何体、顶点流、实体、Mobility、HDR/LDR 判定、SH9 系数合成、
// provider 名全在托管层与资产文件里。
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
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/support/transform/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/ecs/systems/render/light_shadow/EnvironmentSystem.h>
#include<hgl/ecs/systems/render/light_shadow/LightSystem.h>

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
    /// 一套可热切的环境资产。示例只认识 **URI**：配方 / 几何体 / 投影形态（cube 还是
    /// 八面体）/ LDR 曝光策略 / provider 全在 `*.env.toml` 里，示例不再逐个字段赋值。
    ///
    /// 这就是「环境配置资产化」的意义：换一套天空 + 天光 = 换一个字符串，而不是在
    /// 代码里重新拼 `SkyEnvironmentConfig`。整套预设放在
    /// `ShaderLibrary/asset/environment/`（见 `kEnvAssetDirectory`）。
    struct SkyEnvPreset
    {
        const char * label;     ///< 菜单名（仅诊断日志）
        const os_char * env_uri;///< `env:@<name>`：按资产文件里的 `name` 引用
    };

    constexpr int kModeCount = 5;

    /// 预设资产目录（开机扫一次，之后 `env:@Name` 才能命中）。
    constexpr os_char kEnvAssetDirectory[] = OS_TEXT("ShaderLibrary/asset/environment");

    const SkyEnvPreset kPresets[kModeCount] =
    {
        { "大气（解析）",   OS_TEXT("env:@Sky.Atmosphere")      },
        { "Cubemap IBL",    OS_TEXT("env:@Sky34.CubeMapIBL")    },
        { "八面体 IBL",     OS_TEXT("env:@Sky34.OctahedralIBL") },
        { "Lambert 穹顶",   OS_TEXT("env:@Sky34.LambertDome")   },
        { "SH9 球谐天光",   OS_TEXT("env:@Sky34.SH9")           },
    };

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
     * 把一档预设交给托管层。
     *
     * 示例侧现在只剩一行：`SetupSkyEnvironment(uri)`。投影形态（`.TexCube` / `.TexOct`）、
     * provider 名、HDR/LDR 策略、几何体与 scale 全在资产文件里 —— 这正是"环境配置
     * 资产化"要达到的效果：加一套新的天空组合不需要改示例。
     */
    bool ApplySkyMode(int index)
    {
        if (!environment_system || index < 0 || index >= kModeCount)
            return false;

        const SkyEnvPreset & preset = kPresets[index];

        if (!environment_system->SetupSkyEnvironment(OSString(preset.env_uri)))
        {
            GLogError(u8"[SkyShowcase] 模式 %d（%s）装配失败：%ls",
                      index + 1, preset.label, preset.env_uri);
            return false;
        }

        current_mode = index;

        GLogInfo(u8"[SkyShowcase] 模式 %d/%d → %s（%ls；数字键 1~%d 切换，鼠标拖 Gizmo 旋转太阳）",
                 index + 1, kModeCount, preset.label, preset.env_uri, kModeCount);

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

        // `env:@Name` 只能命中已注册条目，所以先把预设目录扫一次（每进程一次即可）。
        {
            int loaded = 0;
            int errors = 0;

            if (!environment_system->LoadEnvironmentDirectory(kEnvAssetDirectory, &loaded, &errors))
            {
                GLogError(u8"[SkyShowcase] 环境预设目录加载失败：%ls", kEnvAssetDirectory);
                return false;
            }

            if (loaded < kModeCount || errors > 0)
                GLogWarning(u8"[SkyShowcase] 环境预设数量异常：loaded=%d errors=%d（期望 %d 个全成功）",
                            loaded, errors, kModeCount);
        }

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
