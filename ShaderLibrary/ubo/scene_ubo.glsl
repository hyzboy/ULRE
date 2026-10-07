// @ulre begin
// @ulre name scene_ubo
// @ulre kind Utility
// @ulre priority 0
// @ulre provide Camera
// @ulre provide SkyLight
// @ulre provide Viewport
// @ulre end
// 全局统一基础数据声明（**无绑定无集**）
//
// 本文件里的 UBO 声明已全部退场：CameraInfo / ColorPalette / GlobalAddresses 表本体 / sky /
// shadow / viewport 一律走 BDA —— 地址进 global_addresses 表（表基址经
// pc_root.addr_global_addresses 下发），按下标解引用。宏名与成员名保持不变
// ⇒ shader 正文与生成侧发射字符串零改动。

#ifndef HGL_SCENE_UBO_GLSL
#define HGL_SCENE_UBO_GLSL

#include "common/descriptor_macros.glsl"

#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

struct CameraInfoData
{
    mat4 projection;
    mat4 inverse_projection;
    mat4 view;
    mat4 inverse_view;
    mat4 vp;
    mat4 inverse_vp;
    vec4 frustum_planes[6];
    mat4 sky;
    vec3 pos;
    float _pad_pos;
    vec3 view_line;
    float _pad_vl;
    vec3 world_up;
    float _pad_wu;
    vec3 camera_facing_up;
    float _pad_cfu;
    vec3 camera_facing_right;
    float _pad_cfr;
    float znear, zfar;
    uint use_reversed_z;
    float _pad_ci0;
    vec3 camera_world_pos;
    float _pad_cwp;
};

layout(buffer_reference, scalar, buffer_reference_align=64) readonly buffer CameraInfoBufferRef
{
    CameraInfoData cameras[];
};

// sky（S2：地址进表，退出 Scene 集绑定）：sky 是单份 buffer ⇒ 所有帧槽同址。
// **`sky_provider` 槽的参数块，块内只含外观标量**（底色 / 光晕强度）：
//   · 光源参数（方向 / 颜色 / 强度 / 角径 / 阴影开关）已迁出到**光源行**
//（`light_rows[0]` = 主方向光，T6b）；
//   · IBL 句柄已归位到它真正的消费者槽 —— 见下方 `AmbientEnvInfoRef`
//（`ambient_light_provider`，T1）。
// 光晕**颜色**直接读主光色（不再单独成字段）。
// 块内同族字段 ⇒ 不再需要显式填充，也不存在句柄对齐分歧（R2）。
// 字段与 C++ `SkyInfo` 逐字段同序（`vec4 base_sky_color` / `float halo_intensity`）。
layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer SkyInfoRef
{
    vec4 base_sky_color;
    float halo_intensity;
};

#define sky SkyInfoRef(world_addresses.addr_sky)

// ambient_env（`ambient_light_provider` 槽的参数块；地址进世界表 ⇒ 单份 buffer，全帧槽同址）。
// C++ 真源 = `inc/hgl/graph/ubo/AmbientEnvInfo.h`；块内**只有句柄**（两端同型 `uvec4`）
// ⇒ 不需要填充、也不会有偏移分歧。消费者 = `lighting/indirect_ibl.glsl`。
layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer AmbientEnvInfoRef
{
    uvec4 env_tex;      // IBL 三个句柄：irradiance / prefilter / BRDF LUT（w = 预留）
};

#define ambient_env AmbientEnvInfoRef(world_addresses.addr_ambient_env)

// viewport（S2：地址进表 —— 单份 buffer，内容按 pass/RT 覆盖写、地址恒定 ⇒ 全帧槽同址）
// 退出 Scene 集绑定；宏名与成员名不变 ⇒ `viewport.*` 读点零改动。
layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer ViewportInfoRef
{
    mat4 ortho_matrix;
    uvec2 canvas_resolution;
    uvec2 viewport_resolution;
    vec2 inv_viewport_resolution;
};

#define viewport ViewportInfoRef(global_addresses.addr_viewport)

// 调色板（256 项 RGBA8 打包，unpackUnorm4x8 解码）：构造期一次写入、长期有效
// ⇒ 地址随地址表下发（不进 pc_root —— 后者只承载每 pass / 每帧变化的地址）。
// 宏名保持不变：shader 正文与生成侧发射的
//   unpackUnorm4x8(color_palette.color[ColorIndex]) 零改动。
layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer ColorPaletteRef
{
    uint color[256];
};

#define color_palette ColorPaletteRef(global_addresses.addr_color_palette)

// 全局地址表（**无绑定无集**）：表本体是 SSBO，基址经 pc_root.addr_global_addresses 下发。
// 宏名不变 ⇒ 所有 `global_addresses.addr_*` 读点（含生成侧发射的 MTL_ROW / camera 宏）
// 与 shader 正文零改动。
layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer GlobalAddressesRef
{
    uint64_t addr_mesh_draw_params_pool;   // 池基址；本批那一块在 pc_root.addr_batch_mesh_draw_params
    uint64_t addr_geometry_aabb;           // GeometryAABB 池基址（与 MeshDrawParams 同 ID 索引：行号 = geometry_id）
    uint64_t addr_pbr_surface;
    uint64_t addr_emissive_surface;
    uint64_t addr_transmission_surface;
    uint64_t addr_color_palette;
    // 每帧槽字段：表按 HGL_FRAME_SLOT_TOTAL 多份，pc_root.addr_global_addresses
    // 指向「本帧那一槽」，所以这个地址总是当前帧的数据。
    uint64_t addr_viewport;
};

#define global_addresses GlobalAddressesRef(pc_root.addr_global_addresses)

// 世界地址表（**无绑定无集**）：世界私有 SSBO 的地址表，基址经 pc_root.addr_world_addresses 下发。
// 为什么独立成表：表内都是**世界私有** buffer（相机行表 / 4-ID 渲染项表 / DrawItemID 表 /
// 本世界 profile 的 sky + shadow）。一个设备上可同时存在多个世界（主世界 + OffscreenWorld），
// 放进全局表只能表达"最后一个世界"的地址 ⇒ 多世界同帧互踩。
// 表本体与 GlobalAddresses 同形：HGL_FRAME_SLOT_TOTAL 份 × kWorldAddressesSlotStride，
// pc_root 指向"本帧那一槽"。
//（定稿见 doc/world-addresses-and-camera-model-plan.md §1/§4）
layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer WorldAddressesRef
{
    uint64_t addr_camera_info;          // 相机行表（行号 = 相机槽 × 帧槽总数 + 帧槽）
    uint64_t addr_global_render_items;  // 4-ID 渲染项表（世界私有）
    uint64_t addr_draw_item_ids;        // DrawItemID 压缩索引表（世界私有）
    uint64_t addr_sky;                  // 本世界 profile 的 SkyInfo（单份 ⇒ 全帧槽同址）
    uint64_t addr_shadow;               // 本世界 profile 的 ShadowInfo（每帧槽一份 ring）
    uint64_t addr_entity_gpu;           // 实体行表 EntityGPU（世界私有；GPU 剔除的枚举源）
    uint64_t addr_l2w;                  // L2W 矩阵池（世界私有 TransformAssignmentBuffer；按帧槽切份，每帧槽一份）
    uint64_t addr_entity_render_ids;    // 实体 4-ID 表（世界私有；按 EntityID.index 直索引，行 = RenderItemDescriptor 16 B；
                                        // GPU 消费者取池口径 geometry_id / material_id / texture_id 的唯一通路）
    uint64_t addr_lights;               // 光源行表（世界私有；行 0 = 主方向光；按帧槽切份）
    uint64_t addr_ambient_env;          // 环境光 provider 参数块（世界私有；单份 ⇒ 全帧槽同址；T1 归位）
};

#define world_addresses WorldAddressesRef(pc_root.addr_world_addresses)

// ── 光源行（世界私有表 `addr_lights`）─────────────────────────────────────────
// C++ 真源 = `inc/hgl/graph/ubo/LightRow.h`（门 `W.light-row-struct-parity` 逐字段同序同型对拍）。
// 行步长 = 64 B：C++ 侧由 `alignas(64)` + 显式 `_pad` 补齐，而 GLSL 的 scalar layout
// **不隐式补**这个填充 ⇒ 末尾 `_pad1[2]` 是必须的（写了数组步长才是 64；不写 = 48，
// 除第 0 行外全部错位读 = 静默拿到垃圾光照参数）。
// `buffer_reference_align=16`：取址走 `GetBufferDeviceAddressAligned16`（只承诺 16），
// 行内成员最大对齐也是 16（vec4）⇒ 16 是能兑现的承诺。
struct LightRowData
{
    uint  type;                  // LightType（0 = Direction）
    uint  flags;                 // kLightFlag*（bit0 enabled / bit1 cast_shadow）
    float _pad0[2];              // 与 C++ 的 _pad0 同形（vec4 必须落在 16B 边界）
    vec4  direction_intensity;   // xyz = **指向光源**的方向（与 pcf_shadow 既有约定一致）；w = 强度
    vec4  color_angle;           // rgb = 颜色；a = 角径（度）
    uint  shadow_kind;           // LightShadowKind（0 = None / 1 = CSM）
    uint  shadow_index;          // 该机制下的资源索引（0 = 无）
    uint  _pad1[2];              // 显式填充至 64 B（行步长契约）
};

layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer LightRowBufferRef
{
    LightRowData lights[];
};

// 行数组本体：下标 = 行号。主方向光恒为 **行 0**（C++ `kMainLightRowIndex`）。
#define light_rows LightRowBufferRef(world_addresses.addr_lights).lights
#define main_light light_rows[0]

struct ShadowCascadeInfo
{
    mat4 shadow_vp;
    vec4 shadow_params;
    vec2 shadow_map_size;
    vec2 inv_shadow_map_size;
    uvec4 shadow_tex;
    vec4 cascade_params;
    vec4 cache_origin;
    uvec4 cache_offset;
    uvec4 cache_valid_rect;
};

// shadow（S2：地址进表，**按帧槽各一份** —— ring[i] 对应帧槽 i）退出 Scene 集绑定。
// 宏名与成员名不变 ⇒ `shadow.*` 读点零改动。
layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer ShadowInfoRef
{
    mat4 shadow_vp;
    vec4 shadow_params;
    vec2 shadow_map_size;
    vec2 inv_shadow_map_size;
    uvec4 shadow_tex;
    uvec4 csm_params;
    ShadowCascadeInfo cascades[4];
};

#define shadow ShadowInfoRef(world_addresses.addr_shadow)

// 相机行：**世界私有**（世界表 WorldAddresses 的 addr_camera_info），行号 = 相机槽 × 帧槽总数 + 帧槽。
// 为什么不在全局表：相机是每个世界独有的观察者数据（多个世界同帧时全局表只能表达一个）。
#define camera CameraInfoBufferRef(world_addresses.addr_camera_info).cameras[pc_root.camera_row]

#endif // HGL_SCENE_UBO_GLSL
