#version 460
// @ulre begin
// @ulre name entity_gpu_cull
// @ulre kind Utility
// @ulre priority 0
// @ulre end
// GPU 视锥剔除着色器（T3d-1a；T3d-3a 判据修正；T3f-2 形态 B gather 输出）
// 输入：实体行表（枚举源，Active/Visible 位）+ 实体 4-ID 表（池口径 id 唯一来源）
//       L2W 矩阵池（世界私有）/ 相机（世界私有）/ GeometryAABB + MeshDrawParams 池（全局）
// 输出：可见实体索引表（形态 A 回灌读）+ 幸存计数；
//       `enable_gather != 0`（形态 B 接管中）时另按幸存 slot gather 写四张 per-batch
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
    uint   entity_count;             // 实体总数
    uint   max_visible_count;        // 幸存表容量上限
    uint   camera_row;               // 本 pass 活跃相机行号（与 pc_root.camera_row 同式）
    uint   mesh_group_size;          // mesh 组顶点上限（CPU 下发 kMeshVertexPassthroughMaxVertices）
    uint   enable_gather;            // 0 = 只出计数（对拍/形态 A）；非 0 = 写 gather 四表（形态 B）
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
// ⚠ include 位置 = 宏展开时机：该模块的**函数体**引用 `world_addresses` 宏 ⇒ 必须
// include 在上面的 pc 覆盖**之后**（否则宏还指向未声明的 pc_root ⇒ 'pc_root': undeclared，
// 症状 = compute pipeline is null + GPU count 恒 0）。
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
    vec4 center_extents[];  // xyz=center, w=unused; xyz=extents（连续存储）
};
#define geometry_aabbs GeometryAABBDataRef(global_addresses.addr_geometry_aabb)

// MeshDrawParams 池行镜像（T3f-2）：**只镜像头部前 9 字段**（X 列表 offset 0..32 连续，
// 算组数只读 is_indexed/index_count/vertex_count）；尾部 11×uint64 地址不进镜像。
// 步长必须 ==128（池行 sizeof 契约）⇒ scalar layout 下用显式 `_pad[92]` 填回
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
    uint8_t _pad[92];
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
    // 本表由 collect 在"本项进批"的同一判据点写入（四元组与 SetRenderItem4ID 逐字
    // 同源）⇒ 与消费侧 cmds/material rows 天然同口径。`geometry_id == 0` = 未上行
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
    float scale = max(length(l2w_matrix[0].xyz), max(length(l2w_matrix[1].xyz), length(l2w_matrix[2].xyz)));
    vec3 world_extents = aabb_extents.xyz * scale;

    // 视锥剔除（6 个平面）
    bool visible = true;
    for (int i = 0; i < 6; ++i)
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

    if (!visible)
        return;

    // 原子递增可见计数
    DrawCountRef draw_count = DrawCountRef(pc.draw_count_addr);
    uint visible_index = atomicAdd(draw_count.value, 1u);

    // 检查是否超出最大可见数
    if (visible_index >= pc.max_visible_count)
        return;

    // 写入可见实体索引（两档共用）
    VisibleEntityIndicesRef visible_indices = VisibleEntityIndicesRef(pc.visible_indices_addr);
    visible_indices.indices[visible_index] = entity_index;

    if (pc.enable_gather == 0u)
        return;

    // ── 形态 B（T3f-2）：按幸存 slot gather 四张 per-batch 行表 ──────────────
    // 契约（§8.3 清单；全部读真源、零复制）：
    //  · l2w_index[s] = EntityGPU.row[Transform]（L2W 行号，与 CPU 行表写值同语义）
    //  · cmds[s]      = { geometry_id(池), first_instance = s }
    //                   ⇒ mesh 侧 gl_InstanceIndex = s + wg.y → values[s] 全链 1:1
    //  · icb[s]       = { X = ceil(池行元素数 / mesh_group_size), Y = 1, Z = 1 }
    //                   （CPU CalcMeshGroupCount 同式；meshlet 分支由 CPU guard 排除）
    //  · mtl[s]       = { material_id, texture_id, entity.shadow_bias_multiplier,
    //                     entity.shadow_receive==0 ? kNoReceive : 0 }
    //                    （T3h：材质两位读实体 4-ID 表、阴影两位读实体行事实位——
    //                    全部**逐字节照抄 CPU 归一后的值**（collect 上行时已按
    //                    FinalizeBatch D3 同式归一：bias<=0 ⇒ 0 ⇒ 着色侧视 0 为
    //                    默认 1.0；receive ⇒ flags 0 / 不接收 ⇒ kMaterialShadowFlag
    //                    NoReceive=1）。CPU guard 的阴影前提已随之删除。）
    const MeshDrawParamsHeadData mdph = mesh_draw_params_pool[geometry_id];
    const uint element_count = mdph.is_indexed != 0u ? mdph.index_count : mdph.vertex_count;
    const uint group_count_x = (element_count + pc.mesh_group_size - 1u) / pc.mesh_group_size;

    DrawIndirectCommandsRef(pc.indirect_commands_addr).commands[visible_index] =
        DrawMeshTasksIndirectCommand(group_count_x, 1u, 1u);

    L2WIndexOutRef(pc.l2w_out_addr).values[visible_index] = transform_id;

    BatchCmdsOutRef(pc.cmds_out_addr).cmds[visible_index] =
        BatchCmdRow(geometry_id, visible_index);

    MtlRowsOutRef(pc.mtl_out_addr).values[visible_index] =
        MtlRow(render_ids.material_id,
               render_ids.texture_id,
               entity.shadow_bias_multiplier,
               entity.shadow_receive == 0u ? 1u : 0u);   // 1 = kMaterialShadowFlagNoReceive
}
