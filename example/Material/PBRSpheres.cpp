// 该范例演示 10x10 的 Standard 网格：
// 使用 baseColor + normal 纹理（Albedo+Normal）
// 通过 metallic/roughness 参数渐变来控制材质
// This example renders a 10x10 Standard grid:
// Uses baseColor + normal textures (Albedo+Normal only)
// Metallic/roughness gradients control material properties.

#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/asset/GeometryPreset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/MaterialResolver.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/graph/asset/MaterialDataBlockAsset.h>

#include<hgl/filesystem/Filename.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/color/ColorPacking.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/support/transform/TransformAccessor.h>
#include<hgl/ecs/support/pipeline/RenderableBuilder.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/ShadowProxy.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/render/buffer/RenderSceneUBOSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/support/cull/GPUCullSystem.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/vk/VKTextureReadback.h>
#include "../Shared/FrameDigest.h"   // 帧摘要自检（A6-d 判据重建共用件）
#include <cstdio>
#include <cstdlib>
#include <hgl/time/Time.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<memory>
#include<cstring>
#include<string>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    GeometryVertexFormat CreateStandardTextureArrayGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2F},
            {VertexSemantic::Normal,   VF_V3F},
        };
        return gvf;
    }
}

// 10×10 grid params
static constexpr uint GRID_SIZE       = 10;    // spheres per axis
static constexpr float SPHERE_SPACING = 2.5f;  // center-to-center distance

// 10 built-in geometries, one per column
static constexpr uint GEOMETRY_VARIANT_COUNT = GRID_SIZE;

// 10 种 PBR 材质的贴图顺序（层号 → 文件夹）搬到了 texarray 规约里：
//   ShaderLibrary/asset/texarray/pbr_10x10_baseColor.texarray.toml
//   ShaderLibrary/asset/texarray/pbr_10x10_normal.texarray.toml
// 与本示例共用同一份规约的还有 CascadeShadowMap（文件夹顺序完全一致）⇒ 同一张数组
// 纹理，不重复上传。原来这里的 `PBR_FOLDER_NAME[10]` 常量表已删除。
//
// 两张数组纹理现在**只**在配方文件里出现（`[[textures]] asset = "texarray:@..."`），
// 示例代码不再持有 URI 常量、不再 CreateTextureAsset。

// 批量材质数据行的**唯一**来源：文件化的数据块（100 行 PBRSurface，行优先）。
// 原来这里是一个 CreateLitMaterialData() 双循环硬编码（含统一底色 0.72 灰），现在
// 改成一份可被离线工具读写、也可被别的示例复用的资产文件 —— 示例代码里不再出现
// 任何逐行参数。
static constexpr const char * PBR_GRID_DATA_PATH =
    "ShaderLibrary/asset/material_data/pbr_10x10_grid.material_data.toml";

class PBRSpheresApp : public WorkObject
{
private:

    ECSContext *  ecs_world     = nullptr;
    Entity *      camera_entity = nullptr;

    std::string   dump_tag;               ///< PBR_DUMP=<tag>：像素验收模式（t 冻结 + 帧 8 回读）
    uint32_t      dump_frame = 0;
    uint32_t      perf_frames = 0;        ///< PBR_FRAMES=N：定帧退出；0 = 未启用
    uint32_t      perf_frame_seen = 0;    ///< 已计帧数（240 帧一窗打印）
    double        perf_total_us = 0.0;    ///< Tick 全帧 CPU 时间累计（每 240 帧一窗打印）

    graph::mtl::MaterialRecipe sphere_recipe{};

    // T3i（③ 多批/混材质）判据（PBR_MULTIBATCH=1）：网格后半（col >= GRID_SIZE/2）
    // 走**第二套配方**——同 mtl_def + 不同顶点节点（billboard 朝向）⇒ 不同 vertex
    // program ⇒ 两个 MaterialBatch ⇒ 走"单次 cull 派发 + 批标签/切片"的多批路径。
    //（改 pipeline 配置不成批：EDS 后 Pipeline 只留 shader 部分，见 InitMaterial 注释。）
    graph::mtl::MaterialRecipe sphere_recipe_b{};
    PrimitiveAsset base_primitives_b[GRID_SIZE]{};
    bool multibatch_enabled = false;

    VertexDataManager * mesh_vdm = nullptr;
    Geometry *          builtin_geometries[GEOMETRY_VARIANT_COUNT]{};
    PrimitiveAsset      base_primitives[GEOMETRY_VARIANT_COUNT]{};

    /// 网格全部 100 个球的材质数据都来自这一块（col→metallic，row→roughness）
    graph::mtl::MaterialDataBlockAsset * sphere_data_block = nullptr;

    // 100 entities, one per sphere
    Entity *sphere_entities[GRID_SIZE][GRID_SIZE]{};
    hgl::ecs::TransformAccessor sphere_transforms[GRID_SIZE][GRID_SIZE]{};

    double elapsed_time = 0.0;

private:
    static uint HashU32(uint row, uint col, uint salt)
    {
        uint x = row * 73856093u ^ col * 19349663u ^ salt * 83492791u;
        x ^= x >> 16;
        x *= 0x7feb352du;
        x ^= x >> 15;
        x *= 0x846ca68bu;
        x ^= x >> 16;
        return x;
    }

    static float Hash01(uint row, uint col, uint salt)
    {
        // Convert lower 24 bits to [0, 1].
        return float(HashU32(row, col, salt) & 0x00FFFFFFu) / float(0x00FFFFFFu);
    }

    static glm::quat MakeRandomRotation(uint row, uint col)
    {
        constexpr float TAU = 6.28318530718f;

        const float rx = Hash01(row, col, 11u) * TAU;
        const float ry = Hash01(row, col, 23u) * TAU;
        const float rz = Hash01(row, col, 37u) * TAU;

        return glm::quat(glm::vec3(rx, ry, rz));
    }

    bool InitMaterial()
    {
        const mtl::MaterialRecipe *loaded_recipe =
            GetRecipe("PBRSpheres.Lit");
        if (!loaded_recipe)
            return false;
        sphere_recipe = *loaded_recipe;

        // T3i：第二套配方（同 def ⇒ 同 shader program；不同 pipeline 配置 ⇒ 第二个批）
        multibatch_enabled = (getenv("PBR_MULTIBATCH") != nullptr);

        if (multibatch_enabled)
        {
            // ⚠ 批键 = {ShaderProgram*, Pipeline*}。两条弯路（实测过）：
            //  1) 只改 pipeline 配置（cull/blend）**不成批**——EDS 之后 Pipeline 只保留
            //     shader 部分，材质渲染状态在绘制时动态应用（PipelineMaterialRenderer::Render）；
            //  2) 换 mtl_def（如 UnlitTexture）**解析失败**（本示例资源集只喂 Lit 所需
            //     纹理/数据行）⇒ 那些实体没有渲染项，不是"第二个批"。
            // ⇒ 用**同 def + 不同顶点节点配置**（billboard 朝向 ⇒ 不同 vertex program，
            //   资源/材质行路径完全一致）拿到真正的第二个批。
            const mtl::MaterialRecipe *billboard_recipe =
                GetRecipe("PBRSpheres.LitBillboard");
            if (!billboard_recipe)
                return false;
            sphere_recipe_b = *billboard_recipe;

            printf("[T3i] PBR_MULTIBATCH=1：网格后半走第二套配方"
                   "（Lit + 顶点节点=Billboard ⇒ 不同 shader program）⇒ 期望 2 个批\n");
        }

        return true;
    }

    /**
     * 把 10×10 网格的材质数据块装上显存。
     *
     * 数据本身**不在代码里**：块来自 `PBR_GRID_DATA_PATH` 指定的文件，读入后整块
     * 一次上传（块内第 i 行 == 池行号 first + i，行号连续 ⇒ 行号可由
     * `row * GRID_SIZE + col` 直接算出，无需逐格保存访问器）。
     */
    bool InitMaterialDataBlock()
    {
        if (!ecs_world)
        {
            printf("[ERROR] InitMaterialDataBlock: No ecs_world\n");
            return false;
        }

        auto *graphics = GetGraphicsContext();
        if (!graphics)
        {
            printf("[ERROR] InitMaterialDataBlock: No GraphicsContext\n");
            return false;
        }

        auto *blocks = graphics->GetMaterialDataBlockRegistry();
        auto *pool   = GetManager<GlobalSSBOBufferRegistry>();
        if (!blocks || !pool)
        {
            printf("[ERROR] InitMaterialDataBlock: No MaterialDataBlockRegistry/pool\n");
            return false;
        }

        sphere_data_block =
            blocks->GetOrLoadURI(ToOSString(PBR_GRID_DATA_PATH));
        if (!sphere_data_block)
        {
            printf("[ERROR] InitMaterialDataBlock: load failed: %s\n",
                   PBR_GRID_DATA_PATH);
            return false;
        }

        if (!sphere_data_block->Upload(pool))
        {
            printf("[ERROR] InitMaterialDataBlock: upload failed: %s\n",
                   sphere_data_block->GetFailureReason().c_str());
            return false;
        }

        if (sphere_data_block->GetRowCount() != GRID_SIZE * GRID_SIZE)
        {
            printf("[ERROR] InitMaterialDataBlock: row count %u != %u\n",
                   sphere_data_block->GetRowCount(), GRID_SIZE * GRID_SIZE);
            return false;
        }

        return true;
    }

    bool InitVDM()
    {
        auto* buffer_manager = GetManager<BufferManager>();
        if (!buffer_manager) {
            printf("[ERROR] InitVDM: No buffer_manager\n");
            return false;
        }

        mesh_vdm = new VertexDataManager(
            buffer_manager,
            CreateStandardTextureArrayGeometryVertexFormat());
        if (!mesh_vdm) {
            printf("[ERROR] InitVDM: Failed to create VertexDataManager\n");
            return false;
        }

        if (!mesh_vdm->Init(HGL_SIZE_1MB, HGL_SIZE_1MB, IndexType::U32)) {
            printf("[ERROR] InitVDM: Failed to init VertexDataManager\n");
            return false;
        }
        return true;
    }

    bool CreateBuiltinGeometries()
    {
        static const char *geometry_uri[GEOMETRY_VARIANT_COUNT] =
        {
            "geo:builtin:sphere_64",
            "geo:builtin:dome_64",
            "geo:builtin:cone",
            "geo:builtin:cylinder",
            "geo:builtin:torus",
            "geo:builtin:hollow_cylinder",
            "geo:builtin:hex_sphere",
            "geo:builtin:capsule",
            "geo:builtin:tapered_capsule_wide",
            "geo:builtin:cube",
        };

        // 共享顶点池：一批几何写进同一个 VertexDataManager（混合缓冲的前提）
        GeometryCreater pc(mesh_vdm);
        SharedCreaterScope scope(GetGeometryAssetRegistry(), &pc);

        for (uint i = 0; i < GEOMETRY_VARIANT_COUNT; ++i)
            builtin_geometries[i] = GetGeometry(geometry_uri[i]);

        for (uint i = 0; i < GEOMETRY_VARIANT_COUNT; ++i)
        {
            if (!builtin_geometries[i]) {
                printf("[ERROR] CreateBuiltinGeometries: Failed to create geometry %u\n", i);
                return false;
            }
        }

        return true;
    }

    bool CreateBasePrimitives()
    {
        for (uint i = 0; i < GEOMETRY_VARIANT_COUNT; ++i)
        {
            if (!builtin_geometries[i]) {
                printf("[ERROR] CreateBasePrimitives: Failed to create primitive %u\n", i);
                return false;
            }
            base_primitives[i] = PrimitiveAsset(builtin_geometries[i], &sphere_recipe, PrimitiveType::Triangles);

            // T3i：第二套 primitive（同几何、第二配方）
            if (multibatch_enabled)
                base_primitives_b[i] = PrimitiveAsset(builtin_geometries[i], &sphere_recipe_b, PrimitiveType::Triangles);
        }

        return true;
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if (!ecs_world) {
            printf("[ERROR] InitECS: No ecs_world\n");
            return false;
        }

        if (!InitMaterialDataBlock())
            return false;

        // 配方的 material SSBO 绑定 = 数据块所在池 + 块首行；具体球的行走 per-entity 4-ID 表。
        const graph::GlobalSSBOBinding block_binding{
            sphere_data_block->GetGlobalSSBOType(),
            sphere_data_block->GetSSBOId(),
            sphere_data_block->RowIDAt(0)};

        if (!(sphere_recipe.material_ssbo_binding = block_binding).IsValid())
        {
            printf("[ERROR] InitECS: invalid material SSBO binding\n");
            return false;
        }

        // T3i：第二配方共用同一 material SSBO 绑定（数据行差异由 per-entity 4-ID 表承载）
        if (multibatch_enabled)
            sphere_recipe_b.material_ssbo_binding = sphere_recipe.material_ssbo_binding;

        if (!CreateBasePrimitives())
            return false;

        // 计算网格原点，使整体居中于世界原点
        const float offset = (GRID_SIZE - 1) * SPHERE_SPACING * 0.5f;

        for (uint row = 0; row < GRID_SIZE; ++row)
        {
            for (uint col = 0; col < GRID_SIZE; ++col)
            {
                // col  → metallic  (X axis)
                // row  → roughness (Y axis)
                const float x = col * SPHERE_SPACING - offset;
                const float y = row * SPHERE_SPACING - offset;

                std::string name = "Sphere_M" + std::to_string(col)
                                 + "_R"       + std::to_string(row);

                // T3c-3 对照：**(0,0) 那个球是静态实体**（其余全可动）。静态实体的 L2W 行号
                // 是常量（`1 + slot`）⇒ 写者每帧重下发也不产生脏行（零重传）；可动实体则随帧槽轮转。
                // 本行只作**行为对照**（位置/画面与其余球一致）；机理判据在探针里（纯数据）。
                const Mobility mobility = (row == 0 && col == 0) ? Mobility::Static : Mobility::Movable;
                const bool use_recipe_b = multibatch_enabled && (col >= GRID_SIZE / 2);

                Entity *e = RenderableBuilder(ecs_world, name.c_str())
                    .WithTransform(glm::vec3(x, y, 0.0f),
                                   MakeRandomRotation(row, col),
                                   glm::vec3(1.0f), mobility)
                    .WithPrimitiveAsset(
                        use_recipe_b ? &base_primitives_b[col] : &base_primitives[col])
                    .WithMaterialDataRow(
                        ToOSString(PBR_GRID_DATA_PATH),
                        row * GRID_SIZE + col)
                    .Build();
                if (!e)
                    return false;
                sphere_entities[row][col] = e;
                sphere_transforms[row][col] =
                    ecs_world->GetTransformByEntity(e->GetEntityID());

                // T3h 判据实体（PBR_SHADOW_FACTS=1 才挂）：两个**非默认阴影事实**
                // 实体——(2,3) 不接收、(7,4) bias=2.0。本示例无投影光 ⇒ 像素看不出
                // 差别（像素全等不构成接线证据）；真判据 = 台架 CheckShadowFacts
                // 值链三段对拍（访问器→EntityGPU 行→gather 行，逐字节）。
                if (getenv("PBR_SHADOW_FACTS"))
                {
                    if (row == 2 && col == 3)
                        e->AddComponent<ShadowProxy>()->SetReceiveShadow(false);
                    else if (row == 7 && col == 4)
                        e->AddComponent<ShadowProxy>()->SetBiasMultiplier(2.0f);
                }

                hgl::ecs::MaterialData *material_data_comp =
                    ecs_world->GetOrCreateMaterialData(e);
                if (!material_data_comp)
                    return false;

                // 每格的差别只有「数组图层号」= 行号 ⇒ 图层号属于配方，给该实体挂一份配方覆盖。
                // 纹理本身由配方文件声明（texarray:@pbr_10x10_*），采样器由 Bindless 预设统一管理。
                graph::mtl::MaterialRecipe cell_recipe =
                    use_recipe_b ? sphere_recipe_b : sphere_recipe;

                for (graph::mtl::RecipeTextureBinding &binding : cell_recipe.textures)
                    if (binding.texture_name == "base_color" || binding.texture_name == "normal")
                        binding.array_layer = row;

                material_data_comp->SetRecipe(cell_recipe);
                // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删
            }
        }

        return true;
    }

    bool EnsureCameraSystem()
    {
        if (!ecs_world) {
            printf("[ERROR] EnsureCameraSystem: No ecs_world\n");
            return false;
        }

        auto camera_system = ecs_world->EnsureCameraSystem();

        if (!camera_system) {
            printf("[ERROR] EnsureCameraSystem: Failed to get or create camera system\n");
            return false;
        }
        return true;
    }

    bool InitCamera()
    {
        if (!EnsureCameraSystem()) {
            printf("[ERROR] InitCamera: Failed to ensure camera system\n");
            return false;
        }

        camera_entity = ecs_world->CreateEntity<Entity>("MainCamera");
        // 相机 = **世界级资源**：经世界访问器创建/取回（实体只是宿主），不直取组件
        auto *camera = ecs_world->GetOrCreateCamera(camera_entity);

        // 俯视整个 10×10 局部 -- 拉远足够看到所有球
        camera->control_mode   = CameraControlMode::ViewModel;
        camera->target         = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance       = 40.0f;
        camera->yaw            = 0.0f;
        camera->pitch          = -25.0f;

        // dump/性能对照模式的相机覆盖（不依赖 PBR_DUMP ⇒ 性能对比也能跑离屏剔除场景）
        if (getenv("PBR_DUMP") || getenv("PBR_PITCH") || getenv("PBR_YAW")
         || getenv("PBR_DIST") || getenv("PBR_TX") || getenv("PBR_TZ"))
        {
            if (const char *v = getenv("PBR_PITCH"))
                camera->pitch = (float)atof(v);
            if (const char *v = getenv("PBR_YAW"))
                camera->yaw = (float)atof(v);
            if (const char *v = getenv("PBR_DIST"))
                camera->distance = (float)atof(v);
            if (const char *v = getenv("PBR_TX"))
                camera->target.x = (float)atof(v);
            if (const char *v = getenv("PBR_TZ"))
                camera->target.z = (float)atof(v);
        }
        camera->is_main_camera = true;
        camera->matrix_dirty   = true;

        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:

    ~PBRSpheresApp()
    {
        // 共享顶点池的几何由 GeometryAssetRegistry 持有：先销毁几何，再销毁顶点池
        if (auto *registry = GetGeometryAssetRegistry())
            registry->Clear();

        SAFE_CLEAR(mesh_vdm)
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.08f, 0.08f, 0.08f, 1.0f));

        if (!InitMaterial())
            return false;

        if (!InitVDM())
            return false;

        if (!CreateBuiltinGeometries())
            return false;

        if (!InitECS())
            return false;

        if (!InitCamera())
            return false;

        // A6-d T2：**接管已强制**（opt-in 面整体删除）——主帧 mesh 批恒走 GPU 路径。

        // PBR_DUMP=<tag>：像素验收 dump —— 帧 8（剔除已热、场景静止）装一次性帧内
        // 回读钩子，落原始字节（A2B10G10R10 packed，与 AlphaTestShadow 同口径），
        // 落完 std::exit。t 冻结为 0 ⇒ 两次运行的几何逐帧确定。
        if (const char *tag = getenv("PBR_DUMP"))
        {
            dump_tag = tag;
        }

        // PBR_FRAMES=N：性能对照定帧退出（配 ULRE_DEBUG.toml 的 channels.perf = true；Tick 全帧计时打印）。
        if (const char *v = getenv("PBR_FRAMES"))
            perf_frames = (uint32_t)atoi(v);

        return true;
    }

    void Tick(double delta_time) override
    {
        // ULRE_DEBUG.toml 的 channels.perf = true：Tick 全帧 CPU 时间（record/present 同步框架内的线程占用；
        // 与 cull/batch/dispatch 分计时互补，给出端到端口径。引擎时间源，禁 chrono）
        const bool perf_on = perf_frames > 0;
        const uint64 perf_t0 = hgl::GetUptimeUs();

        // dump 模式：时间冻结 ⇒ 两次运行（GPU 剔除 vs CPU 基线）的几何逐帧确定，
        // 像素差只可能来自剔除判据本身。
        // 帧摘要自检（PBR_SELFCHECK=1）同理必须冻结时间，否则两次运行几何不同 ⇒ 摘要非确定。
        if (dump_tag.empty() && getenv("PBR_SELFCHECK") == nullptr)
            elapsed_time += delta_time;

        const float t = static_cast<float>(elapsed_time);

        for (uint row = 0; row < GRID_SIZE; ++row)
        {
            for (uint col = 0; col < GRID_SIZE; ++col)
            {
                auto &transform = sphere_transforms[row][col];
                if (!transform.IsValid())
                    continue;

                // Keep per-cell phase/speed differences to make highlights easier to observe.
                const float phase = Hash01(row, col, 101u) * 6.28318530718f;
                const float speed = 0.20f + Hash01(row, col, 211u) * 0.55f;
                const float angle = phase + t * speed;

                const glm::quat spin = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
                transform.SetLocalRotation(spin * MakeRandomRotation(row, col));
            }
        }

        WorkObject::Tick(delta_time);

        // PBR_DUMP 模式：第 8 帧装一次性帧内回读钩子（钩子在本帧提交后、present 前
        // 执行 ⇒ 读到本帧画面），落原始 packed 字节后 std::exit（同 AlphaTestShadow
        // 自检先例：跳过框架析构，验收路径可接受）。
        // PBRSELFCHECK=1：**帧摘要自检**（A6-d 判据重建；共用件 example/Shared/FrameDigest.h）——
        // 第 8 帧帧内回读颜色靶 ⇒ {不同字节取值数, FNV-1a 64} 比对下方内联常量：PASS 退出 0 / FAIL 退出 1。
        // 判据只建在**冻结相机 + 定帧**上；常量由实跑记录，改渲染结果（少画/错画/挪相机）必须复核重录。
        {
            static const bool s_selfcheck = (getenv("PBR_SELFCHECK") != nullptr);
            static uint32_t   s_self_frame = 0;

            static constexpr uint32_t kSelfW           = 1280;
            static constexpr uint32_t kSelfH           = 720;
            static constexpr uint32_t kSelfMinDistinct = 2;   // 不空转守卫（恒定图会 FAIL）
            static constexpr uint64_t kSelfFNV         = 0x3dd88a0999cdc486ull;  // 冻结相机 + 接管下实跑记录
                                                        // （改渲染结果必须复核并重录）

            if (s_selfcheck && ++s_self_frame == 8)
            {
                auto * rt = dynamic_cast<graph::SwapchainRenderTarget *>( ecs_world ? ecs_world->GetRenderTarget() : nullptr);
                if (!rt)
                {
                    printf("[PBRSELF] no swapchain RT\n");
                    std::exit(2);
                }

                ulre_example::FrameDigest expect;
                expect.width                = kSelfW;
                expect.height               = kSelfH;
                expect.distinct_byte_values = kSelfMinDistinct;
                expect.fnv1a64              = kSelfFNV;

                ulre_example::InstallFrameDigestHook(rt, expect, "PBRSELF");
            }
        }


        if (!dump_tag.empty() && ++dump_frame == 8)
        {
            auto *rt = dynamic_cast<graph::SwapchainRenderTarget *>(
                ecs_world ? ecs_world->GetRenderTarget() : nullptr);
            if (!rt)
            {
                printf("[PBRDUMP] no swapchain RT\n");
                std::exit(2);
            }

            std::string tag = dump_tag;
            graph::IRenderTarget *rt_ptr = rt;
            rt->SetInFrameReadbackHook([tag, rt_ptr]()
            {
                std::vector<uint8_t> pixels;
                graph::TextureReadbackInfo info;
                if (!graph::ReadbackColorTarget(rt_ptr, pixels, 0, &info) || pixels.empty())
                {
                    printf("[PBRDUMP] readback failed\n");
                    std::exit(2);
                }

                char name[256];
                snprintf(name, sizeof(name), "pbrdump_%s_%ux%u_%s.raw",
                         tag.c_str(), info.width, info.height,
                         GetVulkanFormat(info.format) ? GetVulkanFormat(info.format)->name : "unknown");
                FILE *fp = fopen(name, "wb");
                if (!fp)
                {
                    printf("[PBRDUMP] open %s failed\n", name);
                    std::exit(2);
                }
                fwrite(pixels.data(), 1, pixels.size(), fp);
                fclose(fp);
                printf("[PBRDUMP] wrote %s (%zu bytes)\n", name, pixels.size());
                std::exit(0);
            });
        }

        // channels.perf：Tick 全帧 CPU 时间累计，240 帧一窗打印；PBR_FRAMES 到数即退。
        if (perf_on)
        {
            perf_total_us += double(hgl::GetUptimeUs() - perf_t0);
            ++perf_frame_seen;

            if (perf_frame_seen % 240 == 0)
            {
                printf("[PBR-PERF] tick: window=240 avg=%.1f us/frame\n",
                       perf_total_us / 240.0);
                fflush(stdout);
                perf_total_us = 0.0;
            }

            if (perf_frame_seen >= perf_frames)
            {
                printf("[PBR-PERF] done frames=%u\n", perf_frame_seen);
                fflush(stdout);
                std::exit(0);
            }
        }
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<PBRSpheresApp>(OS_TEXT("Standard BuiltinGeometry x Albedo+Normal 10x10"), argc, argv, 1280, 720);
}
