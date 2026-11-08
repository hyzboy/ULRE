#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/asset/GeometryPreset.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/ssbo/GlobalSSBOTypes.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/graph/ssbo/LitMaterialData.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/support/transform/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/light_shadow/EnvironmentSystem.h>
#include<hgl/ecs/systems/render/light_shadow/LightSystem.h>

#include<hgl/graph/gizmo/SunDirectionControlSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<glm/gtx/quaternion.hpp>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include "../Shared/FrameDigest.h"   // 帧摘要自检（T0 判据前置共用件）

#include<vector>
#include<memory>
#include<string>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    // 材质数据行的唯一来源：一个 URI + 一个行号，材质数值全部在块文件里
    constexpr char   MATERIAL_DATA_URI[] =
        "mtldata:pack(ShaderLibrary):asset/material_data/ibl_environment";
    constexpr uint32 kMeshRowCount = 5;   // floor / sphere / cube / torus / arrow
}

namespace
{
    GeometryVertexFormat CreateStandardGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2F},
            {VertexSemantic::Normal,   VF_V3F},
        };
        return gvf;
    }

    /**
     * `IBLENV_OCT=1` / `IBLENV_SH=1`：从统一天空资产 `sky_34` 里**选一个画质档位**
     * （`SkyTechnique`），三个档位就是本示例原来的三条手工拼装路径：
     *   · `CubeMap`（默认）：`[cubemap]` 段 —— 立方体贴图 IBL（irradiance /
     *     prefilter 的 `.TexCube` 版）+ 全天球背景（`IBLEnvironment.Sky` 配方）。
     *   · `Octahedral`（`IBLENV_OCT=1`）：`[octahedral]` 段 —— 八面体展开 IBL
     *     （`.TexOct` 版）。split-sum 公式、`lod = roughness × (mip-1)` 映射、
     *     LDR 曝光调制与 `CubeMap` **完全同一套**，唯一差别是纹理寻址 ⇒ 这一档
     *     同时是 `ambient_light_ibl_oct` 模块的现场判据：同一场景同一组材质参数，
     *     两条路径的亮度/颜色必须高度接近。
     *   · `SH9`（`IBLENV_SH=1`）：顶层 `[sh9]`（`auto_generate`）—— **一张贴图
     *     都不读**，由 `EnvironmentSystem` 按解析天空 + 当前太阳方向现场合成 9 个
     *     SH 系数并随太阳重算（方向性上亮下暗生效）⇒ `ambient_light_sh` 的现场
     *     判据：证明「零烘焙 / 零贴图」的低频天光路径能真的出图，而不是退化成黑球。
     *     该档没有视觉天空（`SHLightOnly`），背景即清屏色。
     *
     * 原示例另有 `IBLENV_SKY_OCT=1`（只换背景天空球投影、环境 IBL 仍走 cube），
     * 用来演示旧手工 API 里「视觉与天光解耦」；统一资产把每个技术段的背景配方
     * **由作者写死配对**（`[cubemap]`→Cube 背景、`[octahedral]`→Oct 背景），这个
     * 开关随之退役 —— 这正是「配置资产化」的预期行为：可写的组合由资产定义，
     * 示例只负责选档。
     */
    SkyTechnique AmbientTechnique()
    {
        static const SkyTechnique s_technique =
            getenv("IBLENV_SH") != nullptr  ? SkyTechnique::SH9
          : getenv("IBLENV_OCT") != nullptr ? SkyTechnique::Octahedral
                                            : SkyTechnique::CubeMap;

        return s_technique;
    }

    /// 预设资产目录（开机扫一次，之后 `env:@Name` 才能命中；与 showcase 同源）。
    constexpr os_char kEnvAssetDirectory[] = OS_TEXT("ShaderLibrary/asset/environment");

    /// 统一天空资产：五种技术并列配全，画质档位在**资产外面**选。
    constexpr os_char kSky34URI[] = OS_TEXT("env:@Sky34");
}

#define DRAW_SKY_SPHERE
#define DRAW_GIZMO

/**
 * IBL 环境光照示例:
 *
 * 以 sky_34 全景环境为天空背景,对场景物体做基于图像的光照(split-sum)。
 * IBL 资源由 TexConv 转换产出:
 *   * <env>_irradiance  漫反射辐照度(按世界法线采样)
 *   * <env>_prefilter   镜面预滤波(按反射向量采样)
 *   * brdflut           BRDF LUT((NdotV,roughness) → scale/bias)
 *
 * 三种光照路径 = **同一份统一天空资产** `env:@Sky34` 的三个技术档位
 * (`SkyTechnique`,画质档位在选择方,不在资产里;详见 `AmbientTechnique()`):
 *   · 默认      → `CubeMap`(高配 HDR-free LDR IBL,`.TexCube` 寻址);
 *   · IBLENV_OCT → `Octahedral`(低配,`.TexOct` 寻址,光照公式不变);
 *   · IBLENV_SH  → `SH9`(超低配,零贴图,解析合成球谐,无视觉天空)。
 *
 * 天空外观与天光整块交给 `EnvironmentSystem::SetupSkyEnvironment(uri, technique)`
 * 托管:背景配方、贴图 URI、LDR 曝光策略、几何体与 scale 全在 `sky_34.env.toml`
 * 的对应技术段里。示例自己不再拼 `SkyEnvironmentConfig`、不碰 bindless 句柄、
 * 不写 `ambient_env`、不建天空实体。
 *
 * 场内物体是不同金属度/粗糙度的 PBR 材质("lit"),便于对比 IBL 在不同表面上的表现。
 */
class IBLEnvironmentApp : public WorkObject
{
private:

    struct MeshEntry
    {
        Geometry* geometry = nullptr;
        // Geometry 由 GeometryAssetRegistry 持有，本结构只做观察（勿在此 delete）
        PrimitiveAsset asset{};
    };

    ECSContext* ecs_context = nullptr;
    Entity* camera_entity = nullptr;

#ifdef DRAW_SKY_SPHERE
    /// 天空实体 + 天光数据 + provider 全由它托管（含 IBL 句柄的 bindless 注册）
    std::shared_ptr<EnvironmentSystem> environment_system;
    /// T6b：太阳方向写进**世界私有光源行 0**（sky 已不含光源参数）。
    std::shared_ptr<LightSystem> light_system;
#endif//DRAW_SKY_SPHERE

#ifdef DRAW_GIZMO
    std::shared_ptr<SunDirectionControlSystem> sun_gizmo_system;
#endif//DRAW_GIZMO

    graph::mtl::MaterialRecipe mesh_recipe{};
    /// 每个物体一行 PBR 参数；行数固定，内容在块文件里
    graph::GlobalSSBOBinding mesh_rows[kMeshRowCount]{};
    VertexDataManager* mesh_vdm = nullptr;

    MeshEntry* floor_mesh = nullptr;

    std::vector<std::unique_ptr<MeshEntry>> meshes;

private:

    bool InitEnvironmentControl()
    {
        if (!ecs_context)
            return false;

    #ifdef DRAW_SKY_SPHERE
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

        light_system->SetMainLightDirection(math::Vector3f(0.2f, 0.7f, 0.68f));
            #endif//DRAW_SKY_SPHERE

    #ifdef DRAW_GIZMO
        sun_gizmo_system = ecs_context->GetSystem<SunDirectionControlSystem>();
        if (!sun_gizmo_system)
            sun_gizmo_system = ecs_context->RegisterSystem<SunDirectionControlSystem>();

        if (!sun_gizmo_system)
            return false;

        sun_gizmo_system->SetLightSystem(light_system.get());
        sun_gizmo_system->SetGizmoPosition(math::Vector3f(0.0f, 0.0f, 0.0f));
    #endif//DRAW_GIZMO

        // 太阳方向必须在**天空托管之前**写进光源行 0：SH9 系数由解析合成器按当前
        // 太阳方向现算，装晚了会用"正顶太阳"那一版烘出天光。
        return InitSkyEnvironment();
    }

#ifdef DRAW_SKY_SPHERE
    /**
     * 选一个档位装给托管层：示例侧只剩「URI + 档位」两个参数。
     *
     * 投影形态（`.TexCube` / `.TexOct` 由 `texcube:` / `texoct:` 前缀推导）、
     * provider 名、HDR/LDR 策略、几何体与 scale 全在 `sky_34.env.toml` 的对应
     * 技术段 —— 与 showcase 五档示例共用同一份资产、同一组段。
     * `LoadEnvironmentDirectory` 每进程扫一次即可（`env:@Name` 只认已注册条目）。
     */
    bool InitSkyEnvironment()
    {
        if (!environment_system)
            return false;

        static bool s_dir_loaded = false;

        if (!s_dir_loaded)
        {
            int loaded = 0;
            int errors = 0;

            if (!environment_system->LoadEnvironmentDirectory(kEnvAssetDirectory, &loaded, &errors)
                || loaded < 1 || errors > 0)
            {
                GLogError(u8"[IBLEnvironment] 环境预设目录加载失败：%ls", kEnvAssetDirectory);
                return false;
            }

            s_dir_loaded = true;
        }

        return environment_system->SetupSkyEnvironment(kSky34URI, AmbientTechnique());
    }
#endif//DRAW_SKY_SPHERE

    bool InitMaterial()
    {
        // 与默认 Lit 材质同一套 PBR 表面：环境光**算法**由环境层决定（见 InitSkyEnvironment）
        const mtl::MaterialRecipe *loaded_mesh_recipe = GetRecipe("rcp:pack(ShaderLibrary):asset/recipe/Examples/Environment/IBLEnvironment.Lit");
        if (!loaded_mesh_recipe)
            return false;
        mesh_recipe = *loaded_mesh_recipe;

        return true;
    }

    bool InitVDM()
    {
        auto* buffer_manager = GetManager<BufferManager>();
        if (!buffer_manager)
            return false;

        mesh_vdm = new VertexDataManager(
            buffer_manager,
            CreateStandardGeometryVertexFormat());
        if (!mesh_vdm)
            return false;

        if (!mesh_vdm->Init(HGL_SIZE_1MB, HGL_SIZE_1MB, IndexType::U32))
            return false;

        return true;
    }

    MeshEntry* CreateMeshEntry(Geometry* geometry)
    {
        if (!geometry)
            return nullptr;

        auto mesh = std::make_unique<MeshEntry>();
        mesh->geometry = geometry;
        mesh->asset = PrimitiveAsset(geometry, &mesh_recipe, PrimitiveType::Triangles);

        MeshEntry* result = mesh.get();
        meshes.push_back(std::move(mesh));

        return result;
    }

    bool CreateGeometryMesh()
    {
        using namespace inline_geometry;

        // 共享顶点池：一批几何写进同一个 VertexDataManager
        GeometryCreater pc(mesh_vdm);
        SharedCreaterScope scope(GetGeometryAssetRegistry(), &pc);

        {
            auto geom = GetGeometry("geo:builtin:plane_square");
            if (!geom)
                return false;

            floor_mesh = CreateMeshEntry(geom);
            if (!floor_mesh)
                return false;
        }

        {
            auto geom = GetGeometry("geo:builtin:sphere_64");
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }

        {

            auto geom = GetGeometry("geo:builtin:cube_2x2x2");
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }

        {

            auto geom = GetGeometry("geo:builtin:torus_ring");
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }

#if 0    // Arrow is not part of the *.geo.toml shape schema yet
        {

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreateArrow(pc, &aci);
            });
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }
#endif

        return true;
    }

    /// 每个物体一行独立的 PBR 参数,便于对比 IBL 在不同材质上的表现:
    ///   地板:白色粗糙电介质(以纯漫反射为主,看 irradiance)
    ///   球体:抛光金属(看 prefilter 反射)
    ///   立方体:半粗糙金属
    ///   圆环:红色光滑电介质
    ///   箭头:蓝色光滑电介质
    bool CreateMeshMaterialRows()
    {
        if (!GetMaterialDataBinding(MATERIAL_DATA_URI, 0, mesh_rows[0]))
            return false;

        for (uint32_t i = 1; i < kMeshRowCount; ++i)
            mesh_rows[i] = mesh_rows[0].RowOffset(i);

        return meshes.size() <= kMeshRowCount;
    }

    bool InitSceneEntities()
    {
        if (!ecs_context || !floor_mesh)
            return false;

        if (!CreateMeshMaterialRows())
            return false;

        // 天空实体（`SkyEnvironment`）已由 `SetupSkyEnvironment()` 建好并托管，
        // 这里只负责场景里的物体群。

        {
            auto* entity = ecs_context->CreateEntity<Entity>("Floor");
            auto transform = ecs_context->GetTransform(ecs_context->CreateTransform(entity->GetEntityID(), Mobility::Static));
            auto primitive_comp = entity->GetContext()->GetOrCreateGeometryData(entity);
            hgl::ecs::MaterialData *material_data_comp = entity->GetContext()->GetOrCreateMaterialData(entity);

            transform.SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            transform.SetLocalRotation(glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
            transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform.SetMobility(Mobility::Static);

            primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(primitive_comp->GetOwnerID())->SetPrimitiveAsset(&floor_mesh->asset);
            // 纹理已由配方文件声明（[[textures]]），实体端不再重复绑定
            hgl::ecs::MaterialData::MaterialDataAuthoringResource floor_struct{};
            floor_struct = mesh_rows[0];
            material_data_comp->SetDataResource(floor_struct);
            // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删
        }

        const size_t total = meshes.size();
        const size_t mesh_count = total > 1 ? (total - 1) : 1;
        size_t index = 0;

        for (auto& mesh_ptr : meshes)
        {
            auto* rm = mesh_ptr.get();
            if (!rm || rm == floor_mesh)
                continue;

            const size_t row_index = index + 1;
            if (row_index >= kMeshRowCount)
                return false;

            auto* entity = ecs_context->CreateEntity<Entity>("Mesh_" + std::to_string(index));
            auto transform = ecs_context->GetTransform(ecs_context->CreateTransform(entity->GetEntityID(), Mobility::Static));
            auto primitive_comp = entity->GetContext()->GetOrCreateGeometryData(entity);
            hgl::ecs::MaterialData *material_data_comp = entity->GetContext()->GetOrCreateMaterialData(entity);

            float angle = glm::radians(360.0f * static_cast<float>(index) / static_cast<float>(mesh_count));
            glm::quat rotation = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
            glm::vec3 pos = glm::rotate(rotation, glm::vec3(6.5f, 0.0f, 0.0f));

            transform.SetLocalPosition(pos);
            transform.SetLocalRotation(rotation);
            transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform.SetMobility(Mobility::Static);

            primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(primitive_comp->GetOwnerID())->SetPrimitiveAsset(&rm->asset);
            hgl::ecs::MaterialData::MaterialDataAuthoringResource mesh_struct{};
            mesh_struct = mesh_rows[row_index];
            material_data_comp->SetDataResource(mesh_struct);
            // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

            ++index;
        }

        return true;
    }

    bool InitScene()
    {
        ecs_context = GetECSContext();
        if (!ecs_context)
            return false;

        // 天空实体 + 天光（句柄 / SH / provider）在 InitEnvironmentControl() 里
        // 已由 SetupSkyEnvironment() 一次托管完成。
        if (!InitEnvironmentControl())
            return false;

        if (!InitVDM())
            return false;

        if (!CreateGeometryMesh())
            return false;

        return InitSceneEntities();
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        // 相机 = **世界级资源**：经世界访问器创建/取回（实体只是宿主），不直取组件
        auto *camera = ecs_context->GetOrCreateCamera(camera_entity);

        camera->control_mode = CameraControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 16.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:
    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);

        // IBLENV_SELFCHECK=1：**帧摘要自检**（T0 判据前置；共用件 example/Shared/FrameDigest.h）——
        // 第 8 帧帧内回读颜色靶 ⇒ {不同字节取值数, 平均亮度, FNV-1a 64} 比对下方内联常量：
        // PASS 退出 0 / FAIL 退出 1。
        // 为什么本例必须有它：IBL 句柄（env_tex）是八台架摘要的**盲区**（只有本示例把 indirect_ibl
        // 选为 ambient_light_provider）⇒ 句柄错位这种静默发黑事故只有这条判据有分辨力。
        // 判据只建在**冻结相机 + 定帧**上；常量由实跑记录，改渲染结果（少画/错画/挪相机）必须复核重录。
        static const bool s_selfcheck = (getenv("IBLENV_SELFCHECK") != nullptr);
        static uint32_t   s_self_frame = 0;

        static constexpr uint32_t kSelfW           = 1280;
        static constexpr uint32_t kSelfH           = 720;
        static constexpr uint32_t kSelfMinDistinct = 256;   // 不空转守卫（恒定图会 FAIL）

        // 三条技术档位各自的基线（同一场景 / 同一组材质参数 / 同一套 LDR 曝光调制）。
        // 八面体版是另一张图、另一套采样 ⇒ 摘要必然不同，但亮度应与 Cube 几乎同量级
        // （同一批烘焙数据的两种投影，重采样只挪动高光分布，实测 mean_luma 同为 0.6929）；
        // SH9 档换了整套算法（低频解析）且**没有视觉天空**（`SHLightOnly`，背景=清屏色），
        // 亮度只剩物体间接光 + 直接光贡献。三侧都钉住，
        // 「provider 接错 / 句柄错位 / 采样规则坏掉」在任一档位下都会翻红。
        //
        // 2026-10（阶段 4）：改用 `SetupSkyEnvironment()` 托管后四条基线整体重录过一次。
        // 唯一改变像素的原因是**天空实体改为跟随主相机**（`track_camera`，半径 256 的
        // 半球球心从世界原点挪到相机位置，背景产生 ~6% 的视差位移）；把该开关临时关掉
        // 重跑时 Cube 摘要与改造前**逐字节一致**（0x54295E44E3C1677C）⇒ 托管路径与手工
        // 装配等价。
        //
        // 2026-11（todo-10 资产化）：Cube / Octahedral 切到 `env:@Sky34` 的
        // `[cubemap]` / `[octahedral]` 段。装配语义不变（运行日志仍报
        // `recipe=IBLEnvironment.Sky / provider=ambient_light_ibl, ldr=1, mips=8`，
        // oct 侧 `recipe=IBLEnvironmentSkyOct.Sky / provider=ambient_light_ibl_oct`），
        // 数值相对 2026-10 有 ~0.2% 漂移（0.6941 → 0.6929），来源是阶段 4 之后的
        // 无扩展名 URI 探测 / 前缀推导投影等管线改动，与资产化本身无关；按实跑重录。
        // SH9 档改为 `SkyTechnique::SH9` 后**没有视觉天空**（`SHLightOnly`，背景=
        // 清屏色，物体只剩间接光 + 直接光），亮度降到 0.1282，按实跑重录。
        // 原第四条 `IBLENV_SKY_OCT`（只换背景投影）随资产配对退役，基线删除。
        static constexpr uint64_t kSelfFNV_Cube       = 0x75A21A3F7828F7FAull;  // CubeMap（Debug；mean_luma 0.6929）
        static constexpr uint64_t kSelfFNV_Octahedral = 0x5A9C4E86A8FBE417ull;  // 八面体（Debug；mean_luma 0.6929）
        static constexpr uint64_t kSelfFNV_SH         = 0xEEB9434DC80DD0CBull;  // SH9 解析合成（Debug；mean_luma 0.1282，无背景天空）

        if (s_selfcheck && ++s_self_frame == 8)
        {
            auto * rt = dynamic_cast<graph::SwapchainRenderTarget *>( ecs_context ? ecs_context->GetRenderTarget() : nullptr );
            if (!rt)
            {
                printf("[IBLSELF] no swapchain RT\n");
                std::exit(2);
            }

            const SkyTechnique technique = AmbientTechnique();

            ulre_example::FrameDigest expect;
            expect.width                = kSelfW;
            expect.height               = kSelfH;
            expect.distinct_byte_values = kSelfMinDistinct;
            expect.fnv1a64              = technique == SkyTechnique::SH9        ? kSelfFNV_SH
                                        : technique == SkyTechnique::Octahedral ? kSelfFNV_Octahedral
                                                                                : kSelfFNV_Cube;

            ulre_example::InstallFrameDigestHook(rt, expect, "IBLSELF");
        }
    }

    ~IBLEnvironmentApp()
    {
        // 共享顶点池的几何由 GeometryAssetRegistry 持有：先销毁几何，再销毁顶点池
        if (auto *registry = GetGeometryAssetRegistry())
            registry->Clear();

        SAFE_CLEAR(mesh_vdm)
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.10f, 0.12f, 0.16f, 1.0f));

        if (!InitMaterial())
            return false;

        if (!InitScene())
            return false;

        if (!InitCamera())
            return false;

        return true;
    }
};

int os_main(int argc, os_char** argv)
{
    return RunFramework<IBLEnvironmentApp>(OS_TEXT("IBL Environment"), argc, argv, 1280, 720);
}
