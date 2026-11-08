// ChessboardStress —— 阶段八 **实机压测**工程（T8.1 场景 / T8.2 监控与合批验证）
//
// ═════════════════════════════════════════════════════════════════════════════
// 它和 `src/ecs/tests/TestGPUModelStress.cpp` 的分工（**别混口径**）
// ═════════════════════════════════════════════════════════════════════════════
//   · 无头台架（TestGPUModelStress）：不碰显卡，量「CPU 侧记账/内存」收益，
//     口径 10,000 模型 / 40,000 部件 —— 那里没有 VRAM、没有剔除容量。
//   · 本工程：跑**真机渲染路径**（收集 → 剔除 → 模板展开 → 实例化 draw），
//     规模必须落在引擎的硬上限之内（下面第二节的算式），所以**放不下 10,000**。
//     两个数字不可互相引用。
//
// ═════════════════════════════════════════════════════════════════════════════
// 一、演示内容
// ═════════════════════════════════════════════════════════════════════════════
//   · 默认 27×27 = 729 个**棋盘模型**实例（每模型 1 个 GPU 模板 × 4 个部件），
//     场景里加一个棋盘 = 实体树上加一行；部件展开 / 逐实例矩阵 / 剔除全在
//     `entity_gpu_cull.glsl` 里完成，CPU 不按部件记账。
//   · 默认 400 颗**骰子**走**传统路径**（每实体 1 条 4-ID 行、无模板）作为对照：
//     同一相机、同一帧里既有「模板桶组」也有「素图元实例」，验证两条路径共存
//     不打架（模板实体按 `part_bucket_count` 占连续桶组，素图元落在基础桶）。
//   · 骰子是 `BodyMotionType::Dynamic` 刚体，从空中落下弹在棋盘面上 ⇒ 量的是
//     「Jolt 每帧写回动态 Transform」与「GPU 侧静态/动态双区 TRS 竞技场」共存。
//   · 相机默认绕方阵轨道旋转 ⇒ 实例随视锥进出，剔除账在动。
//
// ═════════════════════════════════════════════════════════════════════════════
// 二、规模为什么是 676 × 4 + 400（**改数字前先看这一节**）
// ═════════════════════════════════════════════════════════════════════════════
// 引擎有三道 4096 硬墙（都是 `static constexpr`，没有扩容开关）：
//   ① `EntityGPUTable::kCapacity = 4096`              —— 实体行上限
//   ② `GPUCullSystem::kMaxVisibleEntities = 4096`      —— 可见实体上限
//   ③ `TransformDataStorage` 默认 4096 / 4096          —— 静态/动态 TRS 行上限
// 更紧的是**竞技场行预算**（`GPUCullSystem::kArenaRowsPerPass = 4096`，每个 pass
// 一份扇区，同 pass 内**所有批共享**同一段 `row_cursor`）：
//
//     占用行数 = Σ(每个素材桶容量) + 1 行桶头/桶，各桶按 4 行向上对齐
//     模板实体 ⇒ 展开 `part_bucket_count` 行（部件桶组）
//             ＋ **另占 1 行代表几何体桶**（模板实体仍带一条素图元，
//               这条桶按实例数计容量，是本设计已知的固定开销）
//     素图元 ⇒ 每实体 1 行
//
//   ⇒ 4 部件模板下**每棋盘 5 行**，`5 × 实体数` 就已经贴满 4096，
//     **896 个棋盘（5 × 896 = 4480）必越界**——这是本工程第一次实跑撞的墙
//     （`[T3i] 竞技场实例行超容量` 断言 + 整批不绘制）。
//   ⇒ 本档：`5 × 729 + 350 = 3995`，留 101 行余量（桶 4 行对齐 + 未来加桶）。
//     实测桶数 = 2（代表几何体桶 1079 + 部件桶组 2916，4 个部件同几何 ⇒ 同一个
//     `bucket_slot`，容量按 `部件数 × 实例数` 计），对齐只吃 1 行。
//     越界的后果不是掉帧，是 `PrimitiveBatchPipeline` 的 fail-fast：
//     打一行「竞技场实例行超容量（kArenaRowsPerPass）」并把该批 items 清空
//     ⇒ **整批当帧不绘制**（黑屏），所以下面的运行期钳制不是可选项。
//
// env 可以改档（`CSTRESS_BOARDS` / `CSTRESS_DICE`），但会在 Init 里按上式**钳制**
// 并打印实际采用的规模；`CSTRESS_SELFCHECK=1` 会强制回默认档（摘要判据要确定）。
//
// ═════════════════════════════════════════════════════════════════════════════
// 三、判据（可自动判定）
// ═════════════════════════════════════════════════════════════════════════════
//   · `[CSTRESS] 合批验证 PASS` —— 派发定帧后核对：Σ桶容量 == 5×棋盘数 + 骰子数
//     （= 4 个部件桶 + 1 个代表几何体桶）× 棋盘数 + 素图元骰子数；
//     桶数应与「几何种类 × 材质批」一致；行数对不上就说明桶组分配出错）
//   · `[CSTRESS] 溢出槽=0` —— GPU 侧 `counters[batch_count]` 溢出计数为 0
//     （非 0 = 可见实例超桶容量，剔除侧静默丢实例）
//   · `[CSTRESS] 帧摘要 PASS` —— `CSTRESS_SELFCHECK=1` 的像素判据
//   · 引擎日志 0 VUID / 0 `[ERROR]`
//
// env：
//   CSTRESS_FRAMES=N      跑 N 帧后退出（CI / 定帧）
//   CSTRESS_STATIC=1      冻结相机与落体（骰子以 Static 姿态摆放）⇒ 取景确定
//   CSTRESS_SELFCHECK=1   帧摘要自检（PASS 退 0 / FAIL 退 1；自动冻结 + 回默认档）
//   CSTRESS_DUMP=<tag>    第 8 帧帧内回读落原始字节后退出
//   CSTRESS_BOARDS=N      棋盘实例数（默认 729，按行预算钳制）
//   CSTRESS_DICE=N        骰子数（默认 350，按行预算钳制）
//   CSTRESS_NO_PHYSICS=1  不注册 Jolt，用来分离「物理耗时」与「渲染耗时」
//   CSTRESS_ORBIT=<deg/s> 轨道转速（默认 10）
//   CSTRESS_SAMPLE=N      每 N 帧采一次 GPU 侧面（默认 60；采样帧会 fence 等待）
//   CSTRESS_SEED=N        骰子初始位置/朝向的随机种子（默认 20260814）

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/ModelTemplateManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/graph/mesh/StaticMesh.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/graph/ssbo/GlobalSSBOTypes.h>
#include<hgl/graph/ssbo/InstanceCustomData.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/MaterialResolver.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/support/transform/TransformAccessor.h>
#include<hgl/ecs/support/pipeline/RenderPassSlot.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/GPUModelRefComponent.h>
#include<hgl/ecs/components/MaterialData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/components/ModelCollisionProxyComponent.h>
#include<hgl/ecs/components/RigidBodyComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/tick/JoltPhysicsSystem.h>
#include<hgl/ecs/support/cull/GPUCullSystem.h>
#include<hgl/ecs/support/storage/FrameProfiler.h>
#include<hgl/ecs/support/storage/PrimitiveTopologyLedger.h>
#include<hgl/ecs/support/storage/MainFrameTopologySnapshot.h>
#include<hgl/ecs/systems/render/collect/RenderPrimitiveCollectSystem.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>

#include<hgl/time/Time.h>

#include"../Shared/FrameDigest.h"   // 帧摘要自检（A6-d 判据重建共用件）

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<cmath>
#include<cstdio>
#include<cstdlib>
#include<string>
#include<vector>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    // ── 硬上限（与引擎常量同源；改引擎必须同步改这里）──────────────────────
    constexpr uint32_t kArenaRowsPerPass = 4096;   // GPUCullSystem::kArenaRowsPerPass
    constexpr uint32_t kArenaHeadroom    = 64;     // 桶对齐（≤3行/桶）+ 未来加桶的余量

    // ── 默认档（见文件头第二节的算式）─────────────────────────────────────
    constexpr uint32_t kDefaultBoards = 729;       // 5 × 729 = 3645
    constexpr uint32_t kDefaultDice   = 350;       //      + 350 = 3995 ≤ 4096
    constexpr float    kCellSpacing   = 1.25f;     // 底板宽 1.12 ⇒ 留 0.13 缝

    // 棋盘本体全部由**内置单位立方体**（±0.5）拼出：部件只差模板内的 local_matrix。
    GeometryVertexFormat CreateCubeGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::Normal,   VF_V3F},
        };
        return gvf;
    }

    // 模板部件：底板 / 台面（略窄一圈、压在底板上）/ 中央钮（棋子意象）
    struct PartDef
    {
        const char   *name;
        math::Vector3f scale;
        math::Vector3f translation;
    };

    static const PartDef kParts[] =
    {
        { "Base",    { 1.12f, 1.12f, 0.06f }, { 0.0f, 0.0f, 0.030f } },
        { "Surface", { 0.98f, 0.98f, 0.03f }, { 0.0f, 0.0f, 0.075f } },
        { "Knob",    { 0.16f, 0.16f, 0.20f }, { 0.0f, 0.0f, 0.190f } },
        { "Apron",   { 0.09f, 1.12f, 0.10f }, { 0.0f, 0.0f, 0.080f } },   // 第 4 个部件只为凑满 4 部件预算
    };
    constexpr size_t PART_COUNT = sizeof(kParts) / sizeof(kParts[0]);

    // 每棋盘占用行数 = 部件桶组（part_bucket_count）+ 代表几何体桶（1 行/实例）
    constexpr uint32_t kRowsPerBoard = uint32_t(PART_COUNT) + 1u;

    // 逐实例自定义色：浅格 / 深格（材质行是白 ⇒ 最终画面色 = 本色）
    static const math::Vector4f kLightColor(0.87f, 0.83f, 0.74f, 1.0f);
    static const math::Vector4f kDarkColor (0.16f, 0.17f, 0.20f, 1.0f);

    // 骰子的 4 种自定义色（与棋盘同池、同材质批 ⇒ 同一条基础桶）
    static const math::Vector4f kDiceColors[4] =
    {
        math::Vector4f(0.85f, 0.20f, 0.18f, 1.0f),
        math::Vector4f(0.20f, 0.55f, 0.85f, 1.0f),
        math::Vector4f(0.95f, 0.72f, 0.15f, 1.0f),
        math::Vector4f(0.30f, 0.78f, 0.42f, 1.0f),
    };

    // 骰子几何尺寸（单位立方体 ⇒ 缩放即尺寸）
    constexpr float kDieHalf     = 0.16f;
    constexpr float kDieSize     = kDieHalf * 2.0f;
    constexpr float kDieMass     = 0.4f;
    constexpr float kDieSpawnMinZ = 2.5f;
    constexpr float kDieSpawnMaxZ = 18.0f;

    // 玩具 LCG：只要可复现，不要随机质量
    struct Lcg
    {
        uint32_t s;
        explicit Lcg(uint32_t seed) : s(seed ? seed : 1u) {}
        uint32_t Next() { s = s * 1664525u + 1013904223u; return s; }
        float Unit()    { return float(Next() >> 8) / 16777215.0f; }          // [0,1]
        float Range(float lo, float hi) { return lo + (hi - lo) * Unit(); }
    };
}

namespace
{
    // 材质数据块：一行中性白，颜色差异全部走逐实例自定义色
    constexpr char MATERIAL_DATA_URI[] =
        "mtldata:file:ShaderLibrary/asset/material_data/gpu_model_chessboard.material_data.toml";
    constexpr uint32_t MATERIAL_DATA_ROW = 0;
}

class ChessboardStressApp : public WorkObject
{
private:

    ECSContext * ecs_context   = nullptr;
    Entity *     camera_entity = nullptr;

    // 共享几何 / 材质（模板部件与实体代表图元用**同一个** PrimitiveAsset）
    Geometry *                 cube_geometry = nullptr;
    PrimitiveAsset             cube_asset{};
    graph::mtl::MaterialRecipe board_recipe{};
    graph::GlobalSSBOBinding   base_row{};

    // GPU 模板与自定义数据行号（Init 期注册，终身有效）
    uint32_t template_id  = 0;
    uint32_t custom_light = 0;   ///< InstanceCustomDataPool 行号（≠0，行 0 是哨兵）
    uint32_t custom_dark  = 0;
    uint32_t dice_rows[4] = { 0, 0, 0, 0 };

    // ── 实际采用的规模（Init 里按行预算钳制后落定）────────────────────────
    uint32_t board_count = kDefaultBoards;
    uint32_t dice_count  = kDefaultDice;
    uint32_t grid_cols   = 0;
    uint32_t grid_rows   = 0;
    float    grid_offset_x = 0.0f;
    float    grid_offset_y = 0.0f;

    std::shared_ptr<JoltPhysicsSystem> physics;

    // ── 运行期开关与统计 ──────────────────────────────────────────────────
    double time = 0.0;

    bool     static_scene = false;
    bool     no_physics   = false;
    float    orbit_speed  = 10.0f;
    uint32_t sample_interval = 60;
    bool     profile_breakdown = true;   ///< 打印详细阶段耗时剖析
    bool     topo_snapshot     = false;  ///< P2：开启主帧拓扑快照（只观测）
    bool     collect_cache     = false;  ///< P3：开启主帧收集快路径（依赖 P2 快照）

    std::string dump_tag;
    uint32_t    dump_frame  = 0;
    uint32_t    run_frames  = 0;
    uint32_t    frame_seen  = 0;
    bool        selfcheck       = false;
    uint32_t    selfcheck_frame = 0;

    uint64_t last_frame_us = 0;
    uint32_t sample_frame  = 0;

    // 帧耗时累加（只在两个采样点之间累计，避免长期漂移）
    double   acc_frame_ms        = 0.0;
    uint32_t acc_frames          = 0;

    bool batch_verified = false;

    // 帧摘要常量：**冻结场景 + 定帧 8** 下 Debug 实跑记录（CSTRESS_STATIC 口径，
    // 相机与落体同时冻结、规模强制回默认档）。改渲染结果必须同步复核并重录。
    static constexpr uint32_t kSelfW           = 1280;
    static constexpr uint32_t kSelfH           = 720;
    static constexpr uint32_t kSelfMinDistinct = 2;
    static constexpr uint64_t kSelfFNV         = 0x701368BFE0E60060ull;   // 2026-08 Debug 实跑记录

private:

    // ─────────────────────────────────────────────────────────────────────
    // 规模钳制：把「行预算」这道硬墙落成运行期可验证的不变量
    // ─────────────────────────────────────────────────────────────────────
    void ResolveScale()
    {
        const uint32_t max_boards = (kArenaRowsPerPass - kArenaHeadroom) / kRowsPerBoard;
        if (board_count > max_boards)
        {
            printf("[CSTRESS] 棋盘数 %u 超竞技场行预算 ⇒ 钳制到 %u（每实例 %u 行 = %zu 部件 + 1 代表几何体）\n",
                   board_count, max_boards, kRowsPerBoard, PART_COUNT);
            board_count = max_boards;
        }
        if (board_count == 0)
            board_count = 1;

        const uint32_t rows_used_by_boards = kRowsPerBoard * board_count;
        const uint32_t max_dice = (rows_used_by_boards + kArenaHeadroom < kArenaRowsPerPass)
                                ? (kArenaRowsPerPass - kArenaHeadroom - rows_used_by_boards)
                                : 0u;
        if (dice_count > max_dice)
        {
            printf("[CSTRESS] 骰子数 %u 超剩余行预算 ⇒ 钳制到 %u\n", dice_count, max_dice);
            dice_count = max_dice;
        }

        // 方阵排布：列数取 √ 向上取整，行数向上取整（最后一排可能不满，无所谓）
        grid_cols = uint32_t(std::ceil(std::sqrt(double(board_count))));
        if (grid_cols == 0)
            grid_cols = 1;
        grid_rows = (board_count + grid_cols - 1) / grid_cols;

        grid_offset_x = float(grid_cols - 1) * 0.5f * kCellSpacing;
        grid_offset_y = float(grid_rows - 1) * 0.5f * kCellSpacing;

        const uint32_t rows = rows_used_by_boards + dice_count;
        printf("[CSTRESS] 规模定档：棋盘 %u（%u×%u 方阵，%zu 部件/实例 ⇒ %u 行/实例）+ 骰子 %u"
               " ⇒ 展开实例行 %u / %u（余量 %u）\n",
               board_count, grid_cols, grid_rows, PART_COUNT, kRowsPerBoard, dice_count,
               rows, kArenaRowsPerPass, kArenaRowsPerPass - rows);
        fflush(stdout);
    }

    bool InitMaterial()
    {
        if (!GetMaterialDataBinding(MATERIAL_DATA_URI, MATERIAL_DATA_ROW, base_row))
            return false;

        const mtl::MaterialRecipe *loaded_recipe = GetRecipe("GPUModelChessboard.Board");
        if (!loaded_recipe)
            return false;
        board_recipe = *loaded_recipe;

        if (!(board_recipe.material_ssbo_binding = base_row).IsValid())
            return false;

        return true;
    }

    bool CreateCubeGeometry()
    {
        auto * device        = GetDevice();
        auto * ssbo_registry = GetManager<GlobalSSBOBufferRegistry>();

        if (!device || !ssbo_registry)
            return false;

        const GeometryVertexFormat geometry_vertex_format = CreateCubeGeometryVertexFormat();
        cube_geometry = GetGeometry("geo:builtin:cube", &geometry_vertex_format);
        if (!cube_geometry)
            return false;

        // 池行必须就位：剔除与绘制都按 geometry_id 索引 MeshDrawParams 行
        cube_geometry->EnsureMeshDrawParams(ssbo_registry, device);

        if (cube_geometry->GetGeometryID() == 0)
        {
            GLogError(u8"[ChessboardStress] 立方体几何未拿到 MeshDrawParams 行号（geometry_id==0）");
            return false;
        }

        return true;
    }

    /**
     * @brief 把「4 部件棋盘」打包进 GPU 模板池（一次性，之后场景侧只增实体行）。
     *
     * content 只在本函数内存在：`RegisterModelTemplate` 当场把行数据写进
     * `ModelTemplatePool` / `ModelPartPool`（VRAM 常驻），不留 CPU 引用。
     */
    bool RegisterBoardTemplate()
    {
        auto * ssbo_registry = GetManager<GlobalSSBOBufferRegistry>();
        if (!ssbo_registry)
            return false;

        ModelAssetContent content;

        // 1 个图元资产（单位立方体 + 棋盘配方），4 个节点各引用它一次 ⇒ 4 个部件行
        content.assets.emplace_back(cube_geometry, &board_recipe, PrimitiveType::Triangles);

        for (size_t p = 0; p < PART_COUNT; ++p)
        {
            StaticMeshNode node;
            node.name        = kParts[p].name;
            node.parentIndex = -1;
            node.hasTRS      = false;

            // 部件 local_matrix = 平移 ∘ 缩放（列主序：先缩后移）
            node.localMatrix = glm::translate(glm::mat4(1.0f),
                                              glm::vec3(kParts[p].translation.x,
                                                        kParts[p].translation.y,
                                                        kParts[p].translation.z));
            node.localMatrix = glm::scale(node.localMatrix,
                                          glm::vec3(kParts[p].scale.x,
                                                    kParts[p].scale.y,
                                                    kParts[p].scale.z));
            node.worldMatrix = node.localMatrix;   // 无父链 ⇒ 世界 = 局部

            // 部件包围盒在**节点自身局部空间**（单位立方体 ±0.5），
            // 展开侧再用 L2W × local_matrix 变换测试。
            node.nodeBounds.SetFromAABB(math::Vector3f(-0.5f, -0.5f, -0.5f),
                                        math::Vector3f( 0.5f,  0.5f,  0.5f));
            node.boundsValid = true;

            node.primitiveIndices.push_back(0);
            content.nodes.push_back(node);
            content.root_nodes.push_back(static_cast<int32_t>(p));
        }

        template_id = ModelTemplateManager::RegisterModelTemplate(ssbo_registry, content);
        if (template_id == 0)
        {
            printf("[ERROR] RegisterBoardTemplate: 模板注册失败\n");
            return false;
        }

        // 逐实例自定义色：池 reserve_rows=1 ⇒ Acquire 不会碰到哨兵行 0
        custom_light = ssbo_registry->Acquire(GlobalSSBOType::InstanceCustomDataPool);
        custom_dark  = ssbo_registry->Acquire(GlobalSSBOType::InstanceCustomDataPool);

        if (custom_light == 0 || custom_dark == 0)
        {
            printf("[ERROR] RegisterBoardTemplate: 自定义数据行发号失败\n");
            return false;
        }

        if (!ssbo_registry->Write(GlobalSSBOType::InstanceCustomDataPool, custom_light,
                                  ssbo::InstanceCustomData(kLightColor))
         || !ssbo_registry->Write(GlobalSSBOType::InstanceCustomDataPool, custom_dark,
                                  ssbo::InstanceCustomData(kDarkColor)))
        {
            printf("[ERROR] RegisterBoardTemplate: 自定义数据行写入失败\n");
            return false;
        }

        for (uint32_t i = 0; i < 4; ++i)
        {
            dice_rows[i] = ssbo_registry->Acquire(GlobalSSBOType::InstanceCustomDataPool);
            if (dice_rows[i] == 0
             || !ssbo_registry->Write(GlobalSSBOType::InstanceCustomDataPool, dice_rows[i],
                                      ssbo::InstanceCustomData(kDiceColors[i])))
            {
                printf("[ERROR] RegisterBoardTemplate: 骰子自定义色行 %u 失败\n", i);
                return false;
            }
        }

        printf("[CSTRESS] 模板 id=%u parts=%zu｜自定义色行 light=%u dark=%u dice=%u/%u/%u/%u\n",
               template_id, PART_COUNT, custom_light, custom_dark,
               dice_rows[0], dice_rows[1], dice_rows[2], dice_rows[3]);
        fflush(stdout);

        return true;
    }

    bool InitPhysics()
    {
        physics = ecs_context->GetSystem<JoltPhysicsSystem>();
        if (!physics)
        {
            physics = ecs_context->RegisterSystem<JoltPhysicsSystem>();
            if (!physics)
            {
                printf("[ERROR] InitPhysics: JoltPhysicsSystem 注册失败\n");
                return false;
            }
            physics->Initialize();
        }

        printf("[CSTRESS] Jolt 物理已就绪（重力默认 Z-up -9.81；骰子 Dynamic）\n");
        fflush(stdout);
        return true;
    }

    /// 挂「代表图元 + 材质行」——模板实体与素图元实体都要这一步（否则不被收集）
    void AttachPrimitive(Entity * e, uint32_t tmpl, uint32_t custom_row)
    {
        auto * geometry_data = e->GetContext()->GetOrCreateGeometryData(e);
        geometry_data->GetOwner()->GetContext()
            ->GetOrCreateGeometryData(geometry_data->GetOwnerID())->SetPrimitiveAsset(&cube_asset);

        hgl::ecs::MaterialData * material_data = e->GetContext()->GetOrCreateMaterialData(e);
        hgl::ecs::MaterialData::MaterialDataAuthoringResource row_resource{};
        row_resource = base_row;
        material_data->SetDataResource(row_resource);

        auto * ref = e->AddComponent<GPUModelRefComponent>();
        ref->template_id       = tmpl;          // 0 = 非模板实体（素图元路径）
        ref->custom_data_index = custom_row;
    }

    bool InitBoards()
    {
        for (uint32_t i = 0; i < board_count; ++i)
        {
            char name[64];
            snprintf(name, sizeof(name), "Board_%u", i);

            Entity * e = ecs_context->CreateEntity<Entity>(name);

            const uint32_t col = i % grid_cols;
            const uint32_t row = i / grid_cols;

            const float x = float(col) * kCellSpacing - grid_offset_x;
            const float y = float(row) * kCellSpacing - grid_offset_y;

            auto transform = ecs_context->GetTransform(
                ecs_context->CreateTransform(e->GetEntityID(), Mobility::Static));
            transform.SetLocalPosition(glm::vec3(x, y, 0.0f));
            transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform.SetLocalScale(glm::vec3(1.0f));
            transform.SetMobility(Mobility::Static);

            AttachPrimitive(e, template_id, ((col + row) & 1u) ? custom_dark : custom_light);

            // 碰撞代理：底板一枚扁平盒 + 中央钮一枚小盒（骰子落在台面上，不逐部件造形状）
            if (!no_physics)
            {
                auto * proxy = e->AddComponent<ModelCollisionProxyComponent>();
                proxy->AddBox(math::Vector3f(0.0f, 0.0f, 0.05f),
                              math::Vector3f(0.56f, 0.56f, 0.05f));
                proxy->AddBox(math::Vector3f(0.0f, 0.0f, 0.19f),
                              math::Vector3f(0.08f, 0.08f, 0.10f));
            }
        }

        return true;
    }

    bool InitDice()
    {
        if (dice_count == 0)
            return true;

        const char * seed_env = getenv("CSTRESS_SEED");
        Lcg rng(uint32_t(atoi(seed_env ? seed_env : "20260814")));

        const float half_x = grid_offset_x + 0.5f * kCellSpacing;
        const float half_y = grid_offset_y + 0.5f * kCellSpacing;

        for (uint32_t i = 0; i < dice_count; ++i)
        {
            char name[64];
            snprintf(name, sizeof(name), "Die_%u", i);

            Entity * e = ecs_context->CreateEntity<Entity>(name);

            glm::vec3 pos(rng.Range(-half_x, half_x),
                          rng.Range(-half_y, half_y),
                          static_scene ? (0.6f + 0.25f * float(i % 7))
                                       : rng.Range(kDieSpawnMinZ, kDieSpawnMaxZ));

            glm::quat rot(glm::vec3(rng.Range(0.0f, 6.28f),
                                    rng.Range(0.0f, 6.28f),
                                    rng.Range(0.0f, 6.28f)));

            const bool movable = !static_scene;

            auto transform = ecs_context->GetTransform(
                ecs_context->CreateTransform(e->GetEntityID(),
                                             movable ? Mobility::Movable : Mobility::Static));
            transform.SetLocalPosition(pos);
            transform.SetLocalRotation(rot);
            transform.SetLocalScale(glm::vec3(kDieSize));
            transform.SetMobility(movable ? Mobility::Movable : Mobility::Static);

            AttachPrimitive(e, 0, dice_rows[i & 3u]);   // template_id=0 ⇒ 素图元路径

            if (no_physics)
                continue;

            auto * proxy = e->AddComponent<ModelCollisionProxyComponent>();
            proxy->AddBox(math::Vector3f(0.0f, 0.0f, 0.0f),
                          math::Vector3f(kDieHalf, kDieHalf, kDieHalf));

            // ⚠ 不显式挂 RigidBodyComponent 时，`CreateOrUpdateBody` 会按 Mobility
            // 自动补 Kinematic/Static ⇒ **不会落体**。要落体必须自己挂并设 Dynamic。
            auto * body               = e->AddComponent<RigidBodyComponent>();
            body->motion_type         = static_scene ? BodyMotionType::Static : BodyMotionType::Dynamic;
            body->mass                = kDieMass;
            body->friction            = 0.55f;
            body->restitution         = 0.22f;
            body->linear_damping      = 0.05f;
            body->angular_damping     = 0.10f;
            body->allow_sleeping      = true;
        }

        return true;
    }

    bool InitScene()
    {
        cube_asset = PrimitiveAsset(cube_geometry, &board_recipe, PrimitiveType::Triangles);

        if (!InitBoards()) return false;
        if (!InitDice())   return false;

        printf("[CSTRESS] 场景装配完成：模板实体 %u（模板 %u × %zu 部件）+ 素图元实体 %u\n",
               board_count, template_id, PART_COUNT, dice_count);
        fflush(stdout);

        return true;
    }

    bool InitCamera()
    {
        if (!ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");

        auto * camera = ecs_context->GetOrCreateCamera(camera_entity);

        // 方阵外接半径：half-diag ≈ sqrt(hx² + hy²)；距离取约 2.2 倍保证整片入画
        const float half_diag = std::sqrt(grid_offset_x * grid_offset_x
                                        + grid_offset_y * grid_offset_y);

        camera->control_mode = CameraControlMode::ViewModel;
        camera->target       = math::Vector3f(0.0f, 0.0f, 0.4f);
        camera->distance     = half_diag * 2.2f + 8.0f;
        camera->yaw          = 0.0f;
        camera->pitch        = -38.0f;

        // 取景覆盖（dump 模式也走这里 ⇒ 两次运行的像素可比）
        if (const char * v = getenv("CSTRESS_YAW"))   camera->yaw      = (float)atof(v);
        if (const char * v = getenv("CSTRESS_PITCH")) camera->pitch    = (float)atof(v);
        if (const char * v = getenv("CSTRESS_DIST"))  camera->distance = (float)atof(v);
        if (const char * v = getenv("CSTRESS_TX"))    camera->target.x = (float)atof(v);
        if (const char * v = getenv("CSTRESS_TY"))    camera->target.y = (float)atof(v);
        if (const char * v = getenv("CSTRESS_TZ"))    camera->target.z = (float)atof(v);

        camera->is_main_camera = true;
        camera->matrix_dirty   = true;
        camera->viewport_info  = GetViewportInfo();

        return true;
    }

    bool InitCull()
    {
        auto gpu_cull = ecs_context->GetSystem<GPUCullSystem>();
        if (!gpu_cull)
        {
            printf("[ERROR] InitCull: GPUCullSystem 未注册\n");
            return false;
        }

        printf("[CSTRESS] GPU 剔除接管已强制（A6-d T2）；模板展开恒开\n");
        fflush(stdout);
        return true;
    }

    // ─────────────────────────────────────────────────────────────────────
    // T8.2 合批验证：Σ桶容量 == 4×棋盘 + 骰子（行预算被真实占用）
    // ─────────────────────────────────────────────────────────────────────
    void VerifyBatching(GPUCullSystem * gpu_cull)
    {
        const uint32_t batch_count = gpu_cull->GetBatchCount();
        if (batch_count == 0)
            return;

        const uint32_t expect_rows = kRowsPerBoard * board_count + dice_count;

        uint32_t row_sum = 0;

        for (uint32_t b = 0; b < batch_count; ++b)
        {
            uint32_t row_base = 0;
            uint32_t capacity = 0;
            if (!gpu_cull->GetBatchLayout(RenderPassSlot::Main, b, row_base, capacity))
                continue;

            row_sum += capacity;
        }

        const bool ok = (row_sum == expect_rows);

        printf("[CSTRESS] 合批验证 %s：素材桶=%u  Σ桶容量=%u（期望 %u）"
               "｜每桶一条实例化 draw ⇒ ICB 命令数=%u\n",
               ok ? "PASS" : "FAIL", batch_count, row_sum, expect_rows, batch_count);
        fflush(stdout);

        // 逐桶明细（一眼看出哪个桶吃掉了多少行）
        for (uint32_t b = 0; b < batch_count; ++b)
        {
            uint32_t row_base = 0;
            uint32_t capacity = 0;
            if (!gpu_cull->GetBatchLayout(RenderPassSlot::Main, b, row_base, capacity))
                continue;
            printf("[CSTRESS]   桶 #%u  row_base=%u capacity=%u\n", b, row_base, capacity);
        }
        fflush(stdout);

        batch_verified = true;
    }

    // ─────────────────────────────────────────────────────────────────────
    // P1：失效台帐报表（只观测 —— 不改变任何绘制行为）
    //
    // 报表口径：台帐计数器是**只增不减**的，所以这里报「窗口增量 / 帧」。
    // 判定 P1 是否达标的两个数值：
    //   · `拓扑代数增量/帧` 应 ≈ 0（稳定场景无拓扑变更，快照才有资格复用）
    //   · `池轮询 布局变`  应 = 0（否则说明有系统在每帧分配/归还行）
    // 动态事实增量每帧 > 0 是正常的：它只要求重发事实，不要求重建批。
    // ─────────────────────────────────────────────────────────────────────
    struct LedgerSnapshot
    {
        uint64_t topo[static_cast<size_t>(TopologyChange::Count)]      = {};
        uint64_t fact[static_cast<size_t>(DynamicFactChange::Count)]  = {};
        uint64_t reject[static_cast<size_t>(CacheRejectReason::Count)] = {};

        uint64_t candidate    = 0;
        uint64_t eligible     = 0;
        uint64_t pool_layout  = 0;
        uint64_t pool_content = 0;
    };

    LedgerSnapshot ledger_prev;

    LedgerSnapshot CaptureLedger()
    {
        LedgerSnapshot s;

        if (!ecs_context)
            return s;

        const PrimitiveTopologyLedger &ledger = ecs_context->GetPrimitiveTopologyLedger();

        for (size_t i = 0; i < static_cast<size_t>(TopologyChange::Count); ++i)
            s.topo[i] = ledger.GetTopologyRevision(static_cast<TopologyChange>(i));

        for (size_t i = 0; i < static_cast<size_t>(DynamicFactChange::Count); ++i)
            s.fact[i] = ledger.GetDynamicFactRevision(static_cast<DynamicFactChange>(i));

        for (size_t i = 0; i < static_cast<size_t>(CacheRejectReason::Count); ++i)
            s.reject[i] = ledger.GetStatReject(static_cast<CacheRejectReason>(i));

        s.candidate    = ledger.GetStatCandidateFrames();
        s.eligible     = ledger.GetStatEligibleFrames();
        s.pool_layout  = ledger.GetStatPoolLayoutChanges();
        s.pool_content = ledger.GetStatPoolContentChanges();

        return s;
    }

    void ReportTopologyLedger(uint32_t window_frames)
    {
        if (!ecs_context)
            return;

        const PrimitiveTopologyLedger &ledger = ecs_context->GetPrimitiveTopologyLedger();
        const LedgerSnapshot now = CaptureLedger();

        const double div = (window_frames > 0) ? double(window_frames) : 1.0;

        auto PerFrame = [div](uint64_t cur, uint64_t prev) -> double {
            return (cur >= prev) ? double(cur - prev) / div : 0.0;
        };
        auto Delta = [](uint64_t cur, uint64_t prev) -> uint64_t {
            return (cur >= prev) ? (cur - prev) : 0;
        };

        // ── 拓扑代数（稳态应全 0）─────────────────────────────────────────
        printf("  -- P1 失效台帐（只观测，不参与跳过决策）--\n");
        printf("  %-22s", "拓扑代数增量/帧");

        double topo_sum = 0.0;
        for (size_t i = 0; i < static_cast<size_t>(TopologyChange::Count); ++i)
        {
            const double v = PerFrame(now.topo[i], ledger_prev.topo[i]);
            topo_sum += v;
            printf(" %s=%.2f", GetTopologyChangeName(static_cast<TopologyChange>(i)), v);
        }
        printf("  ｜ 合计=%.2f\n", topo_sum);

        // ── 动态事实代数（每帧 > 0 属正常）────────────────────────────────
        printf("  %-22s", "动态事实增量/帧");

        double fact_sum = 0.0;
        for (size_t i = 0; i < static_cast<size_t>(DynamicFactChange::Count); ++i)
        {
            const double v = PerFrame(now.fact[i], ledger_prev.fact[i]);
            fact_sum += v;
            printf(" %s=%.2f", GetDynamicFactChangeName(static_cast<DynamicFactChange>(i)), v);
        }
        printf("  ｜ 合计=%.2f\n", fact_sum);

        // ── 池级轮询（布局变 = 危险信号；内容变 = 常态）──────────────────
        printf("  %-22s 布局变=%llu 内容变=%llu\n", "池代数轮询（窗口累计）",
               static_cast<unsigned long long>(Delta(now.pool_layout,  ledger_prev.pool_layout)),
               static_cast<unsigned long long>(Delta(now.pool_content, ledger_prev.pool_content)));

        // ── 资格判定（P1 中 `NoSnapshot` 恒为真，命中数应为 0）────────────
        const uint64_t cand = Delta(now.candidate, ledger_prev.candidate);
        const uint64_t hit  = Delta(now.eligible,  ledger_prev.eligible);

        printf("  %-22s 候选=%llu 命中=%llu (%.1f%%)\n", "主帧缓存资格",
               static_cast<unsigned long long>(cand),
               static_cast<unsigned long long>(hit),
               (cand > 0) ? (double(hit) * 100.0 / double(cand)) : 0.0);

        printf("  %-22s", "资格拒绝归因");

        for (size_t i = 1; i < static_cast<size_t>(CacheRejectReason::Count); ++i)
        {
            printf(" %s=%llu", GetCacheRejectReasonName(static_cast<CacheRejectReason>(i)),
                   static_cast<unsigned long long>(Delta(now.reject[i], ledger_prev.reject[i])));
        }
        printf("\n");

        // ── 拓扑末次变更帧（0 = 从未变更 ⇒ '-'）──────────────────────────
        printf("  %-22s", "拓扑末次变更帧");

        for (size_t i = 0; i < static_cast<size_t>(TopologyChange::Count); ++i)
        {
            const TopologyChange kind = static_cast<TopologyChange>(i);
            const uint32_t      f     = ledger.GetTopologyLastFrame(kind);

            // `frame_serial` 从 1 开始（`BeginFrame` 先自增）⇒ 帧号 0 表示
            // 「变更发生在第一帧渲染之前」（Init 期建场景）。这类变更同样真实发生，
            // 不能用 '-' 表示「从未变更」，故用代数区分。
            if (f > 0)
                printf(" %s=#%u", GetTopologyChangeName(kind), f);
            else if (ledger.GetTopologyRevision(kind) > 0)
                printf(" %s=#<init>", GetTopologyChangeName(kind));
            else
                printf(" %s=-", GetTopologyChangeName(kind));
        }
        printf("  ｜ 当前帧序号=%u\n", ledger.GetFrameSerial());

        ledger_prev = now;
    }

    // ─────────────────────────────────────────────────────────────────────
    // P2：主帧拓扑快照报表（只观测 —— 不改变任何绘制行为）
    //
    // 达标口径（P2 验收）：
    //   · 稳态窗口 `命中 / (命中+未命中)` ≈ 100% —— 代数不变且内容不变，
    //     P3 才可能真的跳过收集；
    //   · **对拍不一致 = 0** —— 这是 P2 唯一的硬告警。非 0 表示有系统改了
    //     成员集合/批归属却没上报代数，P3 一旦据此跳过就是静默丢绘制；
    //   · `构建/帧` ≈ 1.0 —— 大于 1 说明不止一个 pass 抢当主帧
    //     （阴影/移动性过滤 pass 已排除，剩下的可疑对象是离屏 RTT pass）。
    // ─────────────────────────────────────────────────────────────────────
    struct SnapshotCounters
    {
        uint64_t builds          = 0;
        uint64_t commits         = 0;
        uint64_t aborts          = 0;
        uint64_t invalidations   = 0;
        uint64_t hits            = 0;
        uint64_t misses          = 0;
        uint64_t parity_mismatch = 0;
        uint64_t empty_builds    = 0;
    };

    SnapshotCounters snapshot_prev;

    void ReportMainFrameSnapshot(uint32_t window_frames)
    {
        if (!ecs_context)
            return;

        const MainFrameTopologySnapshot & snapshot = ecs_context->GetMainFrameTopologySnapshot();
        const SnapshotStats & s = snapshot.GetStats();

        const double div = (window_frames > 0) ? double(window_frames) : 1.0;

        auto Delta = [](uint64_t cur, uint64_t prev) -> uint64_t {
            return (cur >= prev) ? (cur - prev) : 0;
        };

        const uint64_t builds   = Delta(s.builds,          snapshot_prev.builds);
        const uint64_t commits  = Delta(s.commits,         snapshot_prev.commits);
        const uint64_t aborts   = Delta(s.aborts,          snapshot_prev.aborts);
        const uint64_t inval    = Delta(s.invalidations,   snapshot_prev.invalidations);
        const uint64_t hits     = Delta(s.hits,            snapshot_prev.hits);
        const uint64_t misses   = Delta(s.misses,          snapshot_prev.misses);
        const uint64_t mismatch = Delta(s.parity_mismatch, snapshot_prev.parity_mismatch);
        const uint64_t empty    = Delta(s.empty_builds,    snapshot_prev.empty_builds);

        const double hit_pct = (hits + misses > 0)
                             ? double(hits) * 100.0 / double(hits + misses)
                             : 0.0;

        printf("  -- P2 主帧拓扑快照（只观测，不参与跳过决策）--\n");

        printf("  %-22s 状态=%s 成员=%d 批=%d 代数=%llu 内容=%016llx 构建帧序号=%u\n",
               "快照内容",
               GetSnapshotStateName(snapshot.GetState()),
               snapshot.GetItemCount(), snapshot.GetBatchCount(),
               static_cast<unsigned long long>(snapshot.GetTopologyRevision()),
               static_cast<unsigned long long>(snapshot.GetContentHash()),
               snapshot.GetBuiltFrameSerial());

        printf("  %-22s 构建=%llu 提交=%llu 中断=%llu 失效=%llu ｜ 构建/帧=%.2f\n",
               "构建与提交（窗口）",
               static_cast<unsigned long long>(builds),
               static_cast<unsigned long long>(commits),
               static_cast<unsigned long long>(aborts),
               static_cast<unsigned long long>(inval),
               double(builds) / div);

        printf("  %-22s 命中=%llu 未命中=%llu (%.1f%%) ｜ 空快照=%llu\n",
               "命中率（窗口）",
               static_cast<unsigned long long>(hits),
               static_cast<unsigned long long>(misses), hit_pct,
               static_cast<unsigned long long>(empty));

        printf("  %-22s 对拍不一致=%llu ｜ %s\n",
               "拓扑/内容对拍",
               static_cast<unsigned long long>(mismatch),
               (mismatch == 0) ? "OK（台帐覆盖全部拓扑写者）"
                               : "<- 有未上报的拓扑写者！");

        snapshot_prev.builds          = s.builds;
        snapshot_prev.commits         = s.commits;
        snapshot_prev.aborts          = s.aborts;
        snapshot_prev.invalidations   = s.invalidations;
        snapshot_prev.hits            = s.hits;
        snapshot_prev.misses          = s.misses;
        snapshot_prev.parity_mismatch = s.parity_mismatch;
        snapshot_prev.empty_builds    = s.empty_builds;
    }

    // ─────────────────────────────────────────────────────────────────────
    // P3：主帧收集快路径报表
    //
    // 读法（照这个顺序排查）：
    //   · `接管/帧` —— 快路径完整接管的主帧数 / 窗口帧数。稳态应为 1.0。
    //     稳态却 < 1.0 ⇒ 前提在反复不成立，按下面顺序逐个查：
    //       台帐代数在变（有拓扑写者） → GPU 剔除没接管 → 快照成员数 != 收集数
    //       → `last_main_collect_steady` 为假（材质链还没就绪）
    //   · `回退` —— 判据通过后，又在重建途中发现不一致（实体没了 / 几何组件没了 /
    //     变换没了 / 材质共享行被回收）。**非 0 就是告警**：明明"拓扑代数未变"，
    //     却有成员消失 ⇒ 台帐漏了一个拓扑写者。
    // ─────────────────────────────────────────────────────────────────────
    struct CollectCacheCounters
    {
        uint64_t hits      = 0;
        uint64_t fallbacks = 0;
    };

    CollectCacheCounters collect_cache_prev;

    void ReportCollectCache(uint32_t window_frames)
    {
        if (!ecs_context || !collect_cache)
            return;

        auto system = ecs_context->GetSystem<RenderPrimitiveCollectSystem>();

        if (!system)
            return;

        const uint64_t hits      = system->GetCollectCacheHits();
        const uint64_t fallbacks = system->GetCollectCacheFallbacks();

        const uint64_t d_hits      = (hits >= collect_cache_prev.hits) ? (hits - collect_cache_prev.hits) : 0;
        const uint64_t d_fallbacks = (fallbacks >= collect_cache_prev.fallbacks) ? (fallbacks - collect_cache_prev.fallbacks) : 0;

        const double div = (window_frames > 0) ? double(window_frames) : 1.0;

        printf("  -- P3 主帧收集快路径（真正跳过收集主循环）--\n");
        printf("  %-22s 接管=%llu (%.2f/帧) 回退=%llu ｜ 成员=%llu 稳态=%s\n",
               "窗口统计",
               static_cast<unsigned long long>(d_hits), double(d_hits) / div,
               static_cast<unsigned long long>(d_fallbacks),
               static_cast<unsigned long long>(system->GetCollectCacheLastItems()),
               system->IsLastMainCollectSteady() ? "yes" : "no");
        printf("  %-22s %s\n",
               "诊断",
               (d_fallbacks == 0) ? "OK（快照成员与实体状态一致）"
                                  : "<- 有回退：台帐可能漏了拓扑写者");

        collect_cache_prev.hits      = hits;
        collect_cache_prev.fallbacks = fallbacks;
    }

    // ─────────────────────────────────────────────────────────────────────
    // T8.2 采样：GPU 侧可见数 / 溢出槽（会在采样帧付出一次 fence 等待）
    // ─────────────────────────────────────────────────────────────────────
    void SampleGPU(GPUCullSystem * gpu_cull)
    {
        // 契约（§9 M6）：Map 之前必须 Sync；未同步时 Map 会拒绝返回。
        //
        // 口径隔离：`SyncForParityReadback()` 会等 GPU 跑完**整帧**（fence），
        // 它既不是渲染成本也不是应用成本 —— 单独记 `SampleReadback` 槽；
        // SampleGPU 本身在渲染提交之后调用 ⇒ 不污染任何其它槽。
        uint32_t batch_count = 0;
        uint32_t entities    = 0;
        uint32_t parts       = 0;
        uint32_t overflow    = 0;

        {
            FrameProfileSpan readback_span(FrameProfileSlot::SampleReadback);

            if (!gpu_cull->SyncForParityReadback())
                return;

            batch_count = gpu_cull->GetBatchCount();
            entities    = gpu_cull->GetGPUVisibleCount();

            if (const uint32_t * counters = gpu_cull->MapCurrentBatchCounters(RenderPassSlot::Main))
            {
                for (uint32_t b = 0; b < batch_count; ++b)
                    parts += counters[b];

                overflow = counters[batch_count];   // 块内溢出槽（与 GLSL 同口径）
                gpu_cull->UnmapCurrentBatchCounters();
            }
        }

        const double avg_frame = (acc_frames > 0) ? acc_frame_ms / double(acc_frames) : 0.0;
        const double fps       = (avg_frame > 0.0) ? 1000.0 / avg_frame : 0.0;

        // 归因口径：FrameProfiler 的**顶层**槽互不重叠，可直接相加 ⇒
        // 「未归因」= 帧总时间 − Σ顶层槽（应接近 0；偏大 ⇒ 仍有段落没打点）。
        const uint64_t prof_samples = FrameProfiler::SampleCount();
        const double   prof_frames  = double(prof_samples);
        const double   prof_sum_ms  = (prof_frames > 0.0)
                                    ? double(FrameProfiler::SumUs()) / prof_frames / 1000.0 : 0.0;

        auto SlotMs = [prof_frames](FrameProfileSlot slot) -> double {
            return (prof_frames > 0.0) ? double(FrameProfiler::GetUs(slot)) / prof_frames / 1000.0 : 0.0;
        };
        auto CountAvg = [prof_frames](FrameProfileCount slot) -> double {
            return (prof_frames > 0.0) ? double(FrameProfiler::GetCount(slot)) / prof_frames : 0.0;
        };

        const double avg_tick = SlotMs(FrameProfileSlot::AppTick);

        printf("[CSTRESS] f=%u  帧=%.2fms(%.1f fps)  ECS+Jolt=%.2fms  渲染≈%.2fms"
               "｜可见实体=%u 可见部件=%u ICB=%u 溢出槽=%u\n",
               frame_seen, avg_frame, fps, avg_tick,
               (avg_frame > avg_tick) ? (avg_frame - avg_tick) : 0.0,
               entities, parts, batch_count, overflow);

        if (profile_breakdown && prof_frames > 0.0)
        {
            const double pct_base = (prof_sum_ms > 0.0) ? prof_sum_ms : 1.0;

            auto Line = [pct_base](const char * name, double ms, const char * note) {
                printf("  %-24s %7.3f ms (%5.1f%%)  %s\n", name, ms, ms / pct_base * 100.0, note);
            };

            printf("[CSTRESS-PROFILE] 窗口=%llu 帧  Σ归因=%.3f ms/帧  丢弃样本=%llu\n",
                   static_cast<unsigned long long>(prof_samples), prof_sum_ms,
                   static_cast<unsigned long long>(FrameProfiler::DroppedSamples()));

            // ── 顶层槽（互不重叠，可直接相加）──────────────────────────────
            Line("ScenePrePass",       SlotMs(FrameProfileSlot::ScenePrePass),       "主光 CSM 阴影全流程");
            Line("AcquireSwapchain",   SlotMs(FrameProfileSlot::AcquireSwapchain),   "WSI 取图（含等上一帧 GPU）");
            Line("PreBeginFrame",      SlotMs(FrameProfileSlot::PreBeginFrame),      "PreBegin 相位 + viewport + 纹理上传");
            Line("RenderCoreBegin",    SlotMs(FrameProfileSlot::RenderCoreBegin),    "render_core->BeginFrame");
            Line("TransformSubmit",    SlotMs(FrameProfileSlot::TransformSubmit),    "变换上传提交");
            Line("CameraPublish",      SlotMs(FrameProfileSlot::CameraPublish),      "相机行发布");
            Line("CollectPhase",       SlotMs(FrameProfileSlot::CollectPhase),       "RenderCollect 相位总耗时");
            Line("SyncWorldAddresses", SlotMs(FrameProfileSlot::SyncWorldAddresses), "世界地址同步");
            Line("BatchPhase",         SlotMs(FrameProfileSlot::BatchPhase),         "RenderBatch 相位总耗时");
            Line("BufferCommit",       SlotMs(FrameProfileSlot::BufferCommit),       "缓冲提交（描述符/范围）");
            Line("BufferUpload",       SlotMs(FrameProfileSlot::BufferUpload),       "缓冲上传（DMA）");
            Line("FrameSync",          SlotMs(FrameProfileSlot::FrameSync),          "GPU 剔除派发与 BeginPass");
            Line("BeginRenderPass",    SlotMs(FrameProfileSlot::BeginRenderPass),    "render_core->BeginRenderPass");
            Line("DrawRecord",         SlotMs(FrameProfileSlot::DrawRecord),         "绘制命令录制");
            Line("EndFrame",           SlotMs(FrameProfileSlot::EndFrame),           "render_core->EndFrame");
            Line("SubmitPresent",      SlotMs(FrameProfileSlot::SubmitPresent),      "vkQueueSubmit + 呈现（含 VSync）");
            Line("AppTick",            SlotMs(FrameProfileSlot::AppTick),            "应用 Tick（ECS + Jolt）");
            Line("SampleReadback",     SlotMs(FrameProfileSlot::SampleReadback),     "采样 fence 等待（不计入渲染）");

            const double unattributed = avg_frame - prof_sum_ms;

            printf("  %-24s %7.3f ms          %s\n", "未归因", unattributed,
                   (unattributed > 1.0) ? "<- 仍有未打点段落" : "OK");

            // ── 子槽：**包含于** CollectPhase / BatchPhase，不可再相加 ──────
            printf("  -- 子槽（已含在上面的总段内，勿重复相加）--\n");
            Line("|- CollectPreScan",     SlotMs(FrameProfileSlot::CollectPreScan),     "材质/管线预扫");
            Line("|- CollectMainLoop",    SlotMs(FrameProfileSlot::CollectMainLoop),    "判据循环 + item 装配 + 4-ID/事实发布");
            Line("|- CollectPublish",     SlotMs(FrameProfileSlot::CollectPublish),     "收集尾部诊断/提交");
            Line("|- CollectCacheReplay",  SlotMs(FrameProfileSlot::CollectCacheReplay), "P3 快路径：按拓扑快照重建 item + 事实");
            Line("|- BatchBuildMaterial", SlotMs(FrameProfileSlot::BatchBuildMaterial), "BuildMaterialBatches");
            Line("|- BatchGPUCullGuard",  SlotMs(FrameProfileSlot::BatchGPUCullGuard),  "TryApplyGPUCullOverride（含于 BatchFinalize）");
            Line("|- BatchFinalize",      SlotMs(FrameProfileSlot::BatchFinalize),      "FinalizeBatches（sort/聚簇/桶布局）");

            printf("  -- 每帧计数 -- 收集调用=%.1f 判据图元=%.0f 进批项=%.0f 材质批=%.1f 几何批=%.1f\n",
                   CountAvg(FrameProfileCount::CollectCalls),
                   CountAvg(FrameProfileCount::CollectPrimitives),
                   CountAvg(FrameProfileCount::CollectItems),
                   CountAvg(FrameProfileCount::Batches),
                   CountAvg(FrameProfileCount::BucketGroups));

            ReportTopologyLedger(acc_frames);
            ReportMainFrameSnapshot(acc_frames);
            ReportCollectCache(acc_frames);
        }
        fflush(stdout);

        acc_frame_ms        = 0.0;
        acc_frames          = 0;
        FrameProfiler::Reset();
    }

public:

    ~ChessboardStressApp()
    {
        // cube_geometry 由 GeometryAssetRegistry 持有，此处不再 delete。
    }

    void OnRenderPass(double delta_time) override
    {
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.06f, 0.07f, 0.10f, 1.0f));

        ecs_context = GetECSContext();
        if (!ecs_context)
        {
            printf("[ERROR] Init: no ECSContext\n");
            return false;
        }

        static_scene = getenv("CSTRESS_STATIC") != nullptr;
        no_physics   = getenv("CSTRESS_NO_PHYSICS") != nullptr;

        if (const char * v = getenv("CSTRESS_ORBIT"))   orbit_speed       = (float)atof(v);
        if (const char * v = getenv("CSTRESS_SAMPLE"))  sample_interval   = (uint32_t)atoi(v);
        if (const char * v = getenv("CSTRESS_PROFILE")) profile_breakdown = (atoi(v) != 0);
        if (const char * v = getenv("CSTRESS_TOPO"))    topo_snapshot     = (atoi(v) != 0);
        if (const char * v = getenv("CSTRESS_COLLECT_CACHE")) collect_cache = (atoi(v) != 0);

        if (const char * v = getenv("CSTRESS_BOARDS")) board_count = (uint32_t)atoi(v);
        if (const char * v = getenv("CSTRESS_DICE"))   dice_count  = (uint32_t)atoi(v);

        if (const char * tag = getenv("CSTRESS_DUMP")) dump_tag = tag;
        if (const char * v   = getenv("CSTRESS_FRAMES")) run_frames = (uint32_t)atoi(v);

        if (getenv("CSTRESS_SELFCHECK"))
        {
            selfcheck = true;

            // 摘要判据必须建在**冻结 + 定档**的场景上；缺省项自动补齐。
            if (!static_scene)
            {
                static_scene = true;
                printf("[CSTRESS] 加固：CSTRESS_SELFCHECK=1 但未给 CSTRESS_STATIC=1"
                       " ⇒ 已**自动冻结相机与落体**（摘要判据需确定取景）。\n");
            }
            if (board_count != kDefaultBoards || dice_count != kDefaultDice)
            {
                printf("[CSTRESS] 加固：自检模式强制回默认档 %u×%zu + %u。\n",
                       kDefaultBoards, PART_COUNT, kDefaultDice);
                board_count = kDefaultBoards;
                dice_count  = kDefaultDice;
            }
        }

        ResolveScale();

        if (!InitMaterial())          return false;
        if (!CreateCubeGeometry())    return false;
        if (!RegisterBoardTemplate()) return false;
        if (!no_physics && !InitPhysics()) return false;
        if (!InitScene())             return false;
        if (!InitCamera())            return false;
        if (!InitCull())              return false;

        // P2：主帧拓扑快照（默认关闭）。开启只增加「构建 + 对拍 + 报表」，
        // **不改变任何绘制结果** —— 这正是 P2 允许在实验开关下落地的原因。
        // 两个开关等价：本示例的 `CSTRESS_TOPO` 与引擎侧的 `ULRE_MAINFRAME_TOPO_CACHE`
        // （后者在世界构造时读取）；任一打开即开，报表口径保持一致。
        topo_snapshot = topo_snapshot || ecs_context->IsMainFrameTopologySnapshotEnabled();
        ecs_context->SetMainFrameTopologySnapshotEnabled(topo_snapshot);

        if (topo_snapshot)
        {
            printf("[CSTRESS] 主帧拓扑快照：开启（P2 观测模式 —— 只构建/对拍，不跳过原路径）\n");
            fflush(stdout);
        }

        // P3：主帧收集快路径。**依赖 P2 快照**（快照就是快路径的输入），
        // 所以只要快照没开，快路径一律强制关闭 —— 不做「有开关无数据」的假配置。
        collect_cache = collect_cache || ecs_context->IsMainFrameCollectCacheEnabled();
        if (collect_cache && !topo_snapshot)
        {
            printf("[CSTRESS] 主帧收集快路径：拒绝开启（依赖 P2 拓扑快照，请同时开 CSTRESS_TOPO=1）\n");
            fflush(stdout);
            collect_cache = false;
        }
        ecs_context->SetMainFrameCollectCacheEnabled(collect_cache);

        if (collect_cache)
        {
            printf("[CSTRESS] 主帧收集快路径：开启（P3 —— 稳态时跳过收集主循环，仅重建 item + 重发事实）\n");
            fflush(stdout);
        }

        // 剖析口径由引擎内的 `FrameProfiler`（真实调用边界打点）提供，
        // 本示例只在 Tick 里登记 `AppTick` 并累加帧间距，不再注册任何 phase 探针：
        // 探针只能观测「同 phase 内已注册系统之后」的时点，阶段名会系统性错配。
        if (profile_breakdown)
            FrameProfiler::SetEnabled(true);
        else
            FrameProfiler::SetEnabled(false);   // 显式关：连 channels.perf 也不看

        // 预热一帧：让首个窗口不把初始化期的异常帧算进去
        FrameProfiler::Reset();

        last_frame_us = GetUptimeUs();

        return true;
    }

    void Tick(double delta_time) override
    {
        // 冻结模式：相机与落体都定住（帧 8 的画面 = Init 姿态）⇒ 摘要判据可钉像素。
        if (!static_scene)
        {
            time += delta_time;

            if (ecs_context && camera_entity)
            {
                auto * camera = ecs_context->GetCameraByEntity(camera_entity);
                if (camera)
                {
                    camera->yaw          += orbit_speed * float(delta_time);
                    camera->matrix_dirty  = true;
                }
            }
        }

        // ── 计时：只累加帧间距；各段耗时全部由 FrameProfiler 在引擎内登记 ──
        const uint64_t now_us = GetUptimeUs();

        // 与上一帧 tick_start 的差 = 整帧墙钟（含本次 Render）。首个样本丢弃
        // （last_frame_us 为初始化时点）。
        if (last_frame_us > 0 && now_us > last_frame_us)
        {
            acc_frame_ms += double(now_us - last_frame_us) / 1000.0;
            ++acc_frames;
            FrameProfiler::CountSample();
        }

        last_frame_us = now_us;

        {
            FrameProfileSpan tick_span(FrameProfileSlot::AppTick);

            WorkObject::Tick(delta_time);
        }

        ++frame_seen;

        auto gpu_cull = ecs_context ? ecs_context->GetSystem<GPUCullSystem>() : nullptr;

        if (gpu_cull && gpu_cull->IsDispatchSettled())
        {
            if (!batch_verified)
                VerifyBatching(gpu_cull.get());

            if (sample_interval > 0 && (frame_seen % sample_interval) == 0)
                SampleGPU(gpu_cull.get());
        }

        // CSTRESS_DUMP=<tag>：第 8 帧帧内回读落原始字节后退出（同 GPUDrivenCull 先例）。
        if (!dump_tag.empty() && ++dump_frame == 8)
        {
            auto * rt = dynamic_cast<graph::SwapchainRenderTarget *>(ecs_context->GetRenderTarget());
            if (!rt)
            {
                printf("[CSTRESSDUMP] no swapchain RT\n");
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
                    printf("[CSTRESSDUMP] readback failed\n");
                    std::exit(2);
                }

                char name[256];
                snprintf(name, sizeof(name), "cstress_%s_%ux%u_%s.raw",
                         tag.c_str(), info.width, info.height,
                         GetVulkanFormat(info.format) ? GetVulkanFormat(info.format)->name : "unknown");

                FILE * fp = fopen(name, "wb");
                if (!fp)
                {
                    printf("[CSTRESSDUMP] open %s failed\n", name);
                    std::exit(2);
                }

                fwrite(pixels.data(), 1, pixels.size(), fp);
                fclose(fp);
                printf("[CSTRESSDUMP] wrote %s (%zu bytes)\n", name, pixels.size());
                std::exit(0);
            });
        }

        // CSTRESS_SELFCHECK=1：帧摘要自检（第 8 帧）；PASS 退 0 / FAIL 退 1。
        if (selfcheck && ++selfcheck_frame == 8)
        {
            auto * rt = dynamic_cast<graph::SwapchainRenderTarget *>(ecs_context->GetRenderTarget());
            if (!rt)
            {
                printf("[CSTRESSSELF] no swapchain RT\n");
                std::exit(2);
            }

            ulre_example::FrameDigest expect;
            expect.width                = kSelfW;
            expect.height               = kSelfH;
            expect.distinct_byte_values = kSelfMinDistinct;
            expect.fnv1a64              = kSelfFNV;

            ulre_example::InstallFrameDigestHook(rt, expect, "CSTRESSSELF");
        }

        // CSTRESS_FRAMES=N：定帧退出（CI / 像素对照）
        if (run_frames > 0 && frame_seen >= run_frames)
        {
            printf("[CSTRESS] done frames=%u\n", frame_seen);
            fflush(stdout);
            std::exit(0);
        }
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<ChessboardStressApp>(
        OS_TEXT("GPU Model Stress: 729 template boards (4 parts) + 350 dynamic dice"),
        argc, argv, 1280, 720);
}
