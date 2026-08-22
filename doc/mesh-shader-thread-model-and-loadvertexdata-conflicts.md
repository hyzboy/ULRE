# Mesh Shader 迁移踩坑实录：线程模型与 LoadVertexData 冲突

- 日期：2026-08-23
- 分支：`to_mesh_shader`
- 提交：`ad547e843`（Line 渲染迁移 mesh shader：删 4 slot/SetLineWidth，单 buffer + DrawMeshTasks + 屏幕空间线宽）
- 关联代码：`src/ShaderGen/common/MeshShaderAssembler.h`（`GenerateMeshShader` 的 LineQuad 模式）、`src/ecs/support/line/LineRenderPipeline.cpp`

本文档详细记录 P2（Line line-to-quad + Mesh Shader 化）迁移过程中遇到的两个最严重的问题——**Mesh Shader 线程模型误解（AMD 驱动崩溃）** 与 **LoadVertexData 体系冲突（全 NaN）**。两者都是"把 VS 时代的单顶点模型直觉套用到 mesh shader"导致的，对后续通用 mesh 化（VertexPassthrough 模式、Text、全材质 mesh 化）有直接指导意义。

---

## 1. 背景：Line 渲染的 mesh shader 化

P2 之前，Line 渲染使用传统图形管线：

- 4 个宽度 slot 分组（width=1/2/4/8 → slot 0/1/3/7），不同线宽需调用 `vkCmdSetLineWidth`（每次只能设一个宽度）
- 每 slot 独立 VAB + 独立 PerObject descriptor set（slot_mp 机制）
- 多次 `vkCmdDraw`

P2 改为 mesh shader 路径：

- 宽度作为 per-segment 属性写入 SSBO（`VertexSemantic::Size` VAB，binding=12）
- **一次 `vkCmdDrawMeshTasksEXT`**，mesh shader 每个 invocation 处理 1 条线段 → 展开成 quad（4 顶点 2 三角形）
- 删除：slot 分组 / SetLineWidth / slot_mp / 多 Draw

mesh shader 的线程组织与 VS 完全不同，正是这种差异导致了问题 1 和 2。

---

## 2. 问题 1：Mesh Shader 线程模型误解 → AMD 驱动崩溃

### 2.1 现象

第一次实现 LineQuad 模式后，运行 LineRenderTest **直接驱动崩溃**（不是 validation 错误，是 GPU 挂起/驱动崩溃）。用 Vulkan Configurator 的 Crash Diagnostic Layer（CDL）抓取的 dump 显示：

```
CommandBuffer: Swapchain_RenderCmdBuffer_0
  id: 15  vkCmdPushConstants   stageFlags: 129 (0x81 = VERTEX|MESH)  size: 16
         pValues: [0,0,0, 0x50,0x11,0,0]   ← total_vertices = 0x1150 = 4432
  id: 16  vkCmdDrawMeshTasksEXT  groupCountX: 35  groupCountY: 1  groupCountZ: 1   ← INCOMPLETE（崩溃点）
```

- `total_vertices = 4432` → `line_count = 2216`
- `groupCountX = 35` = ceil(2216 / 64)（MESH_GROUP_SIZE = 64）
- `vkCmdDrawMeshTasksEXT` 状态 `INCOMPLETE` = 驱动在 mesh shader 执行期间崩溃

### 2.2 根因：mesh shader 的 per-threadgroup 语义

`vkCmdDrawMeshTasksEXT(groupCountX, Y, Z)` 的派发模型：

```
总 invocation 数 = groupCountX × groupCountY × groupCountZ × local_size
                   = 35 × 64 = 2240

每个 threadgroup 内：
  gl_WorkGroupID.x      = threadgroup 索引（0..34）
  gl_LocalInvocationIndex = invocation 在组内索引（0..63）
  gl_WorkGroupID.x * 64 + gl_LocalInvocationIndex = 全局索引（0..2239）
```

**关键规范事实**（GL_EXT_mesh_shader）：

| 符号 | 语义 | 索引范围 |
|---|---|---|
| `gl_MeshVerticesEXT[]` | **per-threadgroup** 顶点输出槽 | 0 .. max_vertices-1（本组） |
| `gl_PrimitiveTriangleIndicesEXT[]` | **per-threadgroup** 图元输出槽 | 0 .. max_primitives-1（本组） |
| `SetMeshOutputsEXT(v, p)` | 设置**本 threadgroup** 输出量 | v ≤ max_vertices, p ≤ max_primitives |
| `max_vertices` / `max_primitives` | layout 编译期常量，**本组上限** | LineQuad: 256 / 128 |

与 VS 的本质差异：VS 每个 invocation 输出 **1 个顶点**（`gl_VertexIndex` 天然全局）；mesh shader 每个 invocation 可以输出**多个顶点/图元**（写进本组的共享输出槽），且 `SetMeshOutputsEXT` 是**整组共享**的（所有 invocation 调用，最后一次生效，且各 invocation 的值必须一致）。

### 2.3 三个错误的叠加

第一版 LineQuad 生成的 main 存在三个致命错误：

```glsl
// ❌ 错误 1：line_id 只是组内索引——没有 gl_WorkGroupID.x * 64
const uint line_id = gl_LocalInvocationIndex;          // 0..63
const uint vid     = line_id * 4u;                     // 0..255（碰巧对）

// ❌ 错误 2：SetMeshOutputsEXT 传了"全局"线段总数
SetMeshOutputsEXT((pc_vertex_index.total_vertices >> 1u) * 4u,   // 2216*4 = 8864
                  (pc_vertex_index.total_vertices >> 1u) * 2u);  // 2216*2 = 4432
//   8864 > max_vertices(256) → 驱动崩溃

// ❌ 错误 3：图元索引用 line_id（全局）而非组内槽位
gl_PrimitiveTriangleIndicesEXT[line_id * 2u + 0u] = ...;   // 越界 0..127
```

| 错误 | 后果 |
|---|---|
| `line_id` 缺 `gl_WorkGroupID.x * 64` | 所有 35 个 threadgroup **都处理相同的 0..63 号线段**（重复绘制同一批，其余线段永远不画） |
| `SetMeshOutputsEXT(8864, 4432)` | 请求输出远超 layout 声明的 max_vertices(256)/max_primitives(128) → **AMD 驱动崩溃** |
| 图元索引用全局 `line_id` | 图元槽越界（本组只有 0..127 个槽） |

`SetMeshOutputsEXT` 的崩溃是主因：GLSL 规范要求输出数量不得超过 layout 声明的上限，AMD 驱动对超限请求直接崩溃（NVIDIA 可能只是裁剪，但属于 UB）。

### 2.4 正确实现

```glsl
// ✅ 全局线段索引 = threadgroup 索引 × group size + 局部索引
const uint line_id = gl_WorkGroupID.x * 64u + gl_LocalInvocationIndex;
// ✅ threadgroup 内顶点槽位基址（0 .. 255 = max_vertices-1）
const uint vid     = gl_LocalInvocationIndex * 4u;

const uint total_lines = pc_vertex_index.total_vertices >> 1u;

// ✅ 本组有效线段数——所有 invocation 计算相同值 → SetMeshOutputsEXT 一致
//    groupCountX = ceil(total_lines / 64)，末组起始 <= total_lines，不会 uint 下溢
const uint lines_this_group = min(64u, total_lines - gl_WorkGroupID.x * 64u);
SetMeshOutputsEXT(lines_this_group * 4u, lines_this_group * 2u);

// ✅ 越界 invocation 不输出（只读 SSBO 不写顶点）
if (line_id >= total_lines)
    return;

// ✅ 图元槽位用组内索引（0 .. 127 = max_primitives-1）
gl_PrimitiveTriangleIndicesEXT[gl_LocalInvocationIndex * 2u + 0u] = uvec3(vid+0, vid+1, vid+2);
gl_PrimitiveTriangleIndicesEXT[gl_LocalInvocationIndex * 2u + 1u] = uvec3(vid+1, vid+3, vid+2);
```

验证数值（line_count=2216，groupCountX=35）：

| threadgroup | line_id 范围 | lines_this_group | SetMeshOutputsEXT | 有效/越界 |
|---|---|---|---|---|
| 0..33 | 0..2175 | 64 | (256, 128) | 64 / 0 |
| 34（末组） | 2176..2239 | 40 | (160, 80) | 40 / 24 |

全部满足 `≤ max_vertices(256)/max_primitives(128)`，末组越界 invocation（24 个）提前 return 不写输出槽。

### 2.5 调试方法

- **驱动崩溃不是 validation 错误**——VVL 不拦截，直接 GPU 挂起/驱动崩溃。用 Vulkan Configurator 开启 Crash Diagnostic Layer（CDL）抓 dump，定位崩溃命令（本例 `vkCmdDrawMeshTasksEXT` 状态 INCOMPLETE 即崩溃点）。
- 检查 `vkCmdDrawMeshTasksEXT` 的 `groupCountX` 与 push constant 的 `total_vertices` 是否一致（`groupCountX == ceil(total_vertices / group_size)`）。
- RenderDoc 的 mesh shader output 面板会显示每组输出顶点数——若所有组输出相同的小编号线段（0..63 重复），即 `gl_WorkGroupID` 未参与索引。

---

## 3. 问题 2：LoadVertexData 体系与 mesh 模型冲突 → 全 NaN

### 3.1 现象

修复线程模型后驱动不再崩溃，但**画面全黑**。RenderDoc mesh shader output：

> 前 192 个顶点位置在可视范围内（至少不是 NaN），后面上万个顶点坐标全是 NaN。

（192 = 3 × 64，即前 3 个 threadgroup 的顶点；"上万个"= 35 组 × 256 槽位 = 8960 槽位中除前 192 外全 NaN）

### 3.2 根因：LineQuad 与 LoadVertexData 模型的脱节

ULRE 的 SSBO 顶点输入体系（VS 时代）围绕 **`LoadVertexData()`** 设计：

```glsl
// s1_position_vec3.glsl / s1_transform_id.glsl / s1_palette_index.glsl / s1_size.glsl
vec3  Position;
uint  TransformID;
uint  ColorIndex;
float Width;

#define HGL_TRANSFORMID_LOADER { TransformID = sbo_vertex_transform_id.data[pc_vertex_index.vertex_base + VertexIndexID]; }
#define HGL_COLORINDEX_LOADER  { const uint vidx = ...; ColorIndex = (packed >> ((vidx & 3u) * 8u)) & 0xFFu; }
#define HGL_WIDTH_LOADER       { Width = sbo_vertex_size.data[...].x; }

// 由 s1_position_vec3 的 LoadVertexData() 统一展开（VS 的 main 里调用一次）
void LoadVertexData() {
    Position = sbo_vertex_position.data[pc_vertex_index.vertex_base + VertexIndexID];
    #ifdef HGL_TRANSFORMID_LOADER HGL_TRANSFORMID_LOADER #endif
    #ifdef HGL_COLORINDEX_LOADER  HGL_COLORINDEX_LOADER  #endif
    #ifdef HGL_WIDTH_LOADER       HGL_WIDTH_LOADER       #endif
}
```

**核心矛盾**：

| | LoadVertexData 模型（VS） | LineQuad 需求（mesh） |
|---|---|---|
| 每 invocation 处理 | **1 个顶点**（`VertexIndexID = gl_VertexIndex` 或局部索引） | **1 条线段 = 2 个顶点** |
| 全局变量填充 | 调用一次 `LoadVertexData()` 即全部赋值 | 每线程需读 2 组属性（from/to） |
| 数据来源 | `VertexIndexID` 计算的单个 base | `base` 和 `base+1` |

第一版 LineQuad 是**半直读半依赖**的混合设计：

```glsl
// ✅ 直读：端点位置（每线程 2 顶点）
const uint base = pc_vertex_index.vertex_base + line_id * 2u;
const vec3 from = sbo_vertex_position.data[base];
const vec3 to   = sbo_vertex_position.data[base + 1u];

// ❌ 仍走 LoadVertexData 的全局变量（从未赋值！）
#ifdef HGL_WIDTH_LOADER
    width = Width;                    // Width 是全局变量，LineQuad 从不调用 LoadVertexData()
#endif
...
const vec3 from_world = (GetL2W() * vec4(from, 1.0)).xyz;   // GetL2W() 读未赋值的 TransformID
```

由于 `LoadVertexData()` 从未被调用（LineQuad 每线程 2 顶点，调它也只读 1 顶点），**`Width` / `TransformID` / `ColorIndex` 全局变量从未被赋值**：

- `width = Width` → 未初始化全局变量（GLSL 语义未定义，AMD 上常为垃圾/NaN）
- `GetL2W()` → `l2w.mats[TransformID]` 用**垃圾索引**读矩阵 → 垃圾矩阵（越界读返回 0 或垃圾）→ `from_world`/`to_world` NaN → `c0..c3` NaN → **gl_Position 全 NaN**

"前 192 顶点正常"是 AMD 上未初始化变量恰好取到小值/0 的**巧合**（不同 threadgroup 读到不同的垃圾值，前 3 组碰巧正常），不代表代码有任何正确性。

### 3.3 修复：全直读 SSBO（不碰 LoadVertexData 的任何全局变量）

```glsl
const uint base = pc_vertex_index.vertex_base + line_id * 2u;
const vec3 from = sbo_vertex_position.data[base];                    // ✅ 直读
const vec3 to   = sbo_vertex_position.data[base + 1u];

// ✅ palette 颜色索引：R8 打包解码（与 s1_palette_index 的 HGL_COLORINDEX_LOADER 同公式）
//    VAB 是 R8_UINT 1B/顶点，4 索引打包进 1 个 uint，必须解码
const uint color_index = (sbo_vertex_color.data[base >> 2u] >> ((base & 3u) * 8u)) & 0xFFu;

// ✅ TransformID：uint 直读（引擎强制 R32_UINT，见 skill §11）
const uint transform_id = sbo_vertex_transform_id.data[base];

// ✅ 宽度：Size 语义 VAB 是 R32G32_SFLOAT（vec2），取 .x
const float width = sbo_vertex_size.data[base].x;

// ✅ l2w 直查（替代 GetL2W()——后者依赖全局 TransformID）
const mat4 l2w_m = l2w.mats[transform_id];
const vec3 from_world = (l2w_m * vec4(from, 1.0)).xyz;
const vec3 to_world   = (l2w_m * vec4(to, 1.0)).xyz;
```

**纪律**：mesh shader 每线程处理多顶点时，**所有属性必须直接从 SSBO 读**（`sbo_vertex_position` / `sbo_vertex_color` / `sbo_vertex_transform_id` / `sbo_vertex_size`），**绝不使用 `LoadVertexData()` 填充的全局变量**（`Position` / `ColorIndex` / `TransformID` / `Width`）——那些变量只在 `LoadVertexData()` 被调用（VS 路径或 VertexPassthrough 模式的 main 里）时才有定义值。

### 3.4 三个直读细节

**（a）palette 颜色是 R8 打包解码，不是直读**

`s1_palette_index.glsl` 声明：

```glsl
layout(set=VERTEX_SET, binding=VERTEX_COLOR_BINDING, std430) readonly buffer VertexColorData
{ uint data[]; } sbo_vertex_color;    // R8_UINT 1B/顶点 → 4 索引/uint
```

所以**不能**写 `sbo_vertex_color.data[base]`（会读到 4 个打包的索引），必须：

```glsl
const uint packed = sbo_vertex_color.data[base >> 2u];
const uint color_index = (packed >> ((base & 3u) * 8u)) & 0xFFu;
```

（与 skill §5/§13 的 Luminance/TransformID 打包解码同理——VAB 紧凑字节宽 < 4B 时 SSBO 直读错位。）

**（b）l2w 用 `l2w.mats[transform_id]`，不用 `GetL2W()`**

`GetL2W()`（orient_world.glsl）在 `HGL_L2W_FROM_VERTEX_ATTR` 分支下读全局 `TransformID`——mesh 直读路径必须改为直接查表：

```glsl
const mat4 l2w_m = l2w.mats[transform_id];
```

**（c）SSBO 声明来自 include 的 s1_* 模块**

`sbo_vertex_position` / `sbo_vertex_color` / `sbo_vertex_transform_id` / `sbo_vertex_size` / `l2w` 的声明由材质 requirements 驱动的 `resolved_input_glsl` include（MaterialDefinitionRegistry）提供，mesh shader 直接引用即可（声明在前、main 在后）。`pc_vertex_index` push constant 声明在 mesh 模式下由 `MeshShaderAssembler` 补齐（s1_index 被跳过）。

### 3.5 调试方法

- RenderDoc mesh shader output 面板：看 gl_Position 是否 NaN。**前 N 个顶点正常、后面全 NaN = 未初始化全局变量**（AMD 上垃圾值恰好部分正常）——不是数据上传问题，是 shader 读到了未定义值。
- 检查 mesh shader 的 main 是否调用了 `LoadVertexData()`：**mesh 模式（每线程多顶点）绝不应该调用它**；如果调了，只有第一个顶点（`VertexIndexID` 对应的）有值。
- 反编译 SPIR-V 确认全局变量从未被写：`spirv-dis` 后搜索 `Width`/`TransformID` 对应 OpVariable 的 store 指令数量。

---

## 4. 总结：Mesh Shader 编写纪律

1. **索引三件套**：全局数据索引 = `gl_WorkGroupID.x * group_size + gl_LocalInvocationIndex`；输出槽索引 = `gl_LocalInvocationIndex * (每线程输出量)`（per-threadgroup 0..max-1）；图元槽同理。
2. **SetMeshOutputsEXT 是整组共享**：所有 invocation 必须传**一致的值**（用可被每个 invocation 独立算出的本组有效量 `min(group_size, total - group*size)`），且 ≤ max_vertices/max_primitives。越界 invocation 提前 return（在 SetMeshOutputsEXT 之后）。
3. **多顶点/多属性 per-thread 处理 = 全直读 SSBO**：绝不依赖 `LoadVertexData()` 的全局变量（Position/ColorIndex/TransformID/Width），那些变量在 mesh 模式不赋值 → NaN。
4. **窄格式打包解码**：R8（4/uint）/R16（2/uint）VAB 数据必须打包解码，不能当 uint 直读；R32 直读。
5. **debug 优先 C++ 日志 / RenderDoc mesh output**：驱动崩溃用 CDL dump 定位命令；NaN 看 mesh output 的顶点坐标分布。

## 5. 关联

- `src/ShaderGen/common/MeshShaderAssembler.h`：LineQuad 模式（已修复的正确实现，作为模板）
- `src/ecs/support/line/LineRenderPipeline.cpp`：`MESH_GROUP_SIZE = 64`、`LinePushConstant`（20B）、`groupCountX = ceil(line_count / 64)`
- 后续工作：VertexPassthrough 通用模式（`GenerateMeshShader` 的另一分支）有**同样的线程模型隐患**（`vid = gl_LocalInvocationIndex` 缺 `gl_WorkGroupID`、`SetMeshOutputsEXT` 固定 max 无 per-group 裁剪）——通用 mesh 化时须按 §4 纪律重写。
