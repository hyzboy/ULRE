// scene_ubo_bindings.glsl — 场景级 UBO/SSBO 的**寻址胶水**（手工维护，不参与代码生成）
//
// 分工（2026-10-29 定稿）：
//   · `ShaderLibrary/ubo/<name>.glsl`  = **结构本体**，由 C++ 结构 + 注解自动生成
//    （含 struct / buffer_reference 块），唯一真源是 `inc/hgl/graph/ubo/*.h`；
//   · **本文件** = **地址从哪来**：把结构体跟 `pc_root` / `global_addresses` /
//     `world_addresses` 里的具体字段绑成宏。
//
// 为什么拆开：结构由 AST 推导（可生成、可校验）；寻址是设计决策（哪个槽、哪个表、
// 哪个帧槽），改一次就是一次架构变更，不适合自动生成。混在一起会让"重新生成"
// 覆盖掉寻址逻辑。
#ifndef HGL_SHADERLIB_UBO_SCENE_UBO_BINDINGS_GLSL
#define HGL_SHADERLIB_UBO_SCENE_UBO_BINDINGS_GLSL

// 全局地址表：表本体是 SSBO，基址经 pc_root.addr_global_addresses 下发。
// 宏名不变 ⇒ 所有 `global_addresses.addr_*` 读点（含生成侧发射的 MTL_ROW / camera 宏）
// 与 shader 正文零改动。
#define global_addresses GlobalAddressesRef(pc_root.addr_global_addresses)

// 世界地址表：世界私有 SSBO 的地址表，基址经 pc_root.addr_world_addresses 下发。
// 为什么独立成表：表内都是**世界私有** buffer（相机行表 / 影子数据 / 实体行表 / L2W /
// 实体 4-ID 表 / 光源行 / 环境光块）。一个设备上可同时存在多个世界（主世界 + OffscreenWorld），
// 放进全局表只能表达"最后一个世界"的地址 ⇒ 多世界同帧互踩。
#define world_addresses WorldAddressesRef(pc_root.addr_world_addresses)

// ambient_env（`ambient_light_provider` 槽的参数块）：世界私有、单份 buffer
// ⇒ 全帧槽同址。消费者 = lighting/indirect_ibl.glsl。
#define ambient_env AmbientEnvInfoRef(world_addresses.addr_ambient_env)

// viewport（S2：地址进表 —— 单份 buffer，内容按 pass/RT 覆盖写、地址恒定 ⇒ 全帧槽同址）
#define viewport ViewportInfoRef(global_addresses.addr_viewport)

// 调色板（256 项 RGBA8 打包，unpackUnorm4x8 解码）：构造期一次写入、长期有效
// ⇒ 地址随地址表下发（不进 pc_root —— 后者只承载每 pass / 每帧变化的地址）。
#define color_palette ColorPaletteRef(global_addresses.addr_color_palette)

// 光源行数组本体：下标 = 行号。主方向光恒为 **行 0**（C++ `kMainLightRowIndex`）。
#define light_rows LightRowBufferRef(world_addresses.addr_lights).lights
#define main_light light_rows[0]

// ⚠ 宏名不得取单词级常用标识符：原名 `shadow` 会把任何材质里名为 shadow 的局部变量
// 整词改写成 buffer_reference 表达式 ⇒ 编译期报 'const' : no qualifiers allowed for
// function return（实测击穿过一个材质源里的 `const float shadow = ...` 局部变量声明）。
// 2026-10-11 改名 shadow → shadow_info，同步 pcf_shadow.glsl 的使用点。
#define shadow_info ShadowInfoRef(world_addresses.addr_shadow)

// 相机行：**世界私有**（世界表 WorldAddresses 的 addr_camera_info），行号 = 相机槽 × 帧槽总数 + 帧槽。
// 为什么不在全局表：相机是每个世界独有的观察者数据（多个世界同帧时全局表只能表达一个）。
#define camera CameraInfoBufferRef(world_addresses.addr_camera_info).cameras[pc_root.camera_row]

#endif // HGL_SHADERLIB_UBO_SCENE_UBO_BINDINGS_GLSL
