# Niagara vs ULRE：meshlet / drawcull / clustercull 实现对比

- 对照对象：`E:\zeux.io\niagara`（Arseny Kapoulkine，从零手写 Vulkan，33 集直播）↔ ULRE（`E:\ULRE`）
- 目的：为 ULRE 的 **② meshlet X 源** 与 **④ 实例化** 两项欠账、以及未来遮挡剔除/剔除路线改造提供参考
- 本文件全部结论都带 `path:line`，未标注的是推断，已标「推断」

---

## 0. 总体定位差异（先看这张，再看细节）

| 维度 | Niagara | ULRE |
|---|---|---|
| 剔除单位 | **两级**：draw(物体) + meshlet(cluster) | **一级**：实体（render item） |
| 遮挡剔除 | **有**（Hi-Z 深度金字塔 + early/late 两遍 + 跨帧可见位图） | 无 |
| LOD | **有**（≤8 级，error 驱动，每级各自一套 meshlet） | 无 |
| 背面/cone 剔除 | **有**（meshlet cone） | 无 |
| Task shader | 有（可选，`taskShadingEnabled` 默认关） | **无**（mesh shader 只有 VertexPassthrough / LineQuad / CharQuad 三模式） |
| meshlet 渲染路径 | 有（task+mesh 与 compute+mesh 两条） | **无**（`sbo_meshlets` 声明了但全库零消费者） |
| 剔除输出 | 全局 draw 命令表 + meshlet cluster 表 | **per-batch 行表切片**（gather 四表），与批/竞技场强耦合 |
| 提交模型 | 两段式：cull 出**计数** → `submit` 阶段编成 groupCount → `DrawMeshTasksIndirect`/`DispatchIndirect` | 一段式：cull 直接写 icb 行；绘制侧把**每批计数缓冲**当 `countBuffer` |
| 帧在飞 | `MAX_FRAMES 2`（早/晚两遍在同帧内） | 帧槽 **8**（`HGL_FRAME_SLOT_TOTAL`） |
| 材质寻址 | 全局材质表 + `drawId` 索引（批次不是剔除单位） | 批 = 剔除与绘制的分区单位，行表由批装配产出 |

---

## 1. Niagara：meshlet 数据构建

### 1.1 结构（`src/shaders/mesh.h:11-24`，与 C++ `src/scene.h:10-23` 逐字段对齐）

```glsl
struct Meshlet {              // 20 B（alignas(8)）
    float16_t center[3];      // 6 B  来自 meshopt_computeMeshletBounds
    float16_t radius;         // 2 B
    int8_t  cone_axis[3];     // 3 B  归一化后 ×127 存 int8
    int8_t  cone_cutoff;      // 1 B
    uint dataOffset;          // 顶点引用区 + 三角形索引区在 meshletdata 中的起点
    uint baseVertex;          // minVertex 化后的顶点基址（关键：让引用能收窄到 16 位）
    uint8_t vertexCount, triangleCount, shortRefs, padding;
};
```

### 1.2 构建流程（`src/scene.cpp:90-115` → `appendMeshlet` `src/scene.cpp:24-88`）

1. `meshopt_buildMeshletsBound` 预算 → `meshopt_buildMeshlets(...)`
   - 参数：`max_vertices = MESH_MAXVTX(64)`、`min_triangles = MESH_MAXTRI/4(24)`、`max_triangles = MESH_MAXTRI(96)`、`cone_weight = MESHLET_CONE_WEIGHT(0.25)`
   - 光追 LOD 走 **`meshopt_buildMeshletsSpatial`**（`scene.cpp:103`，`fill_weight=0.5`，为射线遍历优化空间局部性，而非光栅的 cone/fill 平衡）
2. `meshopt_optimizeMeshletLevel(..., 1)`（`scene.cpp:109`）：重排 meshlet 内顶点与三角形顺序，供后续索引压缩
3. 顶点引用压缩（`scene.cpp:28-44`）：
   - 求 `minVertex/maxVertex`；`shortRefs = (maxVertex - minVertex) < 65536`
   - 引用值 = `index - minVertex`；`shortRefs` 时**两个 16 位引用打进一个 uint32**（`i % 2` 时 `|= ref << 16`）
   - `m.baseVertex = baseVertex + minVertex`
4. 三角形索引：`meshopt` 产出的 `meshlet_triangles` 是 uint8 数组，按 `(tri*3+3)/4` 个 uint32 整块 push 进 `meshletdata`（`scene.cpp:46-50`）——**写入端按 uint32 存，读取端按 uint8 读**（见 §3.3）
5. 光追专用顶点缓存（`scene.cpp:52-67`）：仅 `lodRT` 那一级，把本 meshlet 引用的顶点位置 `quantizeHalf` 后按 **4 分量/顶点**紧打包进 `meshletvtx0`（vec4 half = 8 B/顶点），供 BLAS/CLAS 直接吃
6. bounds（`scene.cpp:69-85`）：`meshopt_computeMeshletBounds` → center/radius `quantizeHalf`，cone 量化成 int8

### 1.3 数据布局（关键：同一个 buffer 的三种视图）

`meshletdata`（`uint32` 数组）内部：

```
[ dataOffset 起 ]
  ├─ 顶点引用区: shortRefs ? (vertexCount+1)/2 个 u32（每 u32 两个 u16 引用）
  │              : vertexCount 个 u32
  └─ 三角形区  : (triangleCount*3+3)/4 个 u32（按 uint8 读 = u8vec3 索引）
```

对应 `meshlet.mesh.glsl:112-117` 的解锁：

```glsl
uint vertexOffset = dataOffset;
uint indexOffset  = dataOffset + (shortRefs ? (vertexCount + 1) / 2 : vertexCount);
```

GLSL 侧用同一 binding 的三个不同视图读同一显存（`meshlet.mesh.glsl:40-53`）：

```glsl
layout(binding = 3) readonly buffer MeshletData   { uint     meshletData[];   };
layout(binding = 3) readonly buffer MeshletData16 { uint16_t meshletData16[]; };
layout(binding = 3) readonly buffer MeshletData8  { uint8_t  meshletData8[];  };
```

（BDA/descriptor 三视图同址的技巧，ULRE 侧对应做法是 `layout(buffer_reference, scalar, ...)` + 不同类型名的 ref）

### 1.4 LOD（`src/scene.cpp:229-276`）

- `meshopt_simplifyWithAttributes`，`options = SimplifySparse | SimplifyErrorClamped`，目标 = 上一级索引数的 **60%**，`maxError = 0.1`
- **顶点属性（法线）参与简化权重**（`normalWeights = {1,1,1}`）——避免朝向失真
- 终止条件：错误到上限 / 索引到 0 / 新一级 `>= 上一级 85%`（间距太小）
- **error 从粗到细累积**：`lodError = max(lodError * 1.5f, nextError)`（`scene.cpp:260`，注释明确"important!"）
- `mesh.lodRT` = 第一个 `error * maxScale < kShadowLodError(3e-3)` 的级（`scene.cpp:266-268`）→ 光追用更粗的网格
- **每个 LOD 各自建一整套 meshlet**（`scene.cpp:270-276`），`MeshLod{meshletOffset, meshletCount}` 分段 ⇒ 全池 = 所有 LOD 的 meshlet 顺序拼接
- LOD 上限 `Mesh.lods[8]`（`scene.h:92`）

> 对 ULRE：有没有 meshlet 先不说，**LOD 是 ULRE 完全空白的维度**，且 meshlet 与 LOD 是"每级一套 meshlet"的耦合关系（不是一份 meshlet 分级），选型时必须一起定。

---

## 2. Niagara：`drawcull.comp`（物体级剔除）

文件：`src/shaders/drawcull.comp.glsl`（156 行），派发点 `src/niagara.cpp:1536-1580`

### 2.1 参数与开关

- `local_size_x = 64`，1 线程 = 1 draw，`di >= cullData.drawCount` 直接 return
- spec constant：`LATE`(id 0)、`TASK`(id 1)
- `CullData`（`mesh.h:26-44`）字段：`view`、`P00/P11/znear/zfar`、**`frustum[4]`**、`lodTarget`、`pyramidWidth/Height`、`drawCount`、5 个开关、`postPass`

### 2.2 视锥剔除：4 标量对称形式（`drawcull.comp.glsl:77-84`）

```glsl
visible = visible && center.z * frustum[1] - abs(center.x) * frustum[0] > -radius; // 左+右一次
visible = visible && center.z * frustum[3] - abs(center.y) * frustum[2] > -radius; // 上+下一次
visible = visible && center.z + radius > znear && center.z - radius < zfar;        // 近/远用相机 z
```

- `frustum[0..3]` 在 CPU 侧由 `projectionT[3] ± projectionT[0/1]` 归一化得来（`niagara.cpp:1502-1514`）
- 用 `abs(center.x)` 同时判左右两平面 ⇒ **4 次乘法完成 4 平面**，代价是要投影关于 x/y 轴对称（标准透视满足）
- 边界体用**球**（`mesh.center` + `mesh.radius * draw.scale`，`drawcull.comp.glsl:73-75`），不是 AABB

### 2.3 遮挡剔除：`projectSphere` + 深度金字塔（`drawcull.comp.glsl:86-99`）

```glsl
vec4 aabb;
if (projectSphere(center, radius, znear, P00, P11, aabb)) {     // 球 → 屏幕空间 AABB
    float level = getOcclusionMip(aabb, pyramidWidth, pyramidHeight);
    float depth = textureLod(sampler2D(depthPyramid, depthSampler), (aabb.xy+aabb.zw)*0.5, level).x;
    float depthSphere = znear / (center.z - radius);             // 球最近点的反投影深度
    visible = visible && depthSphere > depth;
}
```

- `projectSphere`：Mara & McGuire 2013 的"裁剪后透视投影球的多面体界"（`shaders/math.h:2-22`），**球心在 znear 前面才返回 true**
- `getOcclusionMip`（`shaders/math.h:24-39`）两步定 mip：
  1. `level = ceil(log2(max(size.x*W, size.y*H)))` — 保证降到一个 texel 以下
  2. **若更细一级下 AABB 仍只覆盖 ≤2×2 texel，则减 1 级**（`fract(min)+size <= 2` 判定）——省一次采样质量
- **采样器配置成 min-reduction**（`VK_SAMPLER_REDUCTION_MODE_MIN`，`resources.h:45` 的 `reductionMode` 参数）；用 `textureLod` 取单点而非双线性四角
- 深度比较用**反投影深度**（`znear/(z-r)`）与金字塔中的 0..1 深度直接比 —— 与无限远投影矩阵配套（注释 `meshlet.task.glsl:109` 说明远平面检查可能误剔跨远平面的 mesh）

### 2.4 LOD 选择（`drawcull.comp.glsl:106-116`）

```glsl
float distance  = max(length(center) - radius, 0);
float threshold = distance * cullData.lodTarget / drawData.scale;
for (uint i = 1; i < mesh.lodCount; ++i)
    if (mesh.lods[i].error < threshold) lodIndex = i;
```

`lodTarget = (2/P11) * (1/screenHeight) * (1 << debugLodStep)`（`niagara.cpp:1519`）—— **"在 z=1 处 1 像素的误差预算"**，随分辨率自动缩放。

### 2.5 两种输出（`drawcull.comp.glsl:120-152`）

**A. TASK 路径**：把该 draw 的 LOD 的 meshlet 切成 `TASK_WGSIZE(64)` 一组，写**多条** `MeshTaskCommand`：

```glsl
struct MeshTaskCommand { uint drawId, taskOffset, taskCount, lateDrawVisibility, meshletVisibilityOffset; };
uint taskGroups = (lod.meshletCount + TASK_WGSIZE - 1) / TASK_WGSIZE;
uint dci = atomicAdd(commandCount, taskGroups);
if (dci + taskGroups <= TASK_WGLIMIT)          // 溢出丢弃（"drop draw calls on overflow"）
    for (...) { ...; taskCommands[dci+i] = ...; }
```

**B. 非 TASK 路径**（`drawcull.comp.glsl:141-151`）：写 1 条 `MeshDrawCommand`（= `VkDrawIndexedIndirectCommand` 形状 + `drawId`，`mesh.h:104-114`），原子加 1。

### 2.6 两遍互补（核心设计，`drawcull.comp.glsl:66-68, 101-104, 154-155`）

```glsl
if (!LATE && drawVisibility[di] == 0) return;        // early：只处理上一帧可见的
...
// late：既要不漏本帧新可见的，又要不重画 early 画过的
if (visible && (!LATE || (clusterOcclusionEnabled && TASK_CULL) || drawVisibility[di] == 0 || postPass != 0))
    ...
if (LATE) drawVisibility[di] = visible ? 1 : 0;      // 回写，供下一帧 early 用
```

- `drawVisibility[]` 是**跨帧持久**的 per-draw 可见表（binding 4）
- early pass 只画"上帧可见"→ 得到一个**保守但有效**的深度金字塔
- late pass 画"本帧可见但上帧不可见"的补集 → 增量补齐
- 注意 late 对 meshlet 剔除路径（TASK_CULL）有例外：**允许重画**，因为要靠 task/mesh 侧读 `lateDrawVisibility` 去反向跳过"上帧可见的 meshlet"（详见 §4）

---

## 3. Niagara：`clustercull.comp`（meshlet 级剔除，compute 路线）

文件：`src/shaders/clustercull.comp.glsl`（148 行）

### 3.1 线程映射

```glsl
layout(local_size_x = TASK_WGSIZE /*64*/) in;
uint commandId = gl_WorkGroupID.x * 64 + gl_WorkGroupID.y;   // ← 与 tasksubmit 的 X*64 布局配对
MeshTaskCommand command = taskCommands[commandId];
uint mgi = gl_LocalInvocationID.x;         // meshlet 组内序号
uint mi  = mgi + command.taskOffset;       // 全局 meshlet 索引
uint mvi = mgi + command.meshletVisibilityOffset; // 可见位图内的位序号
```

⇒ 一个 workgroup 消费一个 `MeshTaskCommand`，正好对应 drawcull 里"一个 draw 的 64 个 meshlet"。

### 3.2 剔除组合（`clustercull.comp.glsl:74-133`）

顺序：**跨帧可见位 → cone 背面 → 4 标量视锥 → 近/远 → late 金字塔遮挡**

- cone cull（`shaders/math.h:41-44`）：
  ```glsl
  bool coneCull(vec3 center, float radius, vec3 cone_axis, float cone_cutoff, vec3 cam)
  { return dot(center - cam, cone_axis) >= cone_cutoff * length(center - cam) + radius; }
  ```
  cone 在世界空间算（`cone_axis` 先随物体朝向旋转，再进相机空间），半径用**未缩放**的 meshlet 半径参与 cone 判定（`clustercull.comp.glsl:79-83`）
- 视锥与近远判定与物体级同式，只是 `radius *= meshDraw.scale`

### 3.3 跨帧 meshlet 可见位图（`clustercull.comp.glsl:88-101, 127-133`）

```glsl
if (clusterOcclusionEnabled == 1 && postPass == 0) {
    uint bit = meshletVisibility[mvi >> 5] & (1u << (mvi & 31));
    if (!LATE && bit == 0)                 visible = false;   // early：只画上帧可见 meshlet
    if (LATE && lateDrawVisibility == 1 && bit != 0) skip = true; // late：上帧可见的已画过，跳过
}
// late 结束回写
if (LATE && clusterOcclusionEnabled == 1) {
    visible ? atomicOr(meshletVisibility[mvi>>5], bit) : atomicAnd(meshletVisibility[mvi>>5], ~bit);
}
```

- `meshletVisibility` 是**位图**（1 bit/meshlet），per-draw 用 `meshletVisibilityOffset` 切片
- 与物体级的 `drawVisibility[]` 形成**两级跨帧可见性状态**，且两级间有明确的"重画/跳过"协商（`lateDrawVisibility` + `skip`）——这是这套设计里最值得精读的部分

### 3.4 输出：24+8 位打包（`clustercull.comp.glsl:135-141`）

```glsl
uint index = atomicAdd(clusterCount, 1);
if (index < CLUSTER_LIMIT /*1<<24*/)
    clusterIndices[index] = commandId | (mgi << 24);   // 低 24 位 = 命令号，高 8 位 = 组内 meshlet 号
```

⇒ 32 位里塞下"哪条命令 + 命令内第几个 meshlet"，省一层间接。上限 2^24 = 16M cluster。

### 3.5 与 task shader 路线的关系（`meshlet.task.glsl`）

剔除逻辑**逐行相同**，差别只在收尾：

```glsl
// compute 路线                            // task 路线
atomicAdd(clusterCount, 1)                  sharedCount++（shared int）
→ clusterIndices[global]                    → payload.clusterIndices[shared]
[clustersubmit 编 groupCount]               barrier()
[DrawMeshTasksIndirect]                     EmitMeshTasksEXT(sharedCount, 1, 1)
```

- payload：`taskPayloadSharedEXT MeshTaskPayload { uint clusterIndices[TASK_WGSIZE]; }`（`mesh.h:125-128`）
- `niagara.cpp:36` 注释说明 `taskShadingEnabled` **默认关**（AMD 上 compute 更快）；`config.h:8` `TASK_CULL 1`

---

## 4. Niagara：`meshlet.mesh.glsl`（消费端）

文件：`src/shaders/meshlet.mesh.glsl`（206 行）

```glsl
layout(local_size_x = MESH_WGSIZE/*64*/) in;
layout(triangles, max_vertices = MESH_MAXVTX/*64*/, max_primitives = MESH_MAXTRI/*96*/) out;

// 两种集群索引来源：task payload（1D）或 compute 产出的 clusterIndices（3D tiled）
uint ci = TASK ? payload.clusterIndices[gl_WorkGroupID.x]
               : clusterIndices[gl_WorkGroupID.x + gl_WorkGroupID.y * 256 + gl_WorkGroupID.z * CLUSTER_TILE];

if (ci == ~0) { SetMeshOutputsEXT(0, 0); return; }   // submit 阶段填的 dummy 槽

MeshTaskCommand command = taskCommands[ci & 0xffffff];
uint mi = command.taskOffset + (ci >> 24);
```

- 顶点阶段（`:125-162`）：按 `shortRefs` 分两支解引用，位置/UV 都是 half 直接进 vec3/vec2，法线/切线走 `unpackTBN`（10-10-10-2 + 8-8 八面体）
- **可选三角形剔除**（`MESH_CULL`，默认 0，`:164-205`）：背面（2D 有向面积）+ 零面积 + 亚像素小三角形（`sbprec = 1/256`，对应 subpixelPrecisionBits），用 `gl_CullPrimitiveEXT` 标记；注释明确承认"不完全匹配硬件行为，可能过松也可能过严"
- 顶点循环用 `#if MESH_MAXVTX <= MESH_WGSIZE break; #else i += MESH_WGSIZE` —— 编译期消掉跨迭代循环（64/64 时零开销）

---

## 5. Niagara：提交阶段（`tasksubmit` / `clustersubmit`）

**为什么需要这一阶段**：`vkCmdDrawMeshTasksIndirectEXT` / `vkCmdDispatchIndirect` 的 `groupCount{X,Y,Z}` 都是 16 位上限 65535（EXT_mesh_shader 限制），而可见 cluster 数可达 16M ⇒ 必须把一维计数**编成 2D**。

### `tasksubmit.comp.glsl`（1 workgroup，64 线程）

```glsl
groupCountX = min((count + 63) / 64, 65535);   // X 上限 65535
groupCountY = 64;                              // Y 固定 64
groupCountZ = 1;
// 尾部补齐到 64 边界，填 dummy 命令（GL_EXT_null_initializer 的 `MeshTaskCommand dummy = {}`）
if (count + tid < boundary) taskCommands[count + tid] = dummy;
```

### `clustersubmit.comp.glsl`（1 workgroup，256 线程）

```glsl
groupCountX = CLUSTER_TILE;                    // 16
groupCountY = min((count + 255) / 256, 65535);
groupCountZ = 256 / CLUSTER_TILE;              // 16
if (count + tid < boundary) clusterIndices[count + tid] = ~0;   // 哨兵，mesh 侧判 ~0
```

注释（`clustersubmit.comp.glsl:30-32`）解释了 `16*Y*16` 这个奇特布局：**7900 上 `Y*256` 最快，RDNA2 集显上 `Y*256` 很慢，16×Y×16 是两者的平衡点** —— 这是硬件相关的实测调优，不是通用结论。

### 两段式链路的接口约定

`dccb` 缓冲头部是 `{commandCount, groupCountX, groupCountY, groupCountZ}`（`tasksubmit.comp.glsl:14-20`），于是：

- `cull` 写 `commandCount`（draw 剔除产物）
- `vkCmdDispatchIndirect(dccb.buffer, 4)` —— **offset 4 跳过 commandCount**，直接读 submit 写好的 groupCount（`niagara.cpp:1605`）⇒ **draw 剔除的结果直接当 cluster 剔除的线程数**
- `vkCmdDrawMeshTasksIndirectEXT(ccb.buffer, 4, 1, 0)`（`niagara.cpp:1670`）

---

## 6. Niagara：一帧的剔除/渲染编排（`niagara.cpp:1771-1794`）

```cpp
cull(meshSubmit ? taskcullPipeline : drawcullPipeline, 2, "early cull", /*late=*/false);
render(/*late=*/false, colorClear, depthClear, 0, 4, "early render");   // loadOp = CLEAR

pyramid(6);                                                             // 深度金字塔

cull(...drawculllatePipeline, 8, "late cull", /*late=*/true);
render(/*late=*/true, ..., 1, 10, "late render");                       // loadOp = LOAD

if (meshPostPasses >> 1) {                                              // 只有真需要才跑
    cull(..., /*late=*/true, /*postPass=*/1);
    render(..., /*postPass=*/1);                                        // cullMode=NONE + depthBias
}
```

配套细节：
- 每次 cull 前 `vkCmdFillBuffer(dccb, 0, 4, 0)` 清计数（`niagara.cpp:1547`），barrier 用 `stageBarrier`（`resources.cpp:76-100`）
- `pyramid()`（`niagara.cpp:1709-1739`）：从 depthTarget 起逐级 `depthreduce.comp`（32×32，min-reduction sampler 取 2×2 最小）到 `depthPyramid`
- `rasterizationStage` 按是否 mesh submit 切换 barrier 掩码（`niagara.cpp:1538-1541`）

---

## 7. ULRE：现状（对照基线）

### 7.1 剔除着色器 `ShaderLibrary/ecs/entity_gpu_cull.glsl`（353 行）

- `local_size_x = 64`，1 线程 = 1 实体；`pc.entity_count` 上限截断
- 输入两级过滤：`flags & 1`(Active) / `flags & 2`(Visible) → 4-ID 表 `geometry_id`（0=未上行，跳过）→ `row[1]`(Transform) 行号（0=未挂载，跳过）
- **视锥剔除：6 个完整平面 + AABB**（`:266-288`）：
  ```glsl
  vec3  world_center  = (l2w * vec4(aabb_center.xyz, 1)).xyz;
  float scale         = max(length(l2w[0].xyz), max(length(l2w[1].xyz), length(l2w[2].xyz)));
  vec3  world_extents = aabb_extents.xyz * scale;              // 最大缩放近似（保守）
  float radius        = |n.x|*e.x + |n.y|*e.y + |n.z|*e.z;
  if (dot(n, world_center) + plane.w < -radius) invisible;
  ```
  - 用**各轴最大缩放**而非逐轴缩放：非均匀缩放下偏保守（不漏剔，可接受），注释已标"简化"
  - 边界体是 **AABB**（`addr_geometry_aabb` 池，center/extents 各一个 vec4，`geometry_id * 2` 索引）
- 幸存：`atomicAdd(draw_count)` → `visible_indices[idx] = entity_index`；`idx >= max_visible_count` 则不再写行（但计数已加）
- **gather 四表**（`enable_gather != 0`，`:308-353`）：
  ```
  batch = batch_tag[entity_index];   // 0xFFFFFFFF / >= batch_count ⇒ 不写行（CPU 路径画），但仍计全局幸存
  slot  = atomicAdd(batch_counter[batch]);
  if (slot >= capacity) { atomicAdd(overflow[+1], 1); return; }   // 硬判据
  row = row_base + slot;                                          // 本批切片
  icb[row]  = { ceil(element_count / mesh_group_size), 1, 1 }      // X 组数由几何现算
  l2w[row]  = transform_id
  cmds[row] = { geometry_id, slot }                                // first_instance = 切片内序号
  mtl[row]  = { material_id, texture_id, shadow_bias, shadow_flags }
  ```

### 7.2 宿主 `src/ecs/support/GPUCullSystem.cpp`（794 行）+ `.h`（253 行）

- 相位 **RenderFrameSync**（晚于 RenderBatch，`.h:24-27`）
- 独立 cull queue + 命令缓冲环（槽 = 帧槽数 8）+ fence 环；与主帧共用 graphics queue ⇒ FIFO 保证 `[cull cmd → 主帧 draw]` 序（`.h:229-238`）
- 批标签 / 分区 / 每批计数**按帧槽分份、CPU 只写当前份**（`.h:94-121`）
- 两档 opt-in：`SetEnabled`（dispatch + 对拍）、`SetDrawOverrideEnabled`（形态 B 接管）
- `kMaxVisibleEntities = 4096`、`kWorkGroupSize = 64`、`kMaxBatches = 256`

### 7.3 meshlet 现状（这是缺口所在）

| 项目 | 状态 |
|---|---|
| 磁盘数据 | **有**：GLTFConvert 用 `meshopt_buildMeshlets`（`src/Tools/GLTFConvert/gltf/convert/CreateGeometry.cpp:157-215`），`max_vertices=64 / max_triangles=124 / cone_weight=0.5` |
| 结构 | `MeshletDescriptor` 16 B（`inc/hgl/graph/geo/VKGeometry.h:17-26`）：`vertex_offset`(u32) / `triangle_offset`(u32，**单位 = u8vec3 即除以 3**) / `counts`(打包 vc+tc) / `reserved`；`MeshletBounds` 另有 cone apex/axis/cutoff（float + s8 双份，`:28-37`） |
| BDA 通路 | 有：`addr_meshlets / addr_meshlet_vertices / addr_meshlet_triangles` 进 `MeshDrawParams`（`src/SceneGraph/VKGeometry.cpp:194-202`） |
| GLSL 声明 | 有：`sbo_meshlets / sbo_meshlet_vertices / sbo_meshlet_triangles`（`src/ShaderGen/meshgen/MeshShaderVertexAdapter.h:104-118`） |
| **渲染路径** | **无**：全库零消费者（grep `sbo_meshlets` 在 `ShaderLibrary/` + `ShaderGen/` 无命中）；mesh shader 只有 VertexPassthrough / LineQuad / CharQuad 三模式（`MeshModeDescriptor.h:22-24, 185-196`） |
| **Task shader** | **无**（`ShaderLibrary/mesh/` 三个模板均无 `EmitMeshTasks`） |
| **LOD** | **无**（`inc/hgl/graph/geo`、`src/SceneGraph`、GLTFConvert 全无 lod 概念） |
| **GPU 剔除支持** | **主动排除**：meshlet 几何被 guard 挡在接管外（`GPUCullSystem.h:154`、`PrimitiveBatchPipeline.cpp:1337-1339`，理由="X 组数与 CPU `GetMeshletCount()` 分支不同源"） |

### 7.4 ULRE 的 mesh X 组数（对照 niagara 的 meshlet 组数）

```cpp
// PrimitiveBatchPipeline.cpp:791-799
if (db.geometry && db.geometry->HasMeshlets())
    mesh_cmd->groupCountX = db.geometry->GetMeshletCount();          // meshlet 路径：1 组 = 1 meshlet
else
    mesh_cmd->groupCountX = CalcMeshGroupCount(is_lines, total_vertices); // 直通路径：1 组 = N 顶点
mesh_cmd->groupCountY = db.instance_count > 1 ? db.instance_count : 1u;
```

- 直通路径的组顶点上限 = `kMeshVertexPassthroughMaxVertices = 192`（`inc/hgl/mtl/MeshShaderLimits.h:14`），由 CPU 经 push constant 下发（`GPUCullSystem.cpp:614`），GLSL 不硬编码
- 与 niagara 的对应物：`MESH_MAXVTX = 64` / `MESH_MAXTRI = 96`，在 `config.h` 里 C++↔GLSL 共享（`shaders/mesh.h:1` `#include "../config.h"`）——**ULRE 的做法更贴近用户"硬件参数必须实测下发"的偏好，niagara 是编译期共享常量，两种都可，但两者混用（组数公式两源）正是 ② 欠账的根因**

---

## 8. 逐项差异清单

### 8.1 剔除能力

| 能力 | Niagara | ULRE | 备注 |
|---|---|---|---|
| 视锥 | 4 标量对称 + 球 | 6 平面 + AABB | ULRE 若要换对称式：要求投影关于 x/y 轴对称（正交/偏心投影不满足） |
| 遮挡 | Hi-Z 两遍 + projectSphere | 无 | 最大缺口 |
| 背面/cone | meshlet cone（int8 量化） | 无 | 依赖 meshlet 数据（ULRE 盘上有 bounds，可直接用） |
| LOD | ≤8 级 error 驱动 | 无 | 与 meshlet 每级一套耦合 |
| 亚像素/零面积三角形 | 可选（`MESH_CULL`，默认关） | 无 | `gl_CullPrimitiveEXT` |
| 跨帧可见性复用 | 两级（draw + meshlet 位图） | 无 | ULRE 帧槽 8 倍于 niagara 的 2，载体充足但未用 |

### 8.2 剔除粒度与批的关系（**结构性差异，不是功能差**）

- **Niagara**：剔除单位 = draw，与批/材质解耦。材质靠 `drawId` 在 shader 里索引全局材质表（`mesh.frag.glsl` binding 1 + `mtb`）；"批"只体现为 pipeline 绑定与 `postPass`。
- **ULRE**：剔除单位 = 批内 item（实体），输出**直接是批的行表切片**。`row_base`/`capacity`/`first_instance` 全套切片语义（`entity_gpu_cull.glsl:329-334`），overflow 是硬判据。
- ⇒ 引入 meshlet 剔除时，ULRE 需要回答"**cluster 表挂在哪一级**"：挂批（每批一套 cluster 表，切片×一分区维度）还是挂全局（像 niagara）。这是 ② 的真正设计问题，不是补一个着色器。

### 8.3 提交模型

| | Niagara | ULRE |
|---|---|---|
| 命令行 | `MeshTaskCommand` 20 B（drawId/taskOffset/taskCount/lateDrawVisibility/meshletVisibilityOffset） | `BatchCmdRow` 8 B（geometry_id/first_instance）+ icb 12 B/行 |
| 计数 | `commandCount` → submit 编 groupCount | 每批计数缓冲**直接当 countBuffer** |
| 间接调用 | `DrawMeshTasksIndirect`(ccb) / `DispatchIndirect`(dccb, offset 4) | `DrawMeshTasksIndirect`(batch.icb_mesh_tasks) |
| 65535 上限 | 必须 2D tiling + dummy padding | 绕开了（X 由几何决定，一次命令 = 一个实例） |
| 溢出 | `TASK_WGLIMIT(1<<22)` / `CLUSTER_LIMIT(1<<24)`，**丢弃** | 切片容量 + overflow 计数（**fail-fast 判据**） |

> 关键推论（推断，但依据充分）：ULRE 现在绕开了 65535 上限，是因为 X = 几何组数（与可见数无关）。**一旦改成"可见 meshlet 数驱动 X"（niagara 的做法），就必然需要 submit 阶段 + dummy 填充 + 2D tiling** —— ② 落地时会撞上这个，提前按 niagara 的 `tasksubmit` 模式设计可省一轮返工。

### 8.4 meshlet 数据格式

| | Niagara | ULRE |
|---|---|---|
| 行大小 | 20 B | 16 B |
| 顶点引用 | **minVertex 化 + 16 位双打包**（`shortRefs`） | 全局 u32 索引（`meshlet_vertices`） |
| 三角形 | u8vec3 存 uint32 buffer（`(3n+3)/4` 个 u32） | u8vec3 存（`triangle_offset` 单位 = 三角形，直接 `*3`） |
| baseVertex | 有（省一次全局索引加法） | 无 |
| 顶点格式 | 位置/UV = half，法线 10-10-10-2，切线 8-8 八面体 | 见 `s1_*` 顶点流模块（按流变体分离） |
| 上限 | 64 v / 96 t（min_tri 24） | 64 v / 124 t |
| cone 权重 | 0.25（光栅）/ 0.5 + spatial（光追） | 0.5 |
| 光追专用缓存 | 有（`meshletvtx0`，half vec4，仅 lodRT） | 无 |
| 多级 | 每 LOD 一套 meshlet | 单套 |

### 8.5 ULRE 比 Niagara 更细的地方（对比时别把 Niagara 当"更先进"）

1. **批级切片/竞技场 + overflow 硬判据**（niagara 溢出直接丢弃，不报）
2. **帧槽 8 份 + "结构类值同源、内容类值可滞后一帧"的相位纪律**（niagara 只有 `MAX_FRAMES 2`，可见位图跨帧却无分份——因为它单队列单帧提交）
3. **地址三档**（Global / World / `pc_root`）+ 相机 16 槽 × 8 帧槽的行号口径
4. **零兼容层的双重 opt-in**（dispatch 与 draw override 分离）
5. **C++↔GLSL 结构对拍由门用例兜底**（niagara 靠 `config.h` 手工共享 + 注释警示）

---

## 9. 可借鉴清单（按 ULRE 欠账排序）

### 针对 ② meshlet X 源
1. **组数公式单源化**是先决条件——不管走哪条路，`GetMeshletCount()` 与 GLSL 现算必须二选一（niagara 用 `config.h` 共享常量 + shader 只读 meshlet 行的 `vertexCount/triangleCount`；ULRE 可把 `GetMeshletCount()` 的结果写进参数行，或让 GLSL 直接算 meshlet 总数）
2. meshlet 行格式若要压缩：`baseVertex` + `shortRefs` 双打包（`scene.cpp:35-44`）+ u8vec3 三角形，是现成范式
3. 每个可见 meshlet 一次 mesh dispatch ⇒ 必然引入 submit 阶段（`tasksubmit` 模式）

### 针对遮挡剔除（如果要做）
4. `projectSphere`（`shaders/math.h:2-22`）+ `getOcclusionMip` 的两步降级（`:24-39`）+ min-reduction sampler：三项都是可直接搬的算法，注意 **ULRE 池里只有 AABB 没有球**，要么补球（center/radius），要么改用 AABB 的屏幕投影
5. early/late 两遍 + 跨帧可见表/位图（`drawcull.comp.glsl:66-104`、`clustercull.comp.glsl:88-101`）——**注意它依赖"帧内两遍渲染"的编排**，ULRE 当前是单遍 forward + 主帧内多次 pass，改造要连编排一起
6. 两级可见性之间的 `lateDrawVisibility` + `skip` 协商逻辑（防重复绘制）

### 针对 LOD
7. error 驱动 + `lodTarget = 1px @ z=1` 的阈值公式（`niagara.cpp:1519` + `drawcull.comp.glsl:108-116`）
8. `lodError = max(lodError * 1.5, nextError)` 的从粗到细累积（`scene.cpp:260`，注释标了"important"）
9. 简化时顶点属性（法线）参与权重

### 不要照抄的
- `clustersubmit` 的 `16*Y*16` tiling：注释自己说明是 7900 与 RDNA2 集显之间的实测折中（`clustersubmit.comp.glsl:30-32`），换硬件就该重测
- `MESH_CULL` 的亚像素剔除：作者自认"不精确，过松也过严"（`meshlet.mesh.glsl:191`）
- `taskShadingEnabled` 默认关的理由是"AMD 上性能"（`niagara.cpp:36`），不是设计偏好

---

## 附：关键常量对照

| 常量 | Niagara | 位置 | ULRE |
|---|---|---|---|
| task workgroup | `TASK_WGSIZE 64` | `config.h:2` | — |
| mesh workgroup | `MESH_WGSIZE 64` | `config.h:5` | 由模板 `{{local_size}}` 生成 |
| meshlet 上限 | 64 v / 96 t | `config.h:14-15` | 64 v / 124 t（`CreateGeometry.cpp:167-168`） |
| cluster tile | `CLUSTER_TILE 16` | `config.h:22` | — |
| task 上限 | `TASK_WGLIMIT 1<<22` | `config.h:25` | — |
| cluster 上限 | `CLUSTER_LIMIT 1<<24` | `config.h:28` | — |
| 帧在飞 | `MAX_FRAMES 2` | `config.h:31` | `HGL_FRAME_SLOT_TOTAL 8` |
| 剔除 workgroup | 64（drawcull/clustercull） | `drawcull.comp.glsl:14` | 64（`entity_gpu_cull.glsl:204`） |
| 实体/可见容量 | — | — | `kMaxVisibleEntities 4096` |
| 批数上限 | — | — | `kMaxBatches 256` |
| 组顶点上限 | `MESH_MAXVTX 64` | `config.h:14` | `kMeshVertexPassthroughMaxVertices 192` |