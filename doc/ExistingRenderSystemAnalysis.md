# ULRE 现有渲染系统分析

> 本文档是 Stage 7.1.4-7.1.5（Compositor Unlit 材质向现有渲染器兼容迁移）的前置分析。
> 聚焦于：描述符集布局、顶点输入、UBO 结构、材质编译管线、ECS 批次/绘制流程。

---

## 1. 描述符集布局（Descriptor Set Layout）

### 1.1 现有系统：6-Set 布局

现有系统使用 `DescriptorSetType` 枚举，枚举的 int 值 **直接映射** Vulkan 描述符集编号：

```
enum class DescriptorSetType {
    Unknow     = 0,
    RenderTarget = 1,   → Vulkan set = 1
    Camera       = 2,   → Vulkan set = 2
    World        = 3,   → Vulkan set = 3
    Global       = 4,   → Vulkan set = 4
    PerFrame     = 5,   → Vulkan set = 5
    PerMaterial  = 6,   → Vulkan set = 6
};
```

在代码中存在别名映射：

| 别名     | 映射到             |
|----------|--------------------|
| Scene    | Global (set=4)     |
| View     | Camera (set=2)     |
| Draw     | PerFrame (set=5)   |
| Material | PerMaterial (set=6) |
| Static   | World (set=3)      |
| Instance | PerFrame (set=5)   |

### 1.2 绑定编号分配规则

`MaterialDescriptorInfo::Resort()` 对每个描述符集内部的描述符按**名字字母序**排列，然后依次分配 binding 编号。

例如 PureColor3D 中：
- **set=1 (RenderTarget)**: `viewport` → binding=0
- **set=2 (Camera)**: `camera` → binding=0
- **set=5 (PerFrame)**: `l2w` → binding=0
- **set=6 (PerMaterial)**: `mtl` → binding=0

因为每个 set 只有一个描述符，所以都是 binding=0。但如果 Camera set 同时有 `camera` 和 `sky`，则 `camera` → binding=0, `sky` → binding=1（字母序）。

### 1.3 新 Compositor 系统：4-Set 布局

Compositor 模板 shader 使用紧凑的 4-set 布局：

| Set | 用途        | 内容                                                            |
|-----|-------------|----------------------------------------------------------------|
| 0   | Scene       | binding=0: ViewportUBO, binding=1: CameraUBO, binding=2: SkyUBO |
| 1   | Transform   | binding=0: L2W_SSBO (mat4[])                                   |
| 2   | Material    | binding=0: MI_SSBO (MI_Unlit[] / MI_Standard[])                |
| 3   | VertexData  | binding=0: VertexPosition_SSBO, binding=1: VertexNormal_SSBO, … |

### 1.4 关键差异

| 维度           | 现有系统                  | Compositor 系统             |
|----------------|---------------------------|-----------------------------|
| 描述符集数量   | 6 个 (set 1-6)            | 4 个 (set 0-3)              |
| 起始 set 编号  | set=1 (Unknow=0 不使用)   | set=0                       |
| 绑定号分配     | 运行时字母序              | 编译期硬编码                |
| ViewportInfo   | set=1, binding=0          | set=0, binding=0            |
| CameraInfo     | set=2, binding=0          | set=0, binding=1            |
| L2W            | set=5, binding=0          | set=1, binding=0            |
| MaterialInst   | set=6, binding=0          | set=2, binding=0            |
| UBO 结构       | SBS_CameraInfo (全量)     | CameraUBO (精简)            |

---

## 2. 顶点输入（Vertex Input）

### 2.1 现有系统的顶点属性

PureColor3D 的 `FixedVertexEntry` 定义：

```cpp
constexpr FixedVertexEntry PURE_COLOR_3D_VERTEX[] = {
    { VAT_VEC3,                   VertexInputGroup::Basic,               VertexInputRate::Vertex,   VAN::Position },
    { Assign::TransformID::VAT_FMT,   VertexInputGroup::TransformID,        VertexInputRate::Instance, Assign::TransformID::VIS_NAME },
    { Assign::MaterialInstanceID::VAT_FMT, VertexInputGroup::MaterialInstanceID, VertexInputRate::Instance, Assign::MaterialInstanceID::VIS_NAME },
};
```

生成的 GLSL 顶点输入：
```glsl
layout(location=0) in vec3 Position;          // VertexInputRate::Vertex
layout(location=1) in uint TransformID;       // VertexInputRate::Instance (R16UI)
layout(location=2) in uint MaterialInstanceID; // VertexInputRate::Instance (R16UI)
```

关键特征：
- **TransformID** / **MaterialInstanceID** 是 Instance-Rate 属性（每实例一个值）
- 使用 `R16UI` 格式（uint16，16位无符号整数）
- TransformID 用于索引 L2W 变换矩阵数组
- MaterialInstanceID 用于索引 MaterialInstance 数组
- 框架自动生成 `GetLocalToWorld()` 和 `GetMI()` 辅助函数

### 2.2 VBO 管理

`VertexDataManager` 负责 VBO 的分配和管理：

```cpp
class VertexDataManager {
    bool Init(VkDeviceSize vbo_size, VkDeviceSize ibo_size, IndexType index_type);
    BlockAllocator::UserNode *AcquireVAB(VkDeviceSize count);  // 分配 vertex buffer
    BlockAllocator::UserNode *AcquireIB(VkDeviceSize count);   // 分配 index buffer
    VAB *GetVAB(uint index);           // 获取第 index 个顶点属性的 VAB
};
```

- 每个 VIL 对应一个 VertexDataManager
- 每个顶点属性单独一个 VAB（Vertex Attribute Buffer）
- 使用 `BlockAllocator` 在 VAB 内做子分配

### 2.3 VILConfig — 运行时顶点格式配置

```cpp
class VILConfig : public UnorderedMap<AnsiString, VAConfig> {
    bool Add(const AnsiString &name, const VAConfig &cfg);
};

struct VAConfig {
    VkFormat format;
    VkVertexInputRate input_rate;
};
```

示例用法（draw_triangle.cpp）：
```cpp
VILConfig vil_config;
vil_config.Add("position", VAConfig(VK_FORMAT_R32G32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX));
vil_config.Add("color",    VAConfig(VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX));
```

- Material 存储 `VertexInput *vertex_input`
- `VertexInput::CreateVIL(const VILConfig *cfg)` 创建 VIL

### 2.4 Compositor 系统的顶点输入

Compositor VS 模板（VBO 模式）：
```glsl
layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec2 inUV0;
layout(location=10) in uint inInstanceID;    // Instance-rate VAB
```

关键差异：
- Compositor 使用 `inInstanceID` 单一 instance-rate 属性
- 现有系统使用 `TransformID` + `MaterialInstanceID` 两个分开的 instance-rate 属性
- Compositor 的 instance ID 同时索引 L2W 和 MI（二者共享同一个实例 ID）

---

## 3. UBO 结构定义（ShaderBufferSource）

`ShaderBufferSource` 是描述符与 GLSL 代码之间的桥梁：

```cpp
struct ShaderBufferSource {
    DescriptorSetType set_type;   // 所属描述符集
    const char *name;             // shader 中的实例名 (如 "camera")
    const char *struct_name;      // GLSL 结构体名 (如 "CameraInfo")
    const char *codes;            // GLSL 成员定义
};
```

### 3.1 标准 SBS 定义汇总

| SBS 名称              | set_type      | 实例名          | 结构体名               | 主要成员                                                   |
|-----------------------|---------------|-----------------|------------------------|------------------------------------------------------------|
| SBS_ViewportInfo      | RenderTarget  | `viewport`      | ViewportInfo           | ortho_matrix, canvas_resolution, viewport_resolution       |
| SBS_CameraInfo        | Camera        | `camera`        | CameraInfo             | projection/view/vp 及逆矩阵, frustum_planes[6], sky, pos  |
| SBS_SkyInfo           | Camera        | `sky`           | SkyInfo                | base_sky_color, sun_direction, sun_color, halo_color, moon |
| SBS_LocalToWorld      | PerFrame      | `l2w`           | LocalToWorldData       | mat4 mats[] (SSBO) 或 mats[L2W_MAX_COUNT] (UBO)           |
| SBS_MaterialInstance  | PerMaterial   | `mtl`           | MaterialInstanceData   | MaterialInstance mi[] (SSBO) 或 mi[MI_MAX_COUNT] (UBO)     |
| SBS_ColorPattle       | PerMaterial   | `color_pattle`  | ColorPattle            | vec4 color[256]                                            |
| SBS_JointInfo         | PerFrame      | `joint`         | JointInfo              | mat4 mats[]                                                |

### 3.2 与 Compositor UBO 的差异

| 数据       | 现有系统 SBS_CameraInfo                                 | Compositor CameraUBO                        |
|------------|------------------------------------------------------|---------------------------------------------|
| 矩阵       | projection, inverse_projection, view, inverse_view, vp, inverse_vp | view, proj, viewProj                        |
| 视锥       | frustum_planes[6], sky(mat4)                          | 无                                           |
| 位置       | pos(vec3), view_line, world_up, billboard_up/right     | cameraPos, cameraPosWorld                    |
| 近远平面   | znear, zfar, use_reversed_z                            | 无                                           |
| 体积       | ~400+ bytes                                            | ~224 bytes                                   |

---

## 4. 材质编译管线

### 4.1 编译流程

```
MaterialPreset (枚举值)
    ↓
CreateMaterialCreateInfo(profile, preset, config)
    ↓  根据 preset 调用对应工厂（如 CreatePureColor3D）
CompileComposedBusinessMaterial(profile, FixedDef, ComposedDef, LogicDef, key, cfg)
    ├─ ValidateMaterialLogicDef(logic)
    ├─ BuildComposedMaterialDefFromLogic(base_def, logic, result)
    │   └─ 按资源名匹配 descriptor → 过滤出使用的描述符
    ├─ BuildVertexGLSLFromBusiness(fixed_def, composed_def)
    │   ├─ 生成 VertexInput 结构体（从 FixedVertexEntry[]）
    │   ├─ 从 VS business 代码提取插值变量
    │   ├─ 注入 VS business 代码
    │   └─ 生成 main()：读取顶点属性 → VertexShaderBusiness() → L2W*VP 变换
    ├─ BuildFragmentGLSLFromBusiness(fixed_def, composed_def, key)
    │   ├─ 生成 FS 输入接口块
    │   ├─ 注入 FS business 代码
    │   └─ 生成 main()：FragColor = FragmentShaderBusiness()
    └─ CompileFixedMaterial(profile, def, vs_glsl, fs_glsl, ...)
        ├─ BuildBindingContract(def.descriptor_entries)
        ├─ new MaterialCreateInfo(&cfg)
        ├─ 添加描述符（UBO/SSBO，方法: AddUBOStruct/SetLocalToWorld/SetMaterialInstance）
        ├─ 添加顶点输入（vsc->AddInput per entry）
        ├─ 设置 MI 结构（mi_glsl_codes + mi_struct_bytes）
        └─ mci->CreateShader()
            ├─ GLSL #version + header
            ├─ ProcDefine()
            ├─ ProcLayout()
            ├─ ProcInput()   — 生成 layout(location=N) in ... 声明
            ├─ ProcMI()      — 生成 MaterialInstance 结构体
            ├─ ProcUBO()     — 生成 layout(set=N,binding=M) uniform ... 声明
            ├─ ProcSSBO()    — 生成 layout(set=N,binding=M) buffer ... 声明
            ├─ ProcOutput()  — 生成 layout(location=0) out vec4 FragColor
            ├─ 附加 functions + main
            └─ CompileToSPV() — glslang → SPIR-V
```

### 4.2 从 MaterialCreateInfo 到可用 Material

```
MaterialManager::CreateMaterial(preset, config)
    ↓
mtl::CreateMaterialCreateInfo(profile, preset, config)
    ↓  返回 MaterialCreateInfo* (含 SPV)
MaterialManager::CreateMaterial(hash_name, mci)
    ├─ RunMaterialCreatePrecheck()       — 缓存检查
    ├─ new Material(name, mci)
    ├─ ExecuteMaterialBuildPipeline(mtl, name, mci, sci_map)
    │   ├─ BuildLegacyShaderModules()    — 创建 ShaderModule
    │   ├─ CollectLegacyDescriptors()    — 提取描述符
    │   ├─ MaterialDescriptorManager()   — 构建描述符集布局
    │   └─ CreatePipelineLayout()        — 创建 VkPipelineLayout
    └─ Material 注册到 cache
```

---

## 5. ECS 绘制管线

### 5.1 18-Phase 执行流水线

```
TickInput → TickTransform → TickCamera → TickLight → TickAnimation
  → TickPhysics → TickScriptUpdate → TickHierarchy → TickBounds
  → RenderCollect → RenderCull → RenderSort
  → RenderBatch → RenderTransformIndex → RenderBufferUpload
  → RenderFrameSync → RenderDrawSubmit → FrameEnd
```

关键 Render 阶段：

| 阶段                | 功能                                                                          |
|---------------------|-------------------------------------------------------------------------------|
| RenderCollect       | 收集所有可见 RenderComponent，按 Material 分组                                |
| RenderCull          | 视锥剔除                                                                      |
| RenderSort          | 对渲染项排序（前向: 前后排序；延迟: 按 Material 排序减少状态切换）            |
| RenderBatch         | 将同 Material 的渲染项合并为 MaterialBatch                                    |
| RenderTransformIndex | 分配 TransformID，填充 TransformAssignmentBuffer                            |
| RenderBufferUpload  | 上传 L2W 矩阵、MI 数据到 GPU                                                 |
| RenderFrameSync     | vkQueueSubmit / fence 同步                                                    |
| RenderDrawSubmit    | 执行绘制命令                                                                  |

### 5.2 绘制调用路径

```
PipelineMaterialRenderer::Render(rcb, batches, ...)
    ├─ BindPipeline(pipeline)                    — vkCmdBindPipeline
    ├─ BindDescriptorSets(material)              — 对每个 DescriptorSetType：
    │   ├─ mp = mtl->GetMP(set_type)             —   获取 MaterialParameters
    │   ├─ mp->Update()                          —   更新描述符集
    │   └─ vkCmdBindDescriptorSets(...)          —   绑定所有非空集
    └─ for each DrawBatch:
        ├─ BindDataBuffer(geom_data_buffer)      — vkCmdBindVertexBuffers
        │                                          + vkCmdBindIndexBuffer
        └─ vkCmdDrawIndexed / vkCmdDraw          — 发出绘制命令
```

### 5.3 MaterialBatch 结构

```cpp
struct DrawBatch {
    uint32_t first_instance;
    uint32_t instance_count;
    const GeometryDataBuffer *geom_data_buffer;  // VABs + IBO
    const GeometryDrawRange *geom_draw_range;    // 顶点/索引范围
};

struct MaterialBatch {
    MaterialPipelineKey key;
    std::vector<RenderItem *> items;
    DrawBatchArray draw_batches;
    PipelineMaterialRenderer *renderer;
};
```

### 5.4 Instance-Rate 属性使用方式

ECS 管线通过以下方式传递实例数据：

1. **RenderTransformIndex** 阶段为每个实例分配一个 TransformID
2. **TransformAssignmentBuffer** 存储所有实例的 L2W 矩阵
3. **MaterialInstanceAssignmentBuffer** 存储所有实例的 MI 数据
4. TransformID 和 MaterialInstanceID 作为 Instance-Rate VBO 上传
5. VS 通过 `GetLocalToWorld()` → `l2w.mats[TransformID]` 获取变换
6. FS 通过 `GetMI()` → `mtl.mi[MaterialInstanceID]` 获取材质参数

---

## 6. Compositor 系统现状

### 6.1 已完成部分（7.1.1-7.1.3）

- **Surface Function（11 个）**: `standard_surface`, `basiclit_surface`, `unlit_color3d_surface`, `unlit_vertexcolor_surface`, `unlit_luminance_surface`, `gizmo3d_surface`, `billboard_texture_surface`, `terrain_grid_surface`, `pbrcolor3d_surface`, `sky_minimal_surface`, `textureblinnphong_surface`
- **Compositor Template（21 个）**: Forward Opaque/Lit/Unlit + Unlit 变体（Normal, VertexColor, Luminance, Luminance2D, Pattle）+ Billboard（Fixed/Dynamic）+ Sky + Terrain Grid
- **Common Module（10 个）**: `surface_interface`, `descriptor_macros`, `scene_ubo`, `l2w_ssbo`, `material_instance_ssbo`, `lighting`, `skylight_simple`, `depth_utils`, `vertex_fetch_ssbo`, `vertex_fetch_vbo`
- **CompositorAssembler**: 路由、注入、组装功能完整，支持 Lit + Unlit 双路径
- **CompositorRenderTest**: 11 个 Phase 全部通过（Phase 1–5 Lit，Phase 6 SSBO，Phase 7–11 Unlit）

### 6.2 与现有系统的核心不兼容点

| 不兼容点                 | 详情                                                            |
|--------------------------|----------------------------------------------------------------|
| 描述符集编号             | 新: set 0-3, 旧: set 1-6                                       |
| CameraUBO 结构           | 新: 精简 224B, 旧: 全量 400B+                                   |
| ViewportUBO              | 新: 在 set=0, 旧: 在 set=1 (RenderTarget)                      |
| 顶点输入                 | 新: inInstanceID 单一属性, 旧: TransformID + MaterialInstanceID  |
| MI 索引方式              | 新: instanceID 直接索引, 旧: MaterialInstanceID 单独属性        |
| L2W 存储                 | 新: 始终 SSBO, 旧: 可选 UBO/SSBO                               |

---

## 7. 总结

现有系统是一个成熟的、完整的渲染管线，其核心特征：

1. **6-Set 描述符布局** — 按语义分类，枚举值即 set 编号
2. **字母序绑定** — `Resort()` 按名字排序分配 binding
3. **Instance-Rate 双属性** — TransformID + MaterialInstanceID 独立索引
4. **SBS 声明式 UBO** — 全量摄像机/天空信息结构
5. **Business Logic 模式** — VS/FS 业务代码由框架包装生成 main()
6. **ECS 批次管线** — 收集→剔除→排序→批组→上传→绘制

要让 Compositor 生成的材质在现有渲染器中工作，需要在**描述符集映射**、**UBO 结构对齐**、**顶点输入兼容**三个维度做桥接。
