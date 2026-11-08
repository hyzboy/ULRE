#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/mtl/MaterialResolver.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/ssbo/GlobalSSBOTypes.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/graph/mesh/StaticMesh.h>
#include<hgl/graph/mesh/LoadStaticMesh.h>
#include<hgl/graph/asset/StaticMeshModelInstance.h>
#include<hgl/graph/asset/AssetSourceMount.h>
#include<hgl/graph/asset/ModelAsset.h>
#include<hgl/ecs/systems/render/streaming/ModelStreamingSystem.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/color/Color.h>
#include<hgl/type/StdString.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKTextureReadback.h>
#include "../../Shared/FrameDigest.h"   // 帧摘要自检（A6-d 判据重建共用件）
#include<hgl/ecs/support/cull/GPUCullSystem.h>
#include<cstring>
#include<cstdlib>
#include<cstdio>
#include<cmath>
#include<filesystem>

// ── 台架门控（默认全关） ────────────────────────────────────────────
//   LS_SCENE=<path>  覆盖 .model 资产路径（默认 res/ABeautifulGame.StaticMesh/…；
//                    含 meshlets 表的资产已**不支持** ⇒ 加载即显式拒绝）
//   LS_DIST/LS_YAW/LS_PITCH  相机覆盖
//   LS_FRAMES=N      跑 N 帧退出
//   LS_DUMP=<tag>    第 8 帧帧内回读落原始字节后退出（lsdump_<tag>_WxH_fmt.raw）
//   LS_DUMP_FRAME=N  改回读帧号（默认 8）
//   LS_SYNC=1        走同步装载通路（默认异步流送）
//   LS_TRACE=1       每 20 帧打印场景句柄的装配计数（确认流送收敛，不卡在 Assembling）

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/support/transform/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/ModelRefComponent.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/light_shadow/EnvironmentSystem.h>
#include<hgl/ecs/systems/render/light_shadow/LightSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/quaternion.hpp>
#include<glm/gtx/quaternion.hpp>

#include<memory>
#include<string>
#include<vector>

using namespace hgl;
using namespace hgl::graph;

namespace
{
    GeometryVertexFormat CreateStandardGeometryVertexFormat(VkFormat normal_format = VF_V2UN8)
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2HF},   // UV RG16F（half×2——4B/顶点）
            {VertexSemantic::Normal,   normal_format}, // 默认 VF_V2UN8，支持 VF_V2HF
        };
        return gvf;
    }
}

// 颜色数值已搬到数据块文件；这里只保留行数（门禁测试会核对文件行数与它一致）
constexpr const size_t COLOR_COUNT = 8;

namespace
{
    // 材质数据行的唯一来源：一个 URI + 行号，材质数值全部在块文件里
    constexpr char MATERIAL_DATA_URI[] =
        "mtldata:pack(res):ABeautifulGame.StaticMesh/load_model.material_data.toml";
    constexpr uint32_t MATERIAL_DATA_ROW = 0;

    // PBR 自装配：scene/recipe/material_data/tex2d 全部取自 pack 同目录；
    // 目录里有两份数据块（debug/PBR），用块名 hint 选 PBR 那份。
    const std::string &ModelPackPath()
    {
        static const std::string path = []
        {
            const char *scene_env = getenv("LS_SCENE");
            return std::string(scene_env ? scene_env
                                         : "pack(res):ABeautifulGame.StaticMesh/ABeautifulGame.model");
        }();
        return path;
    }

    // 请求串可以是裸路径，也可以是包裹 URI（`pack(res):逻辑路径`）。
    // 统一经挂载索引反查转物理路径：裸路径顺带完成存在性检查，
    // 包裹路径转译不到（未挂载 / 索引缺失）返回 false。
    static bool LocateScenePhysical(const std::string &request, std::string &out_physical)
    {
        hgl::OSString resolved;
        if (!hgl::graph::ResolveAssetPhysicalPath(hgl::ToOSString(request), resolved))
            return false;
        out_physical = hgl::ToStdString(resolved);
        return !out_physical.empty();
    }

    // 天光 IBL：复用 IBLEnvironment 示例的同一批烘焙贴图与天空配方
    constexpr os_char IBL_TEX_IRR_CUBE[] = OS_TEXT("pack(res):image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_irradiance.TexCube");
    constexpr os_char IBL_TEX_PRE_CUBE[] = OS_TEXT("pack(res):image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_prefilter.TexCube");
}

class LoadModelApp:public WorkObject
{
private:

    hgl::ecs::ECSContext *ecs_context = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;

    std::string dump_tag;
    uint32_t    ls_frames      = 0;
    uint32_t    ls_frame_seen  = 0;
    uint32_t    dump_frame     = 0;

    struct MaterialData
    {
        GeometryVertexFormat geometry_vertex_format;
        // 调试法线（EmissiveSurface，8 行）；PBR 行走自装配（chess_materials_）
        graph::GlobalSSBOBinding debug_rows[COLOR_COUNT]{};
    };

    MaterialData solid;
    graph::mtl::MaterialRecipe scene_recipe{};
    // PBR 模式：按 material_index 索引的自装配材质集
    SelfContainedModelMaterials chess_materials_;
    bool pbr_mode_ = true;

    // ── 流送（异步）通路（默认；LS_SYNC=1 回到同步）────────────────────
    // 同步通路（TryLoadModel）把「读盘 ⇒ 解析 ⇒ 建 GPU 资源 ⇒ 实例化」全挤在
    // Init() 里，启动一帧卡住。异步通路是**声明式**的：挂一个
    // ModelRefComponent，请求与三段渐进实例化（骨架 ⇒ 几何 ⇒ 材质）全部由
    // ModelStreamingSystem 的装配段推进，本类只在 AsyncModelPolicy 回调里
    // 做相机对焦与日志。两条通路最终落到同一个 StaticMeshModelInstance
    // 的三段 API，画面应当一致。
    bool async_mode_ = false;
    std::shared_ptr<hgl::ecs::ModelStreamingSystem> streaming_system_;

    ///< 组件行（归世界；仅供 LS_TRACE 观察句柄状态；实体随世界销毁）
    hgl::ecs::ModelRefComponent *async_ref_ = nullptr;

    class AsyncModelPolicy;                  ///< 嵌套策略类；定义在本类体内后段
    AsyncModelPolicy *async_policy_ = nullptr;

    std::shared_ptr<hgl::ecs::EnvironmentSystem> environment_system;
    std::shared_ptr<hgl::ecs::LightSystem>       light_system;

    std::vector<PrimitiveAsset>    scene_assets_;
    std::vector<StaticMeshNode>    scene_nodes_;
    std::vector<int32_t>           scene_root_nodes_;
    // 场景世界空间包围球（节点 AABB 角点变换后的并集）：相机自动取景用，
    // 距离 = 2×半径，目标 = 中心；半径 <0 表示未求得（回退旧默认值）。
    math::Vector3f                 scene_bound_center_ { 0.0f, 0.0f, 0.0f };
    float                          scene_bound_radius_ = -1.0f;
    // 场景 → ECS 标准实例化器：逐节点局部 TRS + SetParent 还原层级，
    // 自持资产副本（不再需要 DecomposeTransform 摊平，也不惧 vector 搬迁悬垂）。
    StaticMeshModelInstance        model_instance_;

private:

    bool InitMaterialRows(const char *uri, uint32_t count, graph::GlobalSSBOBinding *rows)
    {
        if (!uri || !rows || count == 0)
            return false;

        graph::GlobalSSBOBinding first{};
        if (!GetMaterialDataBinding(uri, MATERIAL_DATA_ROW, first))
            return false;

        // 同一数据块内的相邻行：基行 + 序号，不必逐行查 URI
        for (uint32_t i = 0; i < count; ++i)
            rows[i] = first.RowOffset(i);

        return true;
    }

    bool InitSolidMDP()
    {
        solid.geometry_vertex_format = CreateStandardGeometryVertexFormat(VF_V2UN8);
        if (solid.geometry_vertex_format.GetCount() == 0)
            return false;

        if (!InitMaterialRows(MATERIAL_DATA_URI, static_cast<uint32_t>(COLOR_COUNT), solid.debug_rows))
            return false;

        return true;
    }

    // 场景配方表：PBR 自装配 15 个棋件配方，或调试法线单配方。
    // 仅供**同步**装载通路使用；异步通路由系统扇出 + 自装配，启动段零扫盘。
    bool BuildSceneRecipes(const mtl::MaterialRecipe *&out_default,
                           const mtl::MaterialRecipe *const *&out_table,
                           uint32_t &out_table_count)
    {
        out_default     = nullptr;
        out_table       = nullptr;
        out_table_count = 0;

        if (pbr_mode_)
        {
            auto *graphics = GetGraphicsContext();
            if (!graphics)
                return false;

            if (!BuildSelfContainedModelMaterials(
                    *graphics, hgl::ToOSString(ModelPackPath()),
                    "ABeautifulGame", chess_materials_))
                return false;

            out_default     = chess_materials_.fallback;
            out_table       = chess_materials_.table.data();
            out_table_count = static_cast<uint32_t>(chess_materials_.table.size());
            return true;
        }

        const mtl::MaterialRecipe *loaded_recipe = GetRecipe("LoadModel.DebugNormalColor");
        if (!loaded_recipe)
            return false;

        scene_recipe = *loaded_recipe;
        if (!(scene_recipe.material_ssbo_binding = solid.debug_rows[0]).IsValid())
            return false;

        out_default = &scene_recipe;
        return true;
    }

    bool TryLoadModel()
    {
        using std::filesystem::path;
        using std::filesystem::exists;

        // 资产路径可覆盖（默认不变）；包裹 URI 先转译物理路径。
        std::string physical;
        if (!LocateScenePhysical(ModelPackPath(), physical))
            return false;

        const path scene_path(physical);
        const path scene_dir = scene_path.parent_path();

        auto *device    = GetDevice();
        auto *geo_mgr   = GetManager<GeometryManager>();
        if (!device || !geo_mgr) return false;

        const OSString pack_path = hgl::ToOSString(scene_path.string());
        const OSString base_dir  = hgl::ToOSString(scene_dir.string());

        const mtl::MaterialRecipe *default_recipe = nullptr;
        const mtl::MaterialRecipe *const *recipe_table = nullptr;
        uint32_t recipe_table_count = 0;

        if (!BuildSceneRecipes(default_recipe, recipe_table, recipe_table_count))
            return false;

        return LoadModelAsPrimitiveAssets(
            device,
            geo_mgr,
            solid.geometry_vertex_format,
            default_recipe,
            pack_path,
            base_dir,
            scene_assets_,
            scene_nodes_,
            scene_root_nodes_,
            recipe_table,
            recipe_table_count);
    }

    /**
     * @brief 异步通路的发起段：配外部设定，然后挂 `ModelRefComponent` 声明请求。
     *
     * 之后所有推进都由 `ModelStreamingSystem`（已注册进 ECS）的装配段每帧自己做；
     * 本类只在 `AsyncModelPolicy` 回调里做对焦与日志。
     */
    bool TryStartAsyncModel()
    {
        using std::filesystem::path;
        using std::filesystem::exists;

        // 请求串保留原文（系统内部自行转译）；存在性检查与目录口径用转译后的物理路径。
        std::string physical;
        if (!LocateScenePhysical(ModelPackPath(), physical))
            return false;

        const OSString base_dir = hgl::ToOSString(path(physical).parent_path().string());

        const mtl::MaterialRecipe *default_recipe = nullptr;
        const mtl::MaterialRecipe *const *recipe_table = nullptr;
        uint32_t recipe_table_count = 0;
        bool auto_scene_materials   = false;

        if (pbr_mode_)
        {
            // 配方/数据块全部交给系统：扇出给材质车道，门禁放行后自装配。
            // 启动段不再同步扫盘 —— 这正是异步通路存在的意义。
            auto_scene_materials = true;
        }
        else
        {
            // 调试法线模式：单配方覆盖全部材质，不能被目录里的 PBR 配方抢走。
            const mtl::MaterialRecipe *loaded_recipe = GetRecipe("LoadModel.DebugNormalColor");
            if (!loaded_recipe)
                return false;

            scene_recipe = *loaded_recipe;
            if (!(scene_recipe.material_ssbo_binding = solid.debug_rows[0]).IsValid())
                return false;

            default_recipe = &scene_recipe;
        }

        streaming_system_ = ecs_context->GetSystem<hgl::ecs::ModelStreamingSystem>();
        if (!streaming_system_)
            streaming_system_ = ecs_context->RegisterSystem<hgl::ecs::ModelStreamingSystem>();
        if (!streaming_system_)
            return false;

        streaming_system_->SetBuildSettings(solid.geometry_vertex_format,
                                            default_recipe,
                                            recipe_table,
                                            recipe_table_count,
                                            base_dir,
                                            auto_scene_materials);

        // 声明式请求：挂组件即完事；三段推进由 ModelStreamingSystem 装配段负责
        auto *ref_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("AsyncModelRoot");
        if (!ref_entity)
            return false;

        async_ref_ = ref_entity->AddComponent<hgl::ecs::ModelRefComponent>();
        if (!async_ref_)
            return false;

        async_ref_->uri = AnsiString(ModelPackPath().c_str());
        async_ref_->skeleton_options.mobility = hgl::ecs::Mobility::Static;
        BuildInstanceOptions(async_ref_->options);

        if (!async_policy_)
            async_policy_ = new AsyncModelPolicy(this);
        async_ref_->policy = async_policy_;

        printf("[LSASYNC] request queued: %s\n", ModelPackPath().c_str());
        return true;
    }

    void BuildInstanceOptions(StaticMeshModelInstance::Options &options) const
    {
        options = StaticMeshModelInstance::Options{};
        options.mobility = hgl::ecs::Mobility::Static;

        if (pbr_mode_)
        {
            // 行序即 glTF 材质下标 ⇒ 按 material_index 选行，不再轮转。
            // 异步通路这里留空：由系统在 attach 前用自装配的 `stream->mats` 补上。
            if (!chess_materials_.bindings.empty())
            {
                options.material_rows      = chess_materials_.bindings.data();
                options.material_row_count =
                    static_cast<uint32_t>(chess_materials_.bindings.size());
                options.rows_by_material_index = true;
            }
        }
        else
        {
            options.material_rows      = solid.debug_rows;
            options.material_row_count = static_cast<uint32_t>(COLOR_COUNT);
        }
    }

    bool InitScene()
    {
        if(!ecs_context)
            return false;

        if (scene_nodes_.empty())
            return false;

        // 纯 TRS 直通：节点局部 TRS 取自 .model 的 TRSTable 真源，父子层级按
        // parentIndex 用 SetParent 还原 —— 不再对 worldMatrix 做 DecomposeTransform
        // 摊平（旧写法在非均匀缩放嵌套下有旋转/缩放残差，且丢失场景树）。
        StaticMeshModelInstance::Options options;
        BuildInstanceOptions(options);

        if (!model_instance_.Instantiate(ecs_context, scene_assets_, scene_nodes_,
                                         scene_root_nodes_, options))
            return false;

        ComputeSceneBounds();

        // 实例已自持资产副本 ⇒ 装载缓冲即刻释放，杜绝误用与驻留
        scene_assets_.clear();
        scene_nodes_.clear();
        scene_root_nodes_.clear();
        return true;
    }

    // 遍历节点：局部 AABB 8 角点经 worldMatrix 变换求并集 ⇒ 世界空间中心+外接球半径
    void ComputeSceneBounds()
    {
        float mn[3] = {  1e30f,  1e30f,  1e30f };
        float mx[3] = { -1e30f, -1e30f, -1e30f };
        bool  any   = false;

        for (const auto &n : scene_nodes_)
        {
            if (!n.boundsValid)
                continue;

            const auto &c = n.nodeBounds.aabb.GetCenter();
            const auto &l = n.nodeBounds.aabb.GetLength();

            for (int i = 0; i < 8; ++i)
            {
                const math::Vector3f corner(
                    c.x + ((i & 1) ? 0.5f * l.x : -0.5f * l.x),
                    c.y + ((i & 2) ? 0.5f * l.y : -0.5f * l.y),
                    c.z + ((i & 4) ? 0.5f * l.z : -0.5f * l.z));

                const math::Vector3f w = math::TransformPosition(n.worldMatrix, corner);

                const float p[3] = { w.x, w.y, w.z };
                for (int a = 0; a < 3; ++a)
                {
                    if (p[a] < mn[a]) mn[a] = p[a];
                    if (p[a] > mx[a]) mx[a] = p[a];
                }
                any = true;
            }
        }

        if (!any)
            return;

        scene_bound_center_ = math::Vector3f((mn[0]+mx[0])*0.5f, (mn[1]+mx[1])*0.5f, (mn[2]+mx[2])*0.5f);
        const float dx = mx[0]-mn[0], dy = mx[1]-mn[1], dz = mx[2]-mn[2];
        scene_bound_radius_ = 0.5f * std::sqrt(dx*dx + dy*dy + dz*dz);
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("MainCamera");
        // 相机 = **世界级资源**：经世界访问器创建/取回（实体只是宿主），不直取组件
        auto *camera = ecs_context->GetOrCreateCamera(camera_entity);

        camera->control_mode = hgl::ecs::CameraControlMode::ViewModel;
        // 自动取景：目标 = 模型包围球中心，距离 = 2×半径（求得时）；否则旧默认
        camera->target = scene_bound_radius_ > 0.0f
            ? scene_bound_center_ : math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = scene_bound_radius_ > 0.0f
            ? 2.0f * scene_bound_radius_ : 8.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        if (const char * v = getenv("LS_DIST"))  camera->distance = (float)atof(v);
        if (const char * v = getenv("LS_YAW"))   camera->yaw      = (float)atof(v);
        if (const char * v = getenv("LS_PITCH")) camera->pitch    = (float)atof(v);

        camera->viewport_info = GetViewportInfo();

        return true;
    }

    /// 骨架到位后按包围盒重新取景（环境变量覆盖优先，与 InitCamera 一致）
    void FocusCameraOnSkeleton()
    {
        if (scene_bound_radius_ <= 0.0f || !camera_entity || !ecs_context)
            return;

        auto *camera = ecs_context->GetOrCreateCamera(camera_entity);

        if (!camera)
            return;

        camera->target   = scene_bound_center_;
        camera->distance = 2.0f * scene_bound_radius_;

        if (const char * v = getenv("LS_DIST"))  camera->distance = (float)atof(v);
        if (const char * v = getenv("LS_YAW"))   camera->yaw      = (float)atof(v);
        if (const char * v = getenv("LS_PITCH")) camera->pitch    = (float)atof(v);

        camera->matrix_dirty = true;
    }

    // ── AsyncModelPolicy：三段回调只做对焦与日志 ──────────────────────────
    // 嵌套类在外侧类成员函数体之前定义 ⇒ `new AsyncModelPolicy(this)` 处类型已完整；
    // 体内可自由访问 LoadModelApp 全部私有成员（嵌套类拥有外侧访问权）。
    class AsyncModelPolicy final : public hgl::ecs::ModelLoadPolicy
    {
    private:
        LoadModelApp *app_;

    public:
        explicit AsyncModelPolicy(LoadModelApp *owner) : app_(owner) {}

        ~AsyncModelPolicy() override = default;

        void OnSkeletonReady(hgl::ecs::ModelRefComponent &ref) override
        {
            int node_count = 0, root_count = 0;
            if (ref.asset)
            {
                const graph::ModelAssetContent &content = ref.asset->GetContent();
                node_count = content.GetNodeCount();
                root_count = content.GetRootCount();
            }

            printf("[LSASYNC] skeleton: %d nodes / %d roots (frame %u)\n",
                   node_count, root_count, app_->ls_frame_seen);

            // 骨架包围盒 ⇒ 相机立刻对焦（此时几何可能还在路上）
            math::BoundingVolumes bounds;
            if (ref.instance.GetSkeletonBounds(bounds))
            {
                app_->scene_bound_center_ = bounds.GetCenter();
                app_->scene_bound_radius_ = bounds.GetMaxRadius();
                app_->FocusCameraOnSkeleton();
            }
        }

        void OnGeometryAttached(hgl::ecs::ModelRefComponent &ref) override
        {
            printf("[LSASYNC] geometries: %d primitives (frame %u)\n",
                   ref.instance.GetAttachedPrimitiveCount(), app_->ls_frame_seen);
        }

        void OnMaterialAttached(hgl::ecs::ModelRefComponent &ref) override
        {
            printf("[LSASYNC] materials attached (frame %u), scene state=%s\n",
                   app_->ls_frame_seen, ref.asset ? ref.asset->GetStateName() : "?");
        }

        // 场景状态机的 Ready 由「全部图元几何就绪」推进（见 ModelStreamingSystem 阶段 C）：
        // 它晚于材质绑定，因此单独报一次，用来确认流送确实收敛而不是永远 Assembling。
        void OnModelReady(hgl::ecs::ModelRefComponent & /*ref*/) override
        {
            printf("[LSASYNC] scene ready (frame %u)\n", app_->ls_frame_seen);
        }

        void OnModelFailed(hgl::ecs::ModelRefComponent & /*ref*/, const char *reason) override
        {
            printf("[LSASYNC] model failed: %s\n", reason);
        }
    };//class AsyncModelPolicy

    bool InitECS()
    {
        ecs_context = GetECSContext();

        if(!ecs_context)
            return false;

        // 异步通路的实例化由 ModelStreamingSystem 装配段逐帧推进，这里不能同步实例化
        if(!async_mode_ && !InitScene())
            return false;

        if(!InitCamera())
            return false;

        return true;
    }

public:
    ~LoadModelApp()
    {
        // 策略对象归本类；组件行归世界。先解引用再释放，遵守 ModelLoadPolicy 生命期契约。
        if (async_ref_)
            async_ref_->policy = nullptr;
        delete async_policy_;
        async_policy_ = nullptr;
        async_ref_ = nullptr;

        model_instance_.Release();
        scene_assets_.clear();
        scene_nodes_.clear();
        scene_root_nodes_.clear();
    }

    bool InitCull()
    {
        // GPU 剔除 opt-in（默认全关 = 纯 CPU 账基线）
        auto gpu_cull = ecs_context ? ecs_context->GetSystem<hgl::ecs::GPUCullSystem>() : nullptr;
        if (!gpu_cull)
        {
            printf("[ERROR] InitCull: GPUCullSystem 未注册\n");
            return false;
        }

        return true;
    }

    // PBR 模式的环境光照：主光方向 + 天光 IBL（复用 IBLEnvironment 的烘焙贴图与天空配方）
    bool InitEnvironmentControl()
    {
        if (!ecs_context)
            return false;

        environment_system = ecs_context->GetSystem<hgl::ecs::EnvironmentSystem>();
        if (!environment_system)
            environment_system = ecs_context->RegisterSystem<hgl::ecs::EnvironmentSystem>();
        if (!environment_system)
            return false;

        light_system = ecs_context->GetSystem<hgl::ecs::LightSystem>();
        if (!light_system)
            light_system = ecs_context->RegisterSystem<hgl::ecs::LightSystem>();
        if (!light_system)
            return false;

        // 太阳方向必须在天空托管**之前**写进光源行 0：天光调制按当时太阳现算
        light_system->SetMainLightDirection(math::Vector3f(0.2f, 0.7f, 0.68f));

        hgl::ecs::SkyEnvironmentConfig config;
        config.mode         = hgl::ecs::SkyMode::CubeMapIBL;
        config.sky_recipe   = "IBLEnvironment.Sky";
        config.sky_scale    = 1.0f;             // 全天球：包住场景即可
        config.track_camera = true;             // 天空实体逐帧跟随主相机（无视差）

        config.irradiance_texture = IBL_TEX_IRR_CUBE;
        config.prefilter_texture  = IBL_TEX_PRE_CUBE;
        // provider 留空 ⇒ 按 prefilter 后缀自动选择（.TexCube → ambient_light_ibl）

        // 这批烘焙图是 LDR 量化版：显式开启曝光调制（与 IBLEnvironment 同口径）
        config.ldr               = hgl::ecs::SkyAmbientLDR::On;
        config.ldr_exposure_base = 2.5f;
        config.ldr_sun_scale     = 1.0f;
        config.ldr_min_ambient   = 0.05f;

        return environment_system->SetupSkyEnvironment(config);
    }

    bool Init() override
    {
        // 默认走 PBR 材质路径；LS_DEBUGNORMAL=1 回到旧的调试法线/颜色行路径
        pbr_mode_ = (getenv("LS_DEBUGNORMAL") == nullptr);

        // 默认走流送（异步）通路；LS_SYNC=1 回到同步装载
        //（两条通路最终落到同一个 StaticMeshModelInstance，第 40 帧画面逐字节相同）
        async_mode_ = (getenv("LS_SYNC") == nullptr);

        ecs_context = GetECSContext();

        if(!ecs_context)
            return(false);

        if(!InitSolidMDP())
            return(false);

        if (async_mode_ ? !TryStartAsyncModel() : !TryLoadModel())
            return(false);

        if(!InitECS())
            return(false);

        if(pbr_mode_ && !InitEnvironmentControl())
            return(false);

        if(!InitCull())
            return(false);

        if (const char * tag = getenv("LS_DUMP"))
            dump_tag = tag;

        if (const char * v = getenv("LS_FRAMES"))
            ls_frames = (uint32_t)atoi(v);

        return(true);
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);

        ++ls_frame_seen;

        // LS_TRACE：借组件观察句柄打印装配计数（异步通路专属；装配本身在系统⑦段）
        if (async_mode_ && getenv("LS_TRACE") && (ls_frame_seen % 20) == 0)
        {
            if (graph::ModelAsset *scene = async_ref_ ? async_ref_->asset : nullptr)
                printf("[LSASYNC-DBG] frame %u state=%s ready=%d failed=%d pending=%d\n",
                       ls_frame_seen, scene->GetStateName(),
                       scene->GetReadyChildCount(),
                       scene->GetFailedChildCount(),
                       scene->GetPendingChildCount());
        }

        // LS_DUMP=<tag>：第 8 帧装一次性帧内回读钩子（本帧提交后、present 前执行 ⇒ 读到本帧画面），
        // 落原始 packed 字节后退出 —— 像素判据用。
        // LSSELFCHECK=1：**帧摘要自检**（A6-d 判据重建；共用件 example/Shared/FrameDigest.h）——
        // 第 8 帧帧内回读颜色靶 ⇒ {不同字节取值数, FNV-1a 64} 比对下方内联常量：PASS 退出 0 / FAIL 退出 1。
        // 判据只建在**冻结相机 + 定帧**上；常量由实跑记录，改渲染结果（少画/错画/挪相机）必须复核重录。
        {
            // 异步通路的产物到位帧不确定 ⇒ 定帧判据不成立，只报一句而不误报 FAIL
            static const bool s_selfcheck = (getenv("LS_SELFCHECK") != nullptr && getenv("LS_SYNC") != nullptr);
            static uint32_t   s_self_frame = 0;

            static constexpr uint32_t kSelfW           = 1280;
            static constexpr uint32_t kSelfH           = 720;
            static constexpr uint32_t kSelfMinDistinct = 2;   // 不空转守卫（恒定图会 FAIL）
            static constexpr uint64_t kSelfFNVDebug    = 0xA99178FD2FA1B0C6ull;  // 调试法线路径（LS_DEBUGNORMAL=1；2026-11-07 重录：res 修复+闭环后，画面为棋盘地面+棋子法线着色，背景暗）
            static constexpr uint64_t kSelfFNVPBR      = 0x35F0E9FD935DA9ADull;  // PBR 路径（落地闭环后重录：
                                                        // 自动装配/车道优先/包裹寻址合入后的定帧画面）
                                                        // （改渲染结果必须复核并重录）

            if (s_selfcheck && ++s_self_frame == 8)
            {
                auto * rt = dynamic_cast<graph::SwapchainRenderTarget *>( ecs_context ? ecs_context->GetRenderTarget() : nullptr);
                if (!rt)
                {
                    printf("[LSSELF] no swapchain RT\n");
                    std::exit(2);
                }

                ulre_example::FrameDigest expect;
                expect.width                = kSelfW;
                expect.height               = kSelfH;
                expect.distinct_byte_values = kSelfMinDistinct;
                expect.fnv1a64              = pbr_mode_ ? kSelfFNVPBR : kSelfFNVDebug;

                ulre_example::InstallFrameDigestHook(rt, expect, "LSSELF");
            }
        }


        static const uint32_t s_target_dump_frame = []() {
            if (const char *f = getenv("LS_DUMP_FRAME"))
                return (uint32_t)atoi(f);
            return 8u;
        }();

        if (!dump_tag.empty() && ++dump_frame == s_target_dump_frame)
        {
            auto * rt = dynamic_cast<graph::SwapchainRenderTarget *>(
                ecs_context ? ecs_context->GetRenderTarget() : nullptr);
            if (!rt)
            {
                printf("[LSDUMP] no swapchain RT\n");
                std::exit(2);
            }

            std::string tag = dump_tag;
            graph::IRenderTarget * rt_ptr = rt;
            rt->SetInFrameReadbackHook([tag, rt_ptr]()
            {
                std::vector<uint8_t> pixels;
                graph::TextureReadbackInfo info;
                if (!graph::ReadbackColorTarget(rt_ptr, pixels, 0, &info) || pixels.empty())
                {
                    printf("[LSDUMP] readback failed\n");
                    std::exit(2);
                }

                char name[256];
                snprintf(name, sizeof(name), "lsdump_%s_%ux%u_%s.raw",
                         tag.c_str(), info.width, info.height,
                         GetVulkanFormat(info.format) ? GetVulkanFormat(info.format)->name : "unknown");
                FILE * fp = fopen(name, "wb");
                if (!fp)
                {
                    printf("[LSDUMP] open %s failed\n", name);
                    std::exit(2);
                }
                fwrite(pixels.data(), 1, pixels.size(), fp);
                fclose(fp);
                printf("[LSDUMP] wrote %s (%zu bytes)\n", name, pixels.size());
                std::exit(0);
            });
        }

        if (ls_frames > 0 && ls_frame_seen >= ls_frames)
        {
            printf("[LoadModel] done frames=%u\n", ls_frame_seen);
            fflush(stdout);
            std::exit(0);
        }
    }
};//class LoadModelApp

int os_main(int argc,os_char **argv)
{
    return RunFramework<LoadModelApp>(OS_TEXT("Load Model"),argc,argv,1280,720);
}
