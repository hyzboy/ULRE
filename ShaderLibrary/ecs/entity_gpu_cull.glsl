#version 460
// @ulre begin
// @ulre name entity_gpu_cull
// @ulre kind Utility
// @ulre priority 0
// @ulre end
// GPU 视锥剔除着色器（T3d-1a）
// 输入：entity_gpu 表（世界私有，按 EntityID.index 索引）
//       L2W 矩阵池（世界私有，按帧槽切份）
//       相机信息（世界私有）
//       几何 AABB 池（全局）
// 输出：可见实体索引表 + 间接绘制命令表
//
// ⚠ `#version` 必须在**第 1 行**：引擎 GLSLCompiler 插件按"第 1 行是否 #version"
// 决定是否注入版本头，`@ulre` 注释块把它挤下去 ⇒ 注入后出现第二个 #version ⇒
// `'#version' : must occur first in shader`。
#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable
#extension GL_GOOGLE_include_directive : enable

#include "common/descriptor_macros.glsl"
#include "ubo/scene_ubo.glsl"
#include "ecs/entity_gpu.glsl"

// Push constants（per-dispatch，由引擎设置）
layout(push_constant) uniform PushConstants
{
    uint64_t addr_world_addresses;  // 世界地址表基址
    uint64_t addr_global_addresses; // 全局地址表基址
    uint64_t visible_indices_addr;  // 可见实体索引表地址
    uint64_t indirect_commands_addr;// 间接绘制命令表地址
    uint64_t draw_count_addr;       // 可见计数地址
    uint entity_count;              // 实体总数
    uint max_visible_count;         // 最大可见实体数
    uint camera_row;                // 本 pass 活跃相机行号（与 pc_root.camera_row 同式）
    uint mesh_group_count_x;        // 每条间接命令的 X 组数（绘制接管=T3f 时按几何给）
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
// 引擎约定：groupCountX = 组数（顶点数/组大小），groupCountY = 实例数，groupCountZ = 1
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
    // 行镜像复用 RenderItemResolve.glsl 的 RenderItemDescriptor / RenderItemBufferRef
    //（16 B uvec4 形状的唯一真源；表与 addr_global_render_items 同结构、不同索引空间）。
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

    // 写入可见实体索引
    VisibleEntityIndicesRef visible_indices = VisibleEntityIndicesRef(pc.visible_indices_addr);
    visible_indices.indices[visible_index] = entity_index;

    // 写入间接绘制命令（VkDrawMeshTasksIndirectCommandEXT 格式）
    // X = 组数（CPU 按几何给；剔除计数验证阶段 0 = 只出计数不出绘制）
    // Y = 实例数（本管线每可见实体一条命令 ⇒ 1）；Z = 1（引擎约定）
    DrawIndirectCommandsRef indirect_commands = DrawIndirectCommandsRef(pc.indirect_commands_addr);
    indirect_commands.commands[visible_index] = DrawMeshTasksIndirectCommand(
        pc.mesh_group_count_x,
        1u,
        1u
    );
}
