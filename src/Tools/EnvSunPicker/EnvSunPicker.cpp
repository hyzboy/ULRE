/**
 * EnvSunPicker —— 环境（.env）资产的太阳方向拾取工具
 *
 * 打开一个统一天空资产（`.env` 二进制或 `.env.toml` 文本），以第一人称视角
 * 站在天空穹顶**内部**渲染它；从世界原点画一根指向当前太阳方向的线段；
 * 鼠标左键**点击**球面（非拖拽）即把命中射线方向写回 `[sun].direction`；
 * Ctrl+S 保存（`.env` 与 `.env.toml` 双格式同写），Esc 退出。
 * 数字键 1..5 在资产已配置的技术档位间热切换预览。
 *
 * 语义约定（与统一天空资产的设计一致）：
 *   · 点击永远只写 `[sun].direction`，**不改任何烘焙贴图**。CubeMap /
 *     Octahedral / LambertDome 这类烘焙档位的贴图里太阳是固定烤死的 ——
 *     选点只影响光源方向（主光/阴影）；若开发者希望烘焙档位下太阳不动，
 *     那是该档位自己的资产配置问题，工具如实处理并在此说明。
 *   · 画质档位的选择在资产外部；本工具按资产 `[ambient].mode` 起步，
 *     数字键仅用于预览对比。
 *
 * 交互：
 *   · 左键拖拽 = 环视（FirstPerson 相机，位置钉在世界原点）；
 *   · 左键点击（按下→抬起位移 < 6px）= 选太阳；
 *   · 1..5 切换资产内存在的档位；Ctrl+S 保存；Esc 退出（脏时二次确认）。
 *
 * 自检：`SUNPICKER_SELFCHECK=1 EnvSunPicker <资产路径>` —— 加载 → 写方向 →
 * 双格式落盘（临时目录）→ 重载比对，GLog 打 PASS/FAIL，退出码 0/1。
 */

#include <hgl/framework/WorkManager.h>

#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/graph/asset/GeometryAssetRegistry.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <hgl/graph/geo/InlineGeometry.h>
#include <hgl/graph/module/GeometryManager.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/camera/Camera.h>
#include <hgl/graph/ssbo/GlobalSSBOTypes.h>
#include <hgl/graph/ssbo/MaterialDataRows.h>
#include <hgl/graph/asset/EnvironmentAsset.h>
#include <hgl/graph/asset/EnvironmentAssetRegistry.h>
#include <hgl/math/geometry/Ray.h>
#include <hgl/vk/buffer/VertexAttribBuffer.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/MaterialResolver.h>

#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/entity/Entity.h>
#include <hgl/ecs/components/GeometryData.h>
#include <hgl/ecs/components/MaterialData.h>
#include <hgl/ecs/components/CameraControlMode.h>
#include <hgl/ecs/support/transform/TransformAccessor.h>
#include <hgl/ecs/systems/tick/CameraSystem.h>
#include <hgl/ecs/systems/tick/InputSystem.h>
#include <hgl/ecs/systems/render/light_shadow/EnvironmentSystem.h>
#include <hgl/ecs/systems/render/light_shadow/LightSystem.h>
#include <hgl/ecs/systems/render/light_shadow/SkyEnvironmentConfig.h>

#include <hgl/serialize/ArchiveFile.h>
#include <hgl/log/Log.h>
#include <hgl/type/String.h>

#include <cmath>
#include <cstdlib>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#endif

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    // 线段材质沿用 RayPicking 示例的现成配方与材质数据行（行 1 = 线）
    constexpr char   MATERIAL_DATA_URI[] =
        "mtldata:pack(ShaderLibrary):asset/material_data/ray_picking";
    constexpr uint32 MATERIAL_DATA_ROW_LINE = 1;

    constexpr os_char kWindowTitle[] = OS_TEXT("EnvSunPicker");

    // 线的长度与端点十字臂长（世界单位）；相机钉在原点，穹顶半径远大于此
    constexpr float kSunLineLength = 60.0f;
    constexpr float kSunCrossArm   = 1.5f;

    // 点击判定：按下→抬起窗口位移小于该像素数视作“点击”而非“环视拖拽”
    constexpr int kClickMoveTolerancePx = 6;

    // os_main 捕获的原始命令行（argv[1] = 资产路径）
    int         g_argc = 0;
    os_char   **g_argv = nullptr;

    GeometryVertexFormat CreateVertexLuminance3DGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position,  VF_V3F},
            {VertexSemantic::Luminance, VF_V1UN8},
        };
        return gvf;
    }

    /// 线段顶点：6 个 = 主线段(0-1) + 端点十字两段(2-3、4-5)，无索引、按邻接成线
    static float g_line_positions[6][3] =
    {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, kSunLineLength},
        {-kSunCrossArm, 0.0f, kSunLineLength},
        { kSunCrossArm, 0.0f, kSunLineLength},
        {0.0f, -kSunCrossArm, kSunLineLength},
        {0.0f,  kSunCrossArm, kSunLineLength},
    };
    static uint8 g_line_luminance[6] = {255, 255, 200, 200, 200, 200};

    inline math::Vector3f NormalizeOr(const math::Vector3f &v, const math::Vector3f &fallback)
    {
        const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (len < 1e-6f)
            return fallback;
        return math::Vector3f(v.x / len, v.y / len, v.z / len);
    }

    /// SkyMode（资产 `[ambient].mode` 的视觉模式）→ 预览档位
    inline bool SkyModeToTechnique(SkyMode mode, SkyTechnique &out)
    {
        switch (mode)
        {
        case SkyMode::Atmosphere:    out = SkyTechnique::Atmosphere;   return true;
        case SkyMode::CubeMapIBL:    out = SkyTechnique::CubeMap;      return true;
        case SkyMode::OctahedralIBL: out = SkyTechnique::Octahedral;   return true;
        case SkyMode::LambertDome:   out = SkyTechnique::LambertDome;  return true;
        case SkyMode::SHLightOnly:   out = SkyTechnique::SH9;          return true;
        default:                     return false;   // None / 未写 ⇒ 由调用方兜底
        }
    }
}//namespace

class EnvSunPickerApp : public WorkObject
{
private:

    ECSContext *ecs_context   = nullptr;
    Entity *    camera_entity = nullptr;
    CameraComponent *camera   = nullptr;
    std::shared_ptr<EnvironmentSystem> environment_system;
    std::shared_ptr<LightSystem>       light_system;

    // ── 资产与档位 ──
    OSString           asset_path;              ///< 打开的文件路径（保存回同路径的双格式）
    EnvironmentAsset * asset    = nullptr;      ///< 注册表持有，这里只观察
    OSString           env_uri;                 ///< "env:file:<path>"
    SkyTechnique       techniques[5]{};         ///< 资产里实际存在的技术档位（数字键顺序）
    int                technique_count = 0;
    int                technique_index = 0;

    // ── 太阳状态（内存真值 = 资产 [sun].direction）──
    math::Vector3f sun_direction{0.2f, 0.7f, 0.68f};
    bool           sun_written_in_asset = false;   ///< 资产里原本有没有 [sun].direction
    bool           dirty = false;

    // ── 线段渲染资源（RayPicking 同款通路）──
    Geometry *          geom_line = nullptr;
    VAB *               line_vab  = nullptr;
    PrimitiveAsset      line_asset{};
    mtl::MaterialRecipe line_recipe{};
    GlobalSSBOBinding   line_material_data_accessor{};

    // ── 输入边沿检测 ──
    // 键盘槽位：0=Esc 1=S 2..6=_1.._5 7=Ctrl（左/右任一）
    static constexpr int kKeySlotCount = 8;
    bool            prev_keys[kKeySlotCount] = {};
    bool            prev_lmb_down = false;
    math::Vector2i  lmb_press_pos{0, 0};
    bool            lmb_pressed = false;
    bool            esc_confirm = false;        ///< 脏状态下第一次 Esc 只提示

    bool selfcheck_active = false;

private:

    // ════════════════════════════════════════════════════════════
    //  资产打开 / 档位收集
    // ════════════════════════════════════════════════════════════

    static OSString ShowOpenFileDialog();

    const OSString &ResolveAssetPath()
    {
        // argv[1] 优先（`EnvSunPicker path\to\x.env.toml`），否则弹系统打开框
        static OSString s_resolved;
        if (!s_resolved.IsEmpty())
            return s_resolved;

        if (g_argc > 1 && g_argv && g_argv[1] && g_argv[1][0] != 0)
        {
            s_resolved = OSString(g_argv[1]);
            return s_resolved;
        }

        s_resolved = ShowOpenFileDialog();
        return s_resolved;
    }

    bool LoadAsset()
    {
        const OSString &path = ResolveAssetPath();
        if (path.IsEmpty())
        {
            GLogInfo(u8"[EnvSunPicker] 未选择资产");
            return false;
        }

        asset_path = path;

        EnvironmentAssetRegistry &registry = environment_system->GetEnvironmentRegistry();
        asset = registry.LoadFile(asset_path);
        if (!asset)
        {
            GLogError(u8"[EnvSunPicker] 加载环境资产失败：%ls（%s）",
                      asset_path.c_str(), registry.GetLastError().c_str());
            asset_path.Clear();
            return false;
        }

        env_uri = OS_TEXT("env:file:") + asset_path;

        // ── 收集资产里实际存在的技术档位（数字键 1..N 的顺序）──
        const EnvironmentSpec &spec = asset->GetSpec();
        technique_count = 0;
        auto push = [&](SkyTechnique t)
        {
            if (technique_count < 5)
                techniques[technique_count++] = t;
        };
        if (spec.cubemap.present)       push(SkyTechnique::CubeMap);
        if (spec.octahedral.present)    push(SkyTechnique::Octahedral);
        if (spec.lambert_dome.present)  push(SkyTechnique::LambertDome);
        if (spec.atmosphere.present)    push(SkyTechnique::Atmosphere);
        if (spec.sh9.auto_generate.present || spec.sh9.bands.present)
            push(SkyTechnique::SH9);

        if (technique_count == 0)
        {
            GLogError(u8"[EnvSunPicker] 资产没有任何已配置的技术段：%ls", asset_path.c_str());
            return false;
        }

        // 起步档位 = [ambient].mode 指向的技术（存在才用），否则第一个可用档位
        technique_index = 0;
        if (spec.ambient.mode.present)
        {
            SkyTechnique mode_tech;
            if (SkyModeToTechnique(spec.ambient.mode.value, mode_tech))
            {
                for (int i = 0; i < technique_count; ++i)
                {
                    if (techniques[i] == mode_tech)
                    {
                        technique_index = i;
                        break;
                    }
                }
            }
        }

        // 初始太阳方向：资产里写了就用写的，没写用兜底值（点击后会被覆写并落盘）
        sun_written_in_asset = spec.sun.present && spec.sun.value.direction.present;
        if (sun_written_in_asset)
            sun_direction = NormalizeOr(spec.sun.value.direction.value, sun_direction);

        return true;
    }

    bool SetupCurrentTechnique()
    {
        if (!environment_system || !asset)
            return false;

        const SkyTechnique tech = techniques[technique_index];

        if (!environment_system->SetupSkyEnvironment(env_uri, tech))
        {
            GLogError(u8"[EnvSunPicker] 装配档位失败：%s", GetSkyTechniqueName(tech));
            return false;
        }

        // 太阳方向同步主光。烘焙档位贴图里的太阳是烤死的，这里只动光照方向；
        // 大气档的天空视觉随 [sun] 走，由 SetupSkyEnvironment 自己消费。
        if (light_system)
            light_system->SetMainLightDirection(sun_direction);

        GLogInfo(u8"[EnvSunPicker] 档位=%s（%d/%d），太阳=(%.3f, %.3f, %.3f)%s",
                 GetSkyTechniqueName(tech), technique_index + 1, technique_count,
                 sun_direction.x, sun_direction.y, sun_direction.z,
                 sun_written_in_asset ? "" : " [资产未写[sun]，点击将写入]");
        return true;
    }

    // ════════════════════════════════════════════════════════════
    //  线段（RayPicking 同款：GeometryCreater + 动态 VAB 写回）
    // ════════════════════════════════════════════════════════════

    void WriteSunLine()
    {
        if (!line_vab)
            return;

        const math::Vector3f dir = NormalizeOr(sun_direction, math::Vector3f(0.0f, 0.0f, 1.0f));
        const math::Vector3f tip(dir.x * kSunLineLength, dir.y * kSunLineLength, dir.z * kSunLineLength);

        // 端点十字的两条正交臂：e1 取与 dir 正交的任意方向（dir 的 xy 投影退化时给 +X），
        // e2 = dir × e1
        math::Vector3f e1 = NormalizeOr(math::Vector3f(-dir.y, dir.x, 0.0f),
                                        math::Vector3f(1.0f, 0.0f, 0.0f));
        math::Vector3f e2(dir.y * e1.z - dir.z * e1.y,
                          dir.z * e1.x - dir.x * e1.z,
                          dir.x * e1.y - dir.y * e1.x);
        e2 = NormalizeOr(e2, math::Vector3f(0.0f, 0.0f, 1.0f));

        // 注意：sizeof(Vector3f)==16（GLM 对齐填充）而 VAB stride==12，
        //       必须用紧凑 float[][3] 写，不能直接 Vector3f[]（见 RayPicking 注释）
        float pts[6][3];
        pts[0][0] = 0.0f;                        pts[0][1] = 0.0f;                        pts[0][2] = 0.0f;
        pts[1][0] = tip.x;                       pts[1][1] = tip.y;                       pts[1][2] = tip.z;
        pts[2][0] = tip.x - e1.x * kSunCrossArm; pts[2][1] = tip.y - e1.y * kSunCrossArm; pts[2][2] = tip.z - e1.z * kSunCrossArm;
        pts[3][0] = tip.x + e1.x * kSunCrossArm; pts[3][1] = tip.y + e1.y * kSunCrossArm; pts[3][2] = tip.z + e1.z * kSunCrossArm;
        pts[4][0] = tip.x - e2.x * kSunCrossArm; pts[4][1] = tip.y - e2.y * kSunCrossArm; pts[4][2] = tip.z - e2.z * kSunCrossArm;
        pts[5][0] = tip.x + e2.x * kSunCrossArm; pts[5][1] = tip.y + e2.y * kSunCrossArm; pts[5][2] = tip.z + e2.z * kSunCrossArm;

        if (!line_vab->Write(pts, 6u))
            GLogWarning(u8"[EnvSunPicker] 太阳线 VAB 写入失败");
    }

    bool CreateSunLine()
    {
        auto *device = GetDevice();
        auto *buffer_manager = GetManager<BufferManager>();
        auto *geometry_manager = GetManager<GeometryManager>();
        if (!device || !buffer_manager || !geometry_manager)
            return false;

        GeometryCreater gc(device, CreateVertexLuminance3DGeometryVertexFormat(), buffer_manager);
        gc.Init("EnvSunPicker.SunRay", 6);
        if (!gc.WriteVAB(VAN::Position, VF_V3F, g_line_positions) ||
            !gc.WriteVAB(VAN::Luminance, VF_V1UN8, g_line_luminance))
            return false;

        geom_line = gc.Create();
        if (!geom_line)
            return false;
        geometry_manager->Add(geom_line);   // 生命周期交 GeometryManager（同 RayPicking）

        line_asset = PrimitiveAsset(geom_line, &line_recipe, PrimitiveType::Lines);

        auto *line_entity = ecs_context->CreateEntity<Entity>("SunRay");
        line_vab = geom_line->GetVAB(VAN::Position);

        auto transform = ecs_context->GetTransform(
            ecs_context->CreateTransform(line_entity->GetEntityID(), Mobility::Static));
        transform.SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));

        auto *primitive_comp = line_entity->GetContext()->GetOrCreateGeometryData(line_entity);
        auto *material_data_comp = line_entity->GetContext()->GetOrCreateMaterialData(line_entity);
        primitive_comp->GetOwner()->GetContext()
            ->GetOrCreateGeometryData(primitive_comp->GetOwnerID())->SetPrimitiveAsset(&line_asset);
        MaterialData::MaterialDataAuthoringResource line_struct{};
        line_struct = line_material_data_accessor;
        material_data_comp->SetDataResource(line_struct);

        WriteSunLine();
        return true;
    }

    bool InitLineMaterial()
    {
        const mtl::MaterialRecipe *loaded = GetRecipe("RayPicking.Line");
        if (!loaded)
        {
            GLogError(u8"[EnvSunPicker] 找不到配方 RayPicking.Line");
            return false;
        }
        line_recipe = *loaded;

        if (!GetMaterialDataBinding(MATERIAL_DATA_URI, MATERIAL_DATA_ROW_LINE, line_material_data_accessor))
            return false;
        if (!(line_recipe.material_ssbo_binding = line_material_data_accessor).IsValid())
            return false;
        return true;
    }

    // ════════════════════════════════════════════════════════════
    //  相机：钉在世界原点的第一人称（穹顶内部视角）
    // ════════════════════════════════════════════════════════════

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        // 相机 = 世界级资源：经世界访问器创建（实体只是宿主）
        camera = ecs_context->GetOrCreateCamera(camera_entity);

        camera->control_mode = CameraControlMode::FirstPerson;
        camera->position = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->world_up = math::Vector3f(0.0f, 0.0f, 1.0f);   // 世界 Z-up
        camera->yaw = 0.0f;
        camera->pitch = 0.0f;
        camera->fov = 70.0f;
        camera->near_plane = 0.05f;
        camera->far_plane = 2000.0f;
        camera->rotation_sensitivity = 0.2f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;
        camera->viewport_info = GetViewportInfo();
        return true;
    }

    // ════════════════════════════════════════════════════════════
    //  拾取
    // ════════════════════════════════════════════════════════════

    void PickSunAtMouse()
    {
        const CameraInfo *ci = ecs_context ? ecs_context->ResolveActiveCameraInfo() : nullptr;
        const ViewportInfo *vi = GetViewportInfo();
        const math::Vector2i *mouse = GetMouseCoord();
        if (!ci || !vi || !mouse || !asset)
            return;

        math::Ray ray;
        ray.SetFromViewportPoint(*mouse, ci, vi->GetViewport());

        // 相机钉在原点 ⇒ 视线方向即“球面命中点”的方向（不需要求交）
        const math::Vector3f dir = NormalizeOr(ray.direction, sun_direction);
        ApplySunDirection(dir);
    }

    void ApplySunDirection(const math::Vector3f &dir)
    {
        sun_direction = dir;
        sun_written_in_asset = true;

        // 写回资产内存态：[sun].direction（Presence 双标志都要置）
        EnvironmentSpec &spec = asset->GetSpec();
        spec.sun.present = true;
        spec.sun.value.direction.present = true;
        spec.sun.value.direction.value = dir;

        dirty = true;
        esc_confirm = false;
        UpdateWindowTitle();

        if (light_system)
            light_system->SetMainLightDirection(dir);

        // 热重装配当前档位：大气档的太阳位置等随之更新
        // （烘焙档位贴图不变，只有主光方向随点击走 —— 这是设计语义）
        SetupCurrentTechnique();
        WriteSunLine();

        GLogInfo(u8"[EnvSunPicker] 太阳方向 = (%.4f, %.4f, %.4f)（Ctrl+S 保存）",
                 dir.x, dir.y, dir.z);
    }

    // ════════════════════════════════════════════════════════════
    //  保存：双格式同写（.env 二进制 + .env.toml 文本）
    // ════════════════════════════════════════════════════════════

    static void StripEnvExtension(OSString &path)
    {
        if (path.EndsWith(OS_TEXT(".env.toml")))
            path = path.Left(path.Length() - 9);
        else if (path.EndsWith(OS_TEXT(".env")))
            path = path.Left(path.Length() - 4);
    }

    bool SaveAsset()
    {
        if (!asset)
            return false;

        OSString stem = asset_path;
        StripEnvExtension(stem);

        AnsiString error;
        bool ok = true;

        // 与 BuiltinAssetExport 相同的统一序列化通路：扩展名定格式（Auto 解析）
        const OSString toml_path = stem + OS_TEXT(".env.toml");
        if (!hgl::serialize::SaveAssetObjectToFile(*asset, toml_path,
                                                   hgl::serialize::ArchiveFormat::Auto, &error))
        {
            GLogError(u8"[EnvSunPicker] 写文本格式失败：%ls（%s）", toml_path.c_str(), error.c_str());
            ok = false;
        }

        const OSString flat_path = stem + OS_TEXT(".env");
        if (!hgl::serialize::SaveAssetObjectToFile(*asset, flat_path,
                                                   hgl::serialize::ArchiveFormat::Auto, &error))
        {
            GLogError(u8"[EnvSunPicker] 写二进制格式失败：%ls（%s）", flat_path.c_str(), error.c_str());
            ok = false;
        }

        if (ok)
        {
            dirty = false;
            esc_confirm = false;
            UpdateWindowTitle();
            GLogInfo(u8"[EnvSunPicker] 已保存：%ls（.env 与 .env.toml）", stem.c_str());
        }
        return ok;
    }

    void UpdateWindowTitle();

    // ════════════════════════════════════════════════════════════
    //  输入处理（边沿检测）
    // ════════════════════════════════════════════════════════════

    void HandleInput(InputSystem *input)
    {
        bool keys[kKeySlotCount];
        keys[0] = input->IsKeyDown(io::KeyboardButton::Esc);
        keys[1] = input->IsKeyDown(io::KeyboardButton::S);
        keys[2] = input->IsKeyDown(io::KeyboardButton::_1);
        keys[3] = input->IsKeyDown(io::KeyboardButton::_2);
        keys[4] = input->IsKeyDown(io::KeyboardButton::_3);
        keys[5] = input->IsKeyDown(io::KeyboardButton::_4);
        keys[6] = input->IsKeyDown(io::KeyboardButton::_5);
        keys[7] = input->IsKeyDown(io::KeyboardButton::LeftCtrl)
               || input->IsKeyDown(io::KeyboardButton::RightCtrl);

        bool edges[kKeySlotCount];
        for (int i = 0; i < kKeySlotCount; ++i)
        {
            edges[i] = keys[i] && !prev_keys[i];
            prev_keys[i] = keys[i];
        }

        const bool ctrl = keys[7];

        if (edges[1] && ctrl)          // Ctrl+S
        {
            SaveAsset();
        }
        else
        {
            for (int i = 0; i < 5; ++i)  // 1..5 切档（按住 Ctrl 时不当档位）
            {
                if (edges[2 + i] && !ctrl)
                {
                    if (i < technique_count)
                    {
                        technique_index = i;
                        SetupCurrentTechnique();
                    }
                    else
                    {
                        GLogInfo(u8"[EnvSunPicker] 档位 %d 在本资产中不存在", i + 1);
                    }
                    break;
                }
            }
        }

        if (edges[0])                  // Esc：脏时两段式确认
        {
            if (dirty && !esc_confirm)
            {
                esc_confirm = true;
                GLogWarning(u8"[EnvSunPicker] 有未保存修改。Ctrl+S 保存，再按一次 Esc 放弃并退出");
            }
            else
            {
                RequestQuit();
            }
        }

        // ── 鼠标左键：按下记点，抬起算位移；小于容差 ⇒ 点击选太阳 ──
        const bool lmb = input->IsMouseButtonDown(io::MouseButton::Left);
        const math::Vector2i *mouse = GetMouseCoord();

        if (lmb && !prev_lmb_down)
        {
            lmb_pressed = true;
            if (mouse)
                lmb_press_pos = *mouse;
        }
        else if (!lmb && prev_lmb_down && lmb_pressed)
        {
            lmb_pressed = false;
            if (mouse)
            {
                const int dx = mouse->x - lmb_press_pos.x;
                const int dy = mouse->y - lmb_press_pos.y;
                if (dx * dx + dy * dy <= kClickMoveTolerancePx * kClickMoveTolerancePx)
                    PickSunAtMouse();
            }
        }
        prev_lmb_down = lmb;
    }

    /// Esc 关闭：走原生 WM_CLOSE（框架循环以窗口关闭标志收尾）；拿不到句柄则退进程兜底
    void RequestQuit();

    /// 自检：加载 → 改方向 → 双写临时目录 → 重载比对。返回是否通过
    bool RunSelfCheck(const OSString &src_path);

public:

    ~EnvSunPickerApp() override
    {
        geom_line = nullptr;
        line_vab = nullptr;
        if (auto *registry = GetGeometryAssetRegistry())
            registry->Clear();
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.02f, 0.02f, 0.03f, 1.0f));

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

        selfcheck_active = (getenv("SUNPICKER_SELFCHECK") != nullptr);
        if (selfcheck_active)
        {
            if (g_argc < 2)
            {
                GLogError(u8"[EnvSunPicker] 自检需要资产路径参数");
                return false;
            }
            const OSString src(g_argv[1]);
            const bool pass = RunSelfCheck(src);
            GLogInfo(pass ? u8"[EnvSunPicker] SELFCHECK PASS" : u8"[EnvSunPicker] SELFCHECK FAIL");
            std::exit(pass ? 0 : 1);
        }

        if (!InitLineMaterial())
            return false;

        if (!LoadAsset())
            return false;

        if (!InitCamera())
            return false;

        if (!CreateSunLine())
            return false;

        if (!SetupCurrentTechnique())
            return false;

        UpdateWindowTitle();

        GLogInfo(u8"[EnvSunPicker] 打开：%ls —— 左键拖拽环视 / 左键点击选太阳 / 1-%d 切档 / Ctrl+S 保存 / Esc 退出",
                 asset_path.c_str(), technique_count);
        return true;
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);

        // 眼睛钉死在世界原点（FirstPerson 的 WASD 若挪了位置也复位）
        if (camera &&
            (camera->position.x != 0.0f || camera->position.y != 0.0f || camera->position.z != 0.0f))
        {
            camera->position = math::Vector3f(0.0f, 0.0f, 0.0f);
            camera->matrix_dirty = true;
        }

        auto input = ecs_context->GetSystem<InputSystem>();
        if (!input)
            input = ecs_context->RegisterSystem<InputSystem>();
        if (!input)
            return;

        HandleInput(input.get());
    }
};//class EnvSunPickerApp:public WorkObject

// ════════════════════════════════════════════════════════════════════
//  Windows 平台段：系统打开框 / 窗口标题 / 关闭
// ════════════════════════════════════════════════════════════════════

#ifdef _WIN32

namespace
{
    HWND FindToolWindow()
    {
        static HWND s_hwnd = nullptr;
        if (!s_hwnd)
            s_hwnd = FindWindowW(nullptr, kWindowTitle);
        return s_hwnd;
    }
}//namespace

OSString EnvSunPickerApp::ShowOpenFileDialog()
{
    wchar_t buffer[4096];
    buffer[0] = 0;

    OPENFILENAMEW ofn{};
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = FindToolWindow();
    ofn.lpstrFilter  = L"Environment 资产 (*.env;*.env.toml)\0*.env;*.env.toml\0所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile    = buffer;
    ofn.nMaxFile     = static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0]));
    ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (!GetOpenFileNameW(&ofn))
        return OSString();

    return OSString(buffer);
}

void EnvSunPickerApp::UpdateWindowTitle()
{
    HWND hwnd = FindToolWindow();
    if (!hwnd)
        return;

    // 标题带文件名尾段与脏标记
    OSString name = asset_path;
    int cut = name.FindRightChar(OS_TEXT('\\'));
    const int cut2 = name.FindRightChar(OS_TEXT('/'));
    if (cut2 > cut)
        cut = cut2;
    if (cut >= 0 && cut + 1 < name.Length())
        name = name.Right(name.Length() - cut - 1);

    OSString cap = OS_TEXT("EnvSunPicker - ") + name;
    if (dirty)
        cap += OS_TEXT(" *");

    SetWindowTextW(hwnd, cap.c_str());
}

void EnvSunPickerApp::RequestQuit()
{
    if (HWND hwnd = FindToolWindow())
    {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);   // 走框架的正常收尾路径
        return;
    }
    std::exit(dirty ? 1 : 0);                // 兜底：拿不到句柄（不该发生）
}

#else // !_WIN32

// 非 Windows：暂无系统打开框（argv 路径仍可用），标题/关闭走框架默认
OSString EnvSunPickerApp::ShowOpenFileDialog()
{
    GLogWarning(u8"[EnvSunPicker] 当前平台没有原生打开框，请把资产路径作为命令行参数传入");
    return OSString();
}

void EnvSunPickerApp::UpdateWindowTitle()
{
}

void EnvSunPickerApp::RequestQuit()
{
    std::exit(0);
}

#endif // _WIN32

// ════════════════════════════════════════════════════════════════════
//  自检实现（跨平台：临时目录 + 注册表重载比对）
// ════════════════════════════════════════════════════════════════════

bool EnvSunPickerApp::RunSelfCheck(const OSString &src_path)
{
    EnvironmentAssetRegistry &registry = environment_system->GetEnvironmentRegistry();

    EnvironmentAsset *a = registry.LoadFile(src_path);
    if (!a)
    {
        GLogError(u8"[EnvSunPicker][selfcheck] 加载失败：%ls（%s）",
                  src_path.c_str(), registry.GetLastError().c_str());
        return false;
    }

    const math::Vector3f probe(0.1234f, 0.5678f, 0.8147f);   // 任意非退化方向（自检专用）
    EnvironmentSpec &spec = a->GetSpec();
    spec.sun.present = true;
    spec.sun.value.direction.present = true;
    spec.sun.value.direction.value = probe;

    OSString tmp_dir;
#ifdef _WIN32
    wchar_t dirbuf[MAX_PATH];
    if (GetTempPathW(MAX_PATH, dirbuf) == 0)
        return false;
    tmp_dir = OSString(dirbuf);
#else
    const char *tmp = getenv("TMPDIR");
    tmp_dir = AnsiString(tmp ? tmp : "/tmp").template As<os_char>();
#endif
    if (!tmp_dir.IsEmpty() && !tmp_dir.EndsWith(OS_TEXT("/")) && !tmp_dir.EndsWith(OS_TEXT("\\")))
        tmp_dir += OS_TEXT("/");
    const OSString stem = tmp_dir + OS_TEXT("sunpicker_selfcheck");

    AnsiString error;
    if (!hgl::serialize::SaveAssetObjectToFile(*a, stem + OS_TEXT(".env.toml"),
                                               hgl::serialize::ArchiveFormat::Auto, &error)
     || !hgl::serialize::SaveAssetObjectToFile(*a, stem + OS_TEXT(".env"),
                                               hgl::serialize::ArchiveFormat::Auto, &error))
    {
        GLogError(u8"[EnvSunPicker][selfcheck] 双格式落盘失败：%s", error.c_str());
        return false;
    }

    // 两个格式各重载一次，比对 [sun].direction 是否原样回来
    for (int fmt = 0; fmt < 2; ++fmt)
    {
        const OSString p = (fmt == 0) ? (stem + OS_TEXT(".env.toml")) : (stem + OS_TEXT(".env"));
        EnvironmentAsset *b = registry.LoadFile(p);
        if (!b)
        {
            GLogError(u8"[EnvSunPicker][selfcheck] 重载失败：%ls（%s）",
                      p.c_str(), registry.GetLastError().c_str());
            return false;
        }
        const EnvironmentSpec &rs = b->GetSpec();
        if (!rs.sun.present || !rs.sun.value.direction.present)
        {
            GLogError(u8"[EnvSunPicker][selfcheck] 重载后 [sun].direction 丢失：%ls", p.c_str());
            return false;
        }
        const math::Vector3f &got = rs.sun.value.direction.value;
        const float dot = got.x * probe.x + got.y * probe.y + got.z * probe.z;
        if (dot < 0.999999f || std::fabs(got.x - probe.x) > 1e-5f)
        {
            GLogError(u8"[EnvSunPicker][selfcheck] 方向不匹配（dot=%.7f）：%ls", dot, p.c_str());
            return false;
        }
        GLogInfo(u8"[EnvSunPicker][selfcheck] round-trip OK：%ls", p.c_str());
    }

    return true;
}

int os_main(int argc, os_char **argv)
{
    g_argc = argc;
    g_argv = argv;
    return RunFramework<EnvSunPickerApp>(kWindowTitle, argc, argv, 1280, 720);
}
