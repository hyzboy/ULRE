#version 460
// @ulre begin
// @ulre name entity_gpu_cull
// ⚠ `ULRE_ARENA_PASSES` 必须与 C++ `GPUCullSystem::kArenaPasses` 同值（门
//   `B.batch-info-struct-parity` 兜底）；批行/计数的块行数经 push constant 下发
//   （`pc.batch_block_stride`），不在此硬编码。
#define ULRE_ARENA_PASSES 2u
// @ulre kind Utility
// @ulre priority 0
// @ulre end
// GPU 视锥剔除着色器（T3d-1a；T3d-3a 判据修正；T3f-2 形态 B gather 输出）
// 输入：实体行表（枚举源，Active/Visible 位）+ 实体 4-ID 表（池口径 id 唯一来源）
//       L2W 矩阵池（世界私有）/ 相机（世界私有）/ GeometryAABB + MeshDrawParams 池（全局）
// 输出：可见实体索引表（形态 A 回灌读）+ 幸存计数；
//       另按幸存 slot gather 写四张 per-batch
//       行表：icb{X,Y=1,Z=1} / cmds{geometry_id, first_instance=slot} /
//       l2w_index{EntityGPU.row[Transform]} / mtl_rows{payload,texture,
//       entity.shadow_bias_multiplier,entity.shadow_receive==0?NoReceive:0}。
//       **全部字段读真源、零复制**（X 从 MeshDrawParams 池行现算；材质两位来自
//       实体 4-ID 表；阴影两位来自实体行事实位——collect 按 CPU D3 同式归一上行，
//       gather 逐字节照抄，T3h 起 guard 不再要求阴影全默认）。
//
// ⚠ `#version` 必须在**第 1 行**：引擎 GLSLCompiler 插件按"第 1 行是否 #version"
// 决定是否注入版本头，`@ulre` 注释块把它挤下去 ⇒ 注入后出现第二个 #version ⇒
// `'#version' : must occur first in shader`。
#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable

#include "common/descriptor_macros.glsl"
#include "ubo/scene_ubo.glsl"
#include "ecs/entity_gpu.glsl"

// Push constants（per-dispatch，由引擎设置）
// ⚠ 与 C++ `CullPushConstants` **逐字段同序同型**（u64 全部前置，u32 收尾）：
// 布局漂移 = 读错字段 = 静默剔错（§8.2b③；push constant 不经描述符 schema，门抓不到）。
layout(push_constant) uniform PushConstants
{
    uint64_t addr_world_addresses;   // 世界地址表基址（本帧槽）
    uint64_t addr_global_addresses;  // 全局地址表基址
    uint64_t visible_indices_addr;   // 可见实体索引表（形态 A 回灌读；两档恒写）
    uint64_t indirect_commands_addr; // icb（VkDrawMeshTasksIndirectCommandEXT ×12B）
    uint64_t l2w_out_addr;           // per-batch l2w_index 表（u32×4B/行）
    uint64_t cmds_out_addr;          // per-batch mesh 参数表（MeshDrawCommand 8B/行）
    uint64_t mtl_out_addr;           // per-batch 材质行表（MaterialInstanceAddresses 16B/行）
    uint64_t draw_count_addr;        // 幸存计数
    uint64_t batch_tag_addr;         // T3i：批标签数组（当前帧槽；0xFFFFFFFF = 不属任何接管批）
    uint64_t batch_info_addr;        // T3i：批分区表（{row_base, capacity} × batch_count）
    uint64_t batch_counter_addr;     // T3i：每批计数（[b] = 本批幸存数；[batch_count] = overflow）
    uint   entity_count;             // 实体总数
    uint   max_visible_count;        // 幸存表容量上限
    uint   camera_row;               // 本 pass 活跃相机行号（与 pc_root.camera_row 同式）
    uint   batch_count;              // T3i：本帧主帧批数（0 = 无接管批）
    uint   batch_block_stride;       // T4c2：批行/计数每 pass 块的行数（= C++ kBatchBlockStride）
    uint   batch_count_shadow;       // 本帧阴影批数（0 = 无阴影接管批）
} pc;

// 世界地址表（通过 push constant 访问，覆盖 scene_ubo.glsl 里的定义）
#undef world_addresses
#undef global_addresses
#undef camera
#define world_addresses WorldAddressesRef(pc.addr_world_addresses)
#define global_addresses GlobalAddressesRef(pc.addr_global_addresses)
// camera 宏：与材质侧 `pc_root.camera_row` 同口径（行号 = 相机槽 × 帧槽总数 + 帧槽，
// 由 CPU `ECSContext::GetActiveCameraRow()` 算好下发）。写死 0 会读到别的相机的视锥。
#define camera CameraInfoBufferRef(world_addresses.addr_camera_info).cameras[pc.camera_row]

// 实体 4-ID 表的行镜像（T3d-3a）：**复用** RenderItemResolve.glsl 的 `RenderItemDescriptor`
// struct + `RenderItemBufferRef`（16 B uvec4 形状的唯一真源，不另造同类结构/镜像）。
// （2026-10-14：该模块只余形状定义，不再有引用 `world_addresses` 宏的函数体 ⇒
//  include 位置不再受宏展开时机约束；此前 "必须在上面的 pc 覆盖之后" 的告诫随解析函数一并作废。）
#include "common/RenderItemResolve.glsl"

// 行数组本体（与世界 RenderItemBufferRef 同结构、不同索引空间：这里按 EntityID.index）
#define entity_render_ids RenderItemBufferRef(world_addresses.addr_entity_render_ids).items

// L2W 矩阵池（世界级访问，按帧槽切份）
layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer LocalToWorldDataRef
{
    mat4 mats[];
};
#define l2w_world LocalToWorldDataRef(world_addresses.addr_l2w)

// 几何 AABB 池（全局）
layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer GeometryAABBDataRef
{
    vec4 center_extents[];  // xyz=center, w=外接球半径（<0 ⇒ 始终可见哨兵）; xyz=extents（连续存储）
};
#define geometry_aabbs GeometryAABBDataRef(global_addresses.addr_geometry_aabb)

// MeshDrawParams 池行镜像（T3f-2）：**只镜像头部前 9 字段**（X 列表 offset 0..32 连续，
// 算组数只读 is_indexed/index_count/vertex_count）；尾部 11×uint64 地址不进镜像。
// 步长必须 ==104（池行 sizeof 契约）⇒ scalar layout 下用显式 `_pad[68]` 填回
//（entity_gpu 镜像同纪律：GLSL 不隐式补 alignas，§7.6；9×4=36 载荷 + 92 填充 = 128）。
// ⚠ 与 X 列表 `HGL_MESH_DRAW_PARAMS_FIELD_LIST` 前 9 项逐字段同序同型，由门用例
// `E.mesh-draw-params-head-parity` 对拍兜底 —— 改 X 列表必须同步本镜像。
struct MeshDrawParamsHeadData
{
    uint index_base;
    uint vertex_base;
    uint is_indexed;
    uint index_count;
    uint vertex_count;
    uint data_index_count;
    uint data_vertex_count;
    float char_height;
    uint first_instance;
    uint8_t _pad[68];
};

layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer MeshDrawParamsHeadRef
{
    MeshDrawParamsHeadData rows[];
};
#define mesh_draw_params_pool MeshDrawParamsHeadRef(global_addresses.addr_mesh_draw_params_pool).rows

// 可见计数（原子递增）
layout(buffer_reference, scalar, buffer_reference_align=4) buffer DrawCountRef
{
    uint value;
};

// 输出：可见实体索引
layout(buffer_reference, scalar, buffer_reference_align=4) buffer VisibleEntityIndicesRef
{
    uint indices[];
};

// 输出：间接绘制命令（VkDrawMeshTasksIndirectCommandEXT 格式）
// 引擎约定：groupCountX = 组数（元素数/组大小），groupCountY = 实例数，groupCountZ = 1
struct DrawMeshTasksIndirectCommand
{
    uint groupCountX;
    uint groupCountY;
    uint groupCountZ;
};

layout(buffer_reference, scalar, buffer_reference_align=16) buffer DrawIndirectCommandsRef
{
    DrawMeshTasksIndirectCommand commands[];
};

// 输出（形态 B）：per-batch l2w_index 表。消费点 = 发射体 `ResolveTransformID(iid) →
// LocalToWorldIndexRef(pc_root.addr_l2w_index).values[iid] → l2w.mats[行号]`；
// 行值语义 = **L2W 行号**（CPU WriteBatchIndexRows 写 `item->transform_index` 同值）。
layout(buffer_reference, scalar, buffer_reference_align=4) buffer L2WIndexOutRef
{
    uint values[];
};

// 输出（形态 B）：per-batch mesh 参数行（CPU 端 MeshDrawCommand 8B：{geometry_id,
// first_instance}；消费点 `cmds[gl_DrawID]`，first_instance 进 gl_InstanceIndex 链）。
struct BatchCmdRow
{
    uint geometry_id;
    uint first_instance;
};

layout(buffer_reference, scalar, buffer_reference_align=4) buffer BatchCmdsOutRef
{
    BatchCmdRow cmds[];
};

// 输出（形态 B）：per-batch 材质行表（CPU 端 MaterialInstanceAddresses 16B 四字段；
// 消费点 = MTL_ROW/MTL_TEX 宏 `values[data_index]`）。阴影两位恒写 0 = 引擎默认
//（正常受影 / 倍率 1.0）——与 CPU 写行在"未挂 ShadowProxy"场景逐字节一致；
// 非默认实体由 CPU guard 挡在接管范围外。
struct MtlRow
{
    uint  payload_index;
    uint  texture_reference_index;
    float shadow_bias_multiplier;
    uint  shadow_flags;
};

layout(buffer_reference, scalar, buffer_reference_align=4) buffer MtlRowsOutRef
{
    MtlRow values[];
};

// ── T3i（③ 多批）输入：批标签 / 分区 / 每批计数 ─────────────────────────
// 三者由 CPU 按帧槽分份（只写当前份）；cull 在 RenderFrameSync ⇒ 读到的恒是本帧产物。
layout(buffer_reference, scalar, buffer_reference_align=4) readonly buffer BatchTagRef
{
    uint tags[];      // [entity_index] = 批号；0xFFFFFFFF = 不属任何接管批（跳过）
};

// 批的 dispatch 档（与 CPU `GPUCullSystem::kBatchTopology*` 同步）：决定本批的 X 公式
#define TOPOLOGY_TRIANGLES 0u   // 顶点组→三角形（VertexPassthrough）
#define TOPOLOGY_LINES     1u   // 段→quad（LineQuad；每线程 1 段 ⇒ 元素数按段数）

struct BatchInfoRow
{
    uint row_base;    // 本批切片在共享竞技场里的起始行
    uint capacity;    // 切片容量（= 本批 item 数；绘制侧 maxDrawCount）
    uint topology;    // TOPOLOGY_*（dispatch 档）
    uint group_size;  // 本批组大小（CPU 单源下发；不在 GLSL 硬编码）
};

layout(buffer_reference, scalar, buffer_reference_align=4) readonly buffer BatchInfoRef
{
    BatchInfoRow rows[];
};

layout(buffer_reference, scalar, buffer_reference_align=4) buffer BatchCountersRef
{
    uint counters[];  // [b] = 本批幸存数（= 绘制计数）；[batch_count] = overflow 计数
};

layout(local_size_x = 64) in;

void main()
{
    uint entity_index = gl_GlobalInvocationID.x;
    if (entity_index >= pc.entity_count)
        return;

    // 读取实体行
    EntityGPUData entity = entity_gpu_rows[entity_index];

    // 跳过非活跃实体
    if ((entity.flags & 1u) == 0u)  // Active 位
        return;

    // 跳过不可见实体
    if ((entity.flags & 2u) == 0u)  // Visible 位
        return;

    // 读取实体 4-ID 行（T3d-3a：**池口径**的唯一来源）
    //
    // ⚠ 为什么不能像早期版本那样用 `entity.row[2]` 当 geometry_id：那是 `Entity::
    // component_rows[Geometry]` = **组件池行号**（每实体一行），而 AABB 池与
    // MeshDrawParams 池按 **asset geometry_id** 索引（一号双表，每 asset 一行）⇒
    // 两者不同口径（PBRSpheres 实测：组件行 1..100 vs 池 id 1..10）。拿组件行去查池
    // = 越界/错借别人的 AABB ⇒ 剔除判据从根上就是错的（旧版 GPU 与 CPU 参考**错得
    // 同源**，所以 mismatch 仍为 0 —— 假绿，见设计稿 §8.2d）。
    //
    // 本表由 collect 在"本项进批"的同一判据点写入（世界唯一的 4-ID 发布点）
    // ⇒ 与消费侧 cmds/material rows 天然同口径。`geometry_id == 0` = 未上行
    // （行 0 哨兵惯例，含"实体无几何"与"行还没到"两种，都该跳过本帧）。
    const RenderItemDescriptor render_ids = entity_render_ids[entity_index];

    const uint geometry_id = render_ids.geometry_id;   // MeshDrawParams / GeometryAABB 池行

    if (geometry_id == 0u)
        return;

    // L2W 行号仍取实体行表的 `row[Transform]`（B1：变换侧写的**帧槽含基址**行号，
    // 那边已闭环；本表的 transform_id 是原始句柄、含帧槽换算未做 ⇒ 不用它索引 L2W）
    const uint transform_id = entity.row[1u];

    if (transform_id == 0u)
        return;

    // 读取相机信息
    CameraInfoData cam = camera;

    // 读取 L2W 矩阵
    mat4 l2w_matrix = l2w_world.mats[transform_id];

    // 读取几何 AABB（center 和 extents 连续存储）
    vec4 aabb_center = geometry_aabbs.center_extents[geometry_id * 2u];
    vec4 aabb_extents = geometry_aabbs.center_extents[geometry_id * 2u + 1u];

    // 变换 AABB 中心到世界空间
    vec3 world_center = (l2w_matrix * vec4(aabb_center.xyz, 1.0)).xyz;

    // 计算变换后的 extents（简化：取最大缩放）
    // 本式是 C++ 侧 `hgl::ecs::GetMaxWorldScaleRaw`（BoundingVolumeCull.h）的**镜像**：
    // 两边都不带退化保护（scale==0 就返回 0）—— 那正是 CPU 剔除参考能逐字对拍的前提。
    float scale = max(length(l2w_matrix[0].xyz), max(length(l2w_matrix[1].xyz), length(l2w_matrix[2].xyz)));
    vec3 world_extents = aabb_extents.xyz * scale;

    // 视锥剔除（6 个平面）。
    // ⚠ center.w = 外接球半径；**< 0 ⇒ 「始终可见」哨兵**（写者：`Geometry::RefreshGeometryAABBRow`
    //   无有效本地 AABB 时填 `GlobalSSBOBufferRegistry::kAlwaysVisibleRadius`=-1，
    //   语义 = 缺数据不得静默剔空物体——2D 几何（V2F 位置）全在这里被救回）。
    //   半径只被哨兵判定消费：extents 才是投影半径的来源（与 CPU 参考逐字同式）。
    bool visible = true;
    for (int i = 0; (aabb_center.w >= 0.0) && (i < 6); ++i)
    {
        vec4 plane = cam.frustum_planes[i];
        vec3 normal = plane.xyz;
        float distance = plane.w;

        // 计算 AABB 在平面法线上的投影半径
        float radius = abs(normal.x) * world_extents.x
                     + abs(normal.y) * world_extents.y
                     + abs(normal.z) * world_extents.z;

        // 计算中心到平面的有符号距离
        float center_distance = dot(normal, world_center) + distance;

        // 如果中心距离 < -半径，则完全在平面外侧
        if (center_distance < -radius)
        {
            visible = false;
            break;
        }
    }

    // ── 块 0（主帧 pass）的可见表：口径与单块时代**逐字一致**（不以标签为条件）──
    // T4c2 起块 1（阴影 pass）也要写行，而它不看可见性 ⇒ 不能用早退收工，改用它做块 0 的开关。
    bool cull_ok = visible;
    if (visible)
    {
        // 原子递增可见计数
        DrawCountRef draw_count = DrawCountRef(pc.draw_count_addr);
        const uint visible_index = atomicAdd(draw_count.value, 1u);

        // 检查是否超出最大可见数（超出：与单块时代同义——不进可见表、块 0 也不写行）
        if (visible_index >= pc.max_visible_count)
            cull_ok = false;
        else
        {
            // 写入可见实体索引（两档共用）
            VisibleEntityIndicesRef visible_indices = VisibleEntityIndicesRef(pc.visible_indices_addr);
            visible_indices.indices[visible_index] = entity_index;
        }
    }

    // ── T3i（③ 多批）：批标签 → 本批切片 → 写四表 ──────────────────────────
    // 标签 = CPU 批装配直写（当前帧槽；cull 在 RenderFrameSync ⇒ 本帧产物）。
    // 无效/越界 = 不属任何接管批 ⇒ **不写行**（该实体由 CPU 路径绘制），
    // 但仍计入全局幸存（对拍口径不变）。
    // T4c2 同族：标签表**按 pass 分块**（块步长 = 可见表上限，与实体 4-ID 表同口径）——
    // 两个 pass **各查自己那一块**：此前不带 pass 项 ⇒ 两 pass 共用标签，静止物体被按
    // 另一 pass 的批索引写行 ⇒ 挤进批 0（capacity=1）⇒ 切片溢出（计划 §11.9）。
    const uint batch0 = BatchTagRef(pc.batch_tag_addr).tags[0u * pc.max_visible_count + entity_index];
    const uint batch1 = BatchTagRef(pc.batch_tag_addr).tags[1u * pc.max_visible_count + entity_index];

    const bool tagged0 = (batch0 != 0xFFFFFFFFu && batch0 < pc.batch_count);
    const bool tagged1 = (batch1 != 0xFFFFFFFFu && batch1 < pc.batch_count_shadow);

    if (!tagged0 && !tagged1)
        return;      // 两个 pass 都不属任何接管批 ⇒ 不写行（该实体由 CPU 路径绘制）

    // ── T4c2：按 pass 两遍写行 ────────────────────────────────────────────
    //  块 0 = 主帧 pass：仍受视锥剔除（`cull_ok`）⇒ 写"幸存者"行；
    //  块 1 = 阴影 pass：**直通档**（不判可见性）⇒ 该批全部项写行。阴影图只看深度、
    //  绘制序无关，故不必搬"批内序号"上 GPU（省一层数据）。
    //  某 pass 未接管该批 ⇒ 该块 `capacity == 0` ⇒ 跳过（块由 CPU 每帧清零）。
    for (uint blk = 0u; blk < ULRE_ARENA_PASSES; ++blk)
    {
        if (blk == 0u && !cull_ok)
            continue;

        const uint cur_batch_count = (blk == 0u) ? pc.batch_count : pc.batch_count_shadow;
        const uint batch = (blk == 0u) ? batch0 : batch1;

        if (batch == 0xFFFFFFFFu || batch >= cur_batch_count)
            continue;

        const uint block_base = blk * pc.batch_block_stride;
        const BatchInfoRow binfo = BatchInfoRef(pc.batch_info_addr).rows[block_base + batch];

        if (binfo.capacity == 0u)
            continue;    // 本 pass 未接管该批（布局行未写）

        BatchCountersRef counters = BatchCountersRef(pc.batch_counter_addr);
        const uint slot = atomicAdd(counters.counters[block_base + batch], 1u);

        if (slot >= binfo.capacity)
        {
            // 3A：溢出 = 批归属与容量不同源（布局错配/越界）⇒ 跳过 + 计入**本块**溢出槽；
            // 台架把该计数当**硬判据**（非零即红）。
            atomicAdd(counters.counters[block_base + cur_batch_count], 1u);
            continue;
        }

        // 契约（§8.5；全部读真源、零复制）：
        //  · 行号 = 本批切片起点 + 切片内序号 ⇒ 每批 draw 只读自己的幸存者
        //  · cmds 的 first_instance = **切片内序号**（mesh 侧 gl_InstanceIndex =
        //    first_instance + wg.y；绘制侧 push 地址已带 row_base 偏移 ⇒ 切片内 0 起）
        //  · mtl 两位 = 实体行事实位逐字节照抄（T3h 口径不变）
        const uint row = binfo.row_base + slot;

        const MeshDrawParamsHeadData mdph = mesh_draw_params_pool[geometry_id];
        uint element_count = mdph.is_indexed != 0u ? mdph.index_count : mdph.vertex_count;
        if (binfo.topology == TOPOLOGY_LINES)
            element_count >>= 1u;   // 线：元素数 = **段数**（每线程 1 段；CPU 侧组数同式，组大小见 MeshDispatchGroupSize）
        const uint group_count_x = (element_count + binfo.group_size - 1u) / binfo.group_size;

        DrawIndirectCommandsRef(pc.indirect_commands_addr).commands[row] =
            DrawMeshTasksIndirectCommand(group_count_x, 1u, 1u);

        L2WIndexOutRef(pc.l2w_out_addr).values[row] = transform_id;

        BatchCmdsOutRef(pc.cmds_out_addr).cmds[row] =
            BatchCmdRow(geometry_id, slot);

        // ── T4c3b：4-ID 表按 pass 分半，本块必须读**本 pass 自己那一半** ────────
        // 行内容**不是 pass 无关**：材质/纹理二 ID 由 collect 按 pass 写（`SetRenderIDs`），
        // 阴影程序/材质行按 pass 解析 ⇒ 阴影块若照抄主帧那一半，解析到的是主帧的材质变体。
        // 行号 = `blk * 每 pass 容量 + entity_index`；4-ID 表 pass 容量与可见表上限同值
        //（`static_assert` 在 `GPUCullSystem.cpp` 守着），故不必再加 push 字段。
        const RenderItemDescriptor ids_blk =
            entity_render_ids[blk * pc.max_visible_count + entity_index];

        MtlRowsOutRef(pc.mtl_out_addr).values[row] =
            MtlRow(ids_blk.material_id,
                   ids_blk.texture_id,
                   entity.shadow_bias_multiplier,
                   entity.shadow_receive == 0u ? 1u : 0u);
    }
}
