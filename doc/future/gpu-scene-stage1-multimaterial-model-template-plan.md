# 纯 GPU Scene 演进阶段 1：多材质 / 跨批次 ModelTemplate 与零子树克隆执行方案

- **文档状态**：待执行（供独立会话直接按步落地）
- **基线分支**：`feature/gpu-scene-stage1-v2`
- **编制日期**：2026-11-24
- **核心原则**：**每一个子步骤都必须独立可编译、测试全绿、示例安全可运行、像素结果可对拍**，严禁跨阶段大爆炸式重写。

---

## 0. 背景与五阶段演进总览

引擎终局目标是实现 **纯 GPU Scene + 模型微型场景树零 CPU 克隆复用 + 真 Meshlet 细粒度剔除渲染**。为确保演进全程不破坏现有工程可用性，整体拆分为五个正交大阶段：

1. **阶段 1（本文档范围）**：补齐 **多材质 / 跨 PSO `ModelTemplate`**，拔除「伪代表图元（Dummy `GeometryData`）」寄生设计与外层 AABB 误剔除缺陷，在现有 `tri_192.mesh` 几何管线下跑通真实多材质 glTF 模型（如 `FlightHelmet`）**「单实例 = 1 个根 CPU Entity」**，并与现有 CPU 克隆树路径完成 **1:1 像素级对拍**。
2. **阶段 2**：将剔除输出从「CPU 预留 `40960` 最坏上限 + 写出 128B 展开矩阵」升级为 **Two-Pass GPU 动态紧凑队列（`VisiblePart { entity_id, part_id }` 8B）+ Mesh Shader `shared` 内存延迟矩阵合成**。
3. **阶段 3**：CPU 收集解耦 —— 建立 VRAM 常驻 `SceneInstancePool`（Dirty Queue 增量同步），消灭每帧 `RenderPrimitiveCollectSystem` 全量实体遍历与 `PrimitiveRenderItem` 构造，激活 `GPUInstancedModelBatchComponent`。
4. **阶段 4**：几何管线独立换芯 —— 接入 `GLTFConvert` 产出的真 Meshlet 数据（Unique Vertices + 8-bit Local Indices），替换 `VertexPassthrough`（`tri_192.mesh`），暂不开逐簇剔除，单独锁定几何解包正确性。
5. **阶段 5**：开启第 3 级 **逐 Meshlet 剔除（球视锥 + 法线锥背面 + Hi-Z）** 与按 Shading Bin 的一维扁平间接派发。

### 阶段 1 严格边界（Scope Guardrails）

- **本阶段必须做**：
  - 让任意静态多节点、多材质、多贴图、甚至跨渲染形态（如单面 Opaque + 双面 Opaque + AlphaMask）的 `.model`，在场景中出现 $N$ 次时，CPU 侧只需创建 **$N$ 个根 `Entity`**（挂载 `Transform + GPUModelRefComponent`），不再克隆内部子节点树与多图元派生子实体。
  - 彻底移除模板实体必须挂载假 `GeometryData`（代表图元）的临时补丁，修复代表图元小包围盒提前误剔大模型的 Bug。
  - 保持旧路径 `StaticMeshModelInstance` 完好可用，作为 A/B 像素对拍真值基线。
- **本阶段明确不做（留待阶段 2~5）**：
  - **不改** `ShaderLibrary/topology/tri_192.mesh` 与片元着色器逻辑；
  - **不改** L2W 展开矩阵池机制（延迟矩阵合成与 Two-Pass 前缀和紧凑化留给阶段 2）；
  - **不删** `RenderPrimitiveCollectSystem` 的帧收集框架（常驻 `SceneInstancePool` 留给阶段 3）；
  - **不碰** Meshlet 运行时格式与 Task Shader（留给阶段 4~5）。

---

## 1. 执行会话强制红线（编码与验证纪律）

在另一会话执行本方案时，必须严格遵守以下仓库规范：

1. **HGL 自有库红线（严禁新增 STL）**：
   - 除 `src/Tools/TexConv` 外，新增/修改引擎代码**严禁引入** `<vector>`、`<string>`、`<unordered_map>`、`<memory>`、`<iostream>`、`printf`、`new`/`delete` 等 STL/CRT 设施（注：历史遗留文件中已有的 `std::vector` 在未重构段不强行全改，但**新写代码、新结构体、新辅助类一律使用 HGL 类型**）。
   - 容器用 `hgl::ValueArray<T>`、`hgl::ManagedArray<T>`、`hgl::UnorderedMap<K,V>`；字符串用 `AnsiString` / `UTF8String` / `OSString`；智能指针用 `hgl::SharedPtr<T>` / `hgl::AutoDelete<T>`；日志用 `<hgl/log/Log.h>`（`GLogInfo` / `FLogInfo` / `LogInfo`）；哈希用 `<hgl/util/hash/FNV1a.h>`。
2. **BufferGen 单一真源纪律**：
   - `ModelTemplateRow`、`ModelPartRow`、`ModelExpansionArena` 已登记在 `src/Tools/BufferGen/buffer_gen.toml`。
   - **严禁手改** `ShaderLibrary/module/descriptor/ssbo/model_template_row.glsl`、`model_part_row.glsl`、`model_expansion_arena.glsl`！修改 C++ 头后必须运行 `BufferGen` 重新生成并通过对齐/偏移静态断言与契约门测试。
3. **有界验证纪律（防挂死）**：
   - 启动任何 Vulkan 示例或图形探针时，**必须设置有限帧或自检环境变量**（如 `CB_SELFCHECK=1`、`CB_FRAMES=10`、`FHF_SELFCHECK=1`、`FHF_FRAMES=10`），严禁无界启动窗口等待人工关闭。
4. **分步检查点纪律**：
   - 每完成一个 Step（1.1 ~ 1.6），先跑定向单测与自检示例，确认全绿后再进入下一步并独立提交。

---

## 2. 现有代码根因剖析（精准定位）

在动手前，需清楚当前 T1~T8 模板原型无法渲染真实多材质模型的 **5 处精确卡点**：

### 卡点 1：`ModelPartRow` 缺纹理配置行号与批次分流键，且 `material_id` 被写死为 0
- **文件**：`inc/hgl/graph/ssbo/ModelTemplateRows.h:169-192`、`src/SceneGraph/asset/ModelTemplateManager.cpp:150-151`、`ShaderLibrary/module/descriptor/ecs/entity_gpu_cull.glsl:570-585`
- **现状**：
  - `ModelPartRow`（128B）仅有 `geometry_id`（@96）、`material_id`（@100）、`part_flags`（@104）、`bucket_slot`（@108），末尾 `uint32_t _pad0[4]`（@112..127，共 16B）闲置。
  - `ModelTemplateManager::RegisterModelTemplate` 第 150 行硬编码 `row.material_id = 0u;`。
  - `entity_gpu_cull.glsl:579` 在调用 `emit_bucket_instance` 时直接传入宿主实体的 `ids_blk.texture_id`，导致同一模型内所有部件被迫采样同一套贴图！

### 卡点 2：合批键与单 `bucket_base` 将同一模型锁死在单一 `MaterialBatch`
- **文件**：`inc/hgl/ecs/core/render/ShaderProgramPipelineKey.h:34-60`、`src/ecs/support/pipeline/PrimitiveBatchPipeline.cpp:270-305, 950-1125`
- **关键机制澄清（好消息）**：
  - 本引擎 BDA 化之后，`MaterialBatch` 的合批键 `ShaderProgramPipelineKey` **仅由 `(ShaderProgram*, Pipeline*, MaterialPipelineConfig)` 决定**！
  - 同一 Shader Domain + 同一渲染状态下的不同材质参数行（`material_id`）和不同贴图组合（`texture_id`，同属一张 `MaterialTextureReferencePool`，批级基址 `texture_reference_base_addr = material_texture_zero_row_gpu` 相同）**天然同属一个 `MaterialBatch`**（例如 `FlightHelmet` 的 6 个 `StandardOpaque` 材质天然同批！）。
  - 更重要的是：`GPUCullSystem` 的 `BatchInfoRow[0..1023]`、`icb_mesh_tasks`、`cmds_out`、`l2w_out`、`mtl_out` 在每个 Pass 内本就是**跨所有 `MaterialBatch` 统一线性编址**的（每个 `MaterialBatch` 只认领自己的 `[cmd_base, cmd_base + cmd_count)` 桶切片，`PipelineMaterialRenderer.cpp:45-75`）。
- **真正卡住跨 PSO 模型（如 Opaque 树干 + AlphaMask 树叶 + 双面贴花）的原因**：
  - `model_bucket_bases` 表目前按 `[pass * max_visible + entity_index]` 一对一存储**单个** `uint32_t` 基址，且 `batch_tag` 也只存单个批号，导致一个 Entity 的部件无法同时落入两个不同的 `MaterialBatch` 切片。

### 卡点 3：寄生在宿主「伪代表图元（Dummy `GeometryData`）」上，引发外层误剔除与容量浪费
- **文件**：`example/Instancing/GPUModelChessboard.cpp:298-308`、`ShaderLibrary/module/descriptor/ecs/entity_gpu_cull.glsl:616-685`、`src/ecs/support/pipeline/PrimitiveBatchPipeline.cpp:912-944`
- **现状**：
  1. 模板实体必须挂一个假的 `GeometryData`（如单位立方体 `cube_asset`）才能通过 `RenderPrimitiveCollectSystem` 并获得非零 `render_ids.geometry_id`。
  2. `entity_gpu_cull.glsl:620` 写了 `if (geometry_id == 0u) return;`，且在第 647~684 行**先拿这个假 `geometry_id` 的 AABB 做了一次 6 平面视锥剔除**！一旦模型真实尺寸大于假代表图元（或中心偏移），实例未出视锥就会被提前误剔，`expand_template_entity` 根本没机会执行！
  3. 基础聚类（`PrimitiveBatchPipeline.cpp:935`）还会为这个从不绘制的代表几何体额外分配 1 个桶容量（4 部件棋盘实际每实例占 5 行竞技场容量）。

### 卡点 4：`CPUCullReferenceSystem` 对拍台架尚未感知「无 `GeometryData` 模板实体」与「展开计数放大」
- **文件**：`src/ecs/support/cull/CPUCullReferenceSystem.cpp:170-265, 325-375`
- **现状**：
  1. `PerformCPUCulling()`（第 335 行）见 `ids->geometry_id == 0` 直接跳过，若模板实体去掉假 `GeometryData`，CPU/GPU 可见位图对拍会报 mismatch。
  2. `CheckShadowFacts()`（第 174 行、第 258 行）：
     - 展开部件的矩阵行号写往 `GetModelExpansionBaseIndex() + inst_row`，不在实体基础 `row[Transform]` 反查表 `l2w_row_to_entity_` 内，直接反查会报 `slice row→entity 反查失败`；
     - 第 258 行硬判 `if (sum > gpu_count_) GLogError("两本账越界")` —— 单实体单实例时代 `Σ桶实例数 <= 可见实体数` 恒成立，但 1 个可见模板实体展开出 $K$ 个可见部件后，`sum`（部件实例总数）天然可能大于 `gpu_count_`（可见根实体数）！

### 卡点 5：正式资产加载链（`.model` + `SelfContainedModelMaterials`）与 `ModelTemplateManager` 断连
- **文件**：`inc/hgl/graph/mesh/LoadStaticMesh.h`、`example/Material/GLTF_FlightHelmet.cpp`、`inc/hgl/graph/asset/LevelLoader.h`
- **现状**：`BuildSelfContainedModelMaterials` + `LoadModelAsPrimitiveAssets` 产出的多材质配方与 `GlobalSSBOBinding` 只喂给了 `StaticMeshModelInstance`（克隆 Entity 树），缺少一键物化 `MaterialRuntimeRow` 并打包进 `ModelTemplatePool / ModelPartPool` 的标准装配器。

---

## 3. 阶段 1 目标架构设计

### 3.1 GPU 表结构无损扩展（保持 128B 步长与既有对齐不变）

#### （1）`ModelPartRow`（`inc/hgl/graph/ssbo/ModelTemplateRows.h`）
利用末尾现成的 `uint32_t _pad0[4]`（16 字节），填入 `texture_id` 与 `shading_group`，总大小严格保持 **128 字节**：

```cpp
// ModelPartRow.part_flags 新增位定义：
constexpr uint32_t kModelPartFlagCanCastShadow = 1u << 0; ///< 阴影 pass 可投影
constexpr uint32_t kModelPartFlagIsOpaque      = 1u << 1; ///< 不透明部件
constexpr uint32_t kModelPartFlagHasMaterial   = 1u << 2; ///< 部件自带显式 material_id / texture_id（置位时允许合法行号 0；未置位时回退继承宿主实体材质，兼容旧单材质模板）

struct alignas(16) HGL_SSBO_BDA ModelPartRow
{
    Matrix4f local_matrix;                                              // @0   (64B) 相对模型根的局部位姿
    Vector4f aabb_center_radius = Vector4f(0.0f, 0.0f, 0.0f, 0.0f);     // @64  (16B) xyz 中心，w 半径（<0 始终可见哨兵）
    Vector4f aabb_extents       = Vector4f(0.0f, 0.0f, 0.0f, 0.0f);     // @80  (16B) AABB 半长

    uint32_t geometry_id   = 0;                           // @96  (4B) Geometry 池行号（0 = 跳过）
    uint32_t material_id   = 0;                           // @100 (4B) 材质 SSBO 数据行号（MaterialRuntimeRow::data_index_row）
    uint32_t part_flags    = 0;                           // @104 (4B) kModelPartFlag*
    uint32_t bucket_slot   = kModelPartBucketSlotNone;    // @108 (4B) 所属 shading_group 内的几何去重槽号

    uint32_t texture_id    = 0;                           // @112 (4B) MaterialTextureConfiguration::row_index（Forward/Shadow 同源）
    uint32_t shading_group = 0;                           // @116 (4B) 模板内 Shading Group 序号 [0, shading_group_count)
    uint32_t _pad0[2]      = { 0u, 0u };                  // @120 (8B) 补足 128B
};
// 必加 static_assert：sizeof == 128, offsetof(texture_id) == 112, offsetof(shading_group) == 116
```

> **为什么需要 `kModelPartFlagHasMaterial` 标志位，而不是靠 `material_id != 0` 判断？**
> 因为材质 SSBO 池或纹理配置池中 `row == 0` 可能是合法的第 0 行数据（例如 `FlightHelmet.material_data.toml` 的首个材质或无贴图纯因子材质 `texture_id == 0`）！用显式 flag 位区分「模板自带材质（直接采用 `part.material_id` 与 `part.texture_id`）」与「无材质纯几何模板（如 `GPUModelChessboard` 旧用法，回退取宿主实体的 `ids_blk.material_id / texture_id`）」，既无 `0` 号歧义，又做到 100% 向下兼容。

#### （2）`ModelTemplateRow`（`inc/hgl/graph/ssbo/ModelTemplateRows.h`）
利用现有的 `_pad0[1]`（@44），记录本模板包含的 Shading Group 数量，其余所有字段偏移分毫不改：

```cpp
struct alignas(16) HGL_SSBO_BDA ModelTemplateRow
{
    uint32_t part_offset = 0;                                        // @0
    uint32_t part_count  = 0;                                        // @4
    uint32_t lod_proxy_template_ids[kModelLodProxyCount] = {0,0,0};  // @8
    float    lod_screen_ratios[kModelLodProxyCount]      = {0,0,0};  // @20
    uint32_t shadow_proxy_template_id = 0;                           // @32
    uint32_t flags = 0;                                              // @36
    uint32_t part_bucket_count   = 0;                                // @40 本模板所有 shading_group 的桶数总和
    uint32_t shading_group_count = 1;                                // @44 模板内 Shading Group 数（至少为 1）
    Vector4f local_bound_center_radius = Vector4f(0,0,0,0);          // @48
    Vector4f local_bound_extents       = Vector4f(0,0,0,0);          // @64
    uint32_t _pad1[12] = { 0u };                                     // @80..127
};
```

#### （3）`ModelExpansionArena`（`inc/hgl/graph/ubo/ModelExpansionArena.h`）跨批次桶基址二级表
利用现成的 `uint64_t reserved3`（@40，保持 `sizeof(ModelExpansionArena) == 48` 不变）：

```cpp
struct alignas(16) HGL_SSBO_ROW ModelExpansionArena
{
    uint32_t flags = 0;                     // 位 0 = 展开启用
    uint32_t l2w_base_row = 0;              // 本帧槽展开矩阵段起始行号
    uint32_t l2w_row_capacity = 0;          // 展开段行数上限
    uint32_t bucket_base_count = 0;         // per-实体表行数

    uint64_t bucket_base_addr = 0;          // 一级表：per-(pass, entity_slot) → group_table_offset（u32）
    uint64_t prev_l2w_base_row = 0;         // 上一帧展开矩阵段起始行号
    uint64_t custom_data_addr = 0;          // 实例自定义数据输出表基址
    uint64_t group_bucket_base_addr = 0;    // 二级表（原 reserved3）：group_bucket_bases[group_table_offset + part.shading_group] → 绝对桶号
};
```

---

### 3.2 跨 `MaterialBatch`（多 Shading Group）寻址机制

这是阶段 1 最精巧、改动最小的设计：

1. **什么是 `shading_group`？**
   - 同一个模型内部的所有部件，按其材质物化后的 **管线批次身份**（即 `(forward ShaderProgramPipelineKey, shadow ShaderProgramPipelineKey)`，等价于 `MaterialRuntimeRow` 的 `(variant, shadow_variant, pipeline_config)`）聚类为 $G$ 个组（$G = \text{shading\_group\_count}$，常见模型 $G = 1 \sim 4$）：
     - 例 1：`FlightHelmet` 有 6 个部件、6 套不同贴图与 PBR 因子，但全部是单面 `StandardOpaque` ⇒ **$G = 1$**，所有部件 `shading_group = 0`，`bucket_slot = 0..5`！
     - 例 2：一棵树含「不透明树干（`StandardOpaque`）」+「双面镂空树叶（`StandardAlphaMask`）」⇒ **$G = 2$**，树干 `shading_group = 0`，树叶 `shading_group = 1`；各自组内 `bucket_slot` 从 `0` 开始按 `geometry_id` 去重编号。
2. **CPU 装批时怎么填表（`PrimitiveBatchPipeline::TryApplyGPUCullOverride`）？**
   - 在每个 Pass（`0` 主帧 / `1` 阴影）开始装批时：
     - 每个被收集到的 `(模板实例集 / 宿主覆盖组)` 在二级表 `group_bucket_bases[]` 中分配一段连续的 $G$ 个 `uint32_t` 槽位（初值全填 `0xFFFFFFFFu`），记起始下标为 `group_table_offset`。
     - 该模板的所有实例在 `model_bucket_bases[pass * max_visible + entity_index]` 中统一写入 `group_table_offset`，并在 `tags[entity_index]` 写入有效标记（见 3.4 节，仅作「该实体在本 Pass 已接管」判据）。
   - 遍历到具体某个 `MaterialBatch *b` 时：
     - 若模板 `T` 的第 `g` 个 `shading_group` 归属本批 `b`，则在 `b` 的桶列表中追加该组的 `group_bucket_count[g]` 个几何桶（容量累加 `实例数 × 该槽部件数`），并把二级表对应项填上绝对桶基址：
       ```cpp
       group_bucket_bases[group_table_offset + g] = bucket_first + local_group_bucket;
       ```
     - 若某一组 `g` 在当前 Pass 不参与渲染（例如纯半透明玻璃部件在 Shadow Pass 不投阴影），其 `group_bucket_bases[group_table_offset + g]` 保持 `0xFFFFFFFFu`。
3. **GPU 展开时怎么寻址（`entity_gpu_cull.glsl::expand_template_entity`）？**
   - 仅需 4 行代码即可将任意部件路由到正确的 `MaterialBatch` 切片中：
     ```glsl
     const uint group_table_offset = model_bucket_bases[blk * pc.max_visible_count + entity_slot];
     if (group_table_offset == MODEL_BUCKET_BASE_INVALID)
         return;

     // 在循环每个部件 part 时：
     const uint tag_bucket_base =
         (arena.group_bucket_base_addr != uint64_t(0))
             ? ModelGroupBucketBaseRef(arena.group_bucket_base_addr)
                   .bases[group_table_offset + part.shading_group]
             : group_table_offset; // 兼容回退

     if (tag_bucket_base == MODEL_BUCKET_BASE_INVALID)
         continue; // 该部件所属 shading_group 在本 pass 不绘制（如透明件不投阴影）

     const uint bucket = tag_bucket_base + part.bucket_slot;
     ```
   - 材质与纹理解析：
     ```glsl
     const bool has_part_mtl = (part.part_flags & MODEL_PART_FLAG_HAS_MATERIAL) != 0u;
     const uint emit_mtl_id  = has_part_mtl ? part.material_id : ids_blk.material_id;
     const uint emit_tex_id  = has_part_mtl ? part.texture_id  : ids_blk.texture_id;
     ```

---

### 3.3 CPU 侧模板渲染元数据与「去伪代表图元」收集链路

为了让挂载 `GPUModelRefComponent` 的根 Entity **彻底摆脱伪 `GeometryData`（以及多材质模板摆脱宿主 `MaterialData`）**，在 CPU 侧补充一张与世界（`ECSContext`）绑定的轻量注册表（例如 `ModelTemplateMaterialTable`，由 `ECSContext` 持有，遵循 `MaterialRuntimeTable` 同范式，内部使用 HGL 容器）：

1. **数据结构**：
   ```cpp
   struct TemplateShadingGroupDesc
   {
       MaterialRuntimeRowID   runtime_row = INVALID_MATERIAL_RUNTIME_ROW_ID; ///< 该组的代表运行期材质行（提供 ShaderProgram/Pipeline/pipeline_config/texture_reference_base_addr）
       ValueArray<uint32_t>   slot_geometry;  ///< 组内各 bucket_slot 对应的 geometry_id
       ValueArray<uint32_t>   slot_count;     ///< 组内各 bucket_slot 在单实例中出现的部件次数
       bool                   casts_shadow = true; ///< 组内是否存在 CanCastShadow 部件
   };

   struct TemplateRenderBinding
   {
       uint32_t                             template_id = 0;
       ValueArray<MaterialRuntimeRowID>     owned_part_rows; ///< 本模板持有的全部部件 MaterialRuntimeRowID（析构/注销时 Release 引用计数）
       ValueArray<TemplateShadingGroupDesc> groups;          ///< 长度 == ModelTemplateRow.shading_group_count
   };
   ```
   > **引用计数生命周期**：模板注册材质绑定时对每个部件 `Intern` 出的 `MaterialRuntimeRowID` 加引用（`AddRef`），保证即使没有任何独立 `Entity` 挂 `MaterialData`，这些共享材质行及其 `MaterialTextureConfiguration` 在 VRAM 中也常驻有效；模板注销或 `ECSContext` 销毁时统一 `Release`。

2. **`RenderPrimitiveCollectSystem` 收集改造（消除假 `GeometryData`）**：
   - 当前收集循环只扫带 `GeometryData` 的实体。改造后支持两条模板入口：
     - **模式 A（自包含多材质模板，推荐主路径）**：实体挂 `Transform + GPUModelRefComponent`，且 `ModelTemplateMaterialTable` 中查得到 `template_id` 的 `TemplateRenderBinding`（无需挂 `GeometryData` 和 `MaterialData`）：
       - 在主帧 Pass（`pass_index == 0`）：对 `binding.groups` 中的每个 `g`（通常仅 1~3 个），使用 `groups[g].runtime_row` 生成一个标记了 `(template_id, shading_group = g)` 的轻量 `PrimitiveRenderItem`（`draw_key.geometry_id = 0`，标记 `is_template_carrier = true`）投入对应的 `MaterialBatch`！
       - 在阴影 Pass（`pass_index == 1`）：仅对 `groups[g].casts_shadow == true` 且阴影变体有效的组生成载体项投入对应阴影 `MaterialBatch`。
       - 调用统一的模板事实发布函数（更新 `EntityGPUTable::SetVisible`、`SetShadowFacts`、`SetModelFacts`；`EntityRenderIDTable` 保持 0 即可，因为部件自带材质与几何）。
     - **模式 B（宿主材质覆盖单组模板，兼容 `GPUModelChessboard`）**：实体挂 `Transform + MaterialData + GPUModelRefComponent`（**不再需要挂假的 `GeometryData` `cube_asset`！**）：
       - 走宿主 `MaterialData` 物化出 1 条 `MaterialRuntimeRow`，将 `EntityRenderIDTable` 的 `(material_id, texture_id)` 发布（`geometry_id` 填 0 或模板首几何均可，但基础聚类跳过模板载体项，不再浪费 1 个假几何桶！）。
   - **`PrimitiveBatchPipeline` 装批改造**：
     - 在 `FinalizeBatch` 中：因为载体 `PrimitiveRenderItem` 携带了该组真实的 `MaterialRuntimeRowID`，`batch.texture_reference_base_addr` **无需改一行代码就能自动从载体项的 `mat_row->material_texture_zero_row_gpu` 正确提取**！
     - 在 `TryApplyGPUCullOverride` 基础聚类循环中：遇到 `is_template_carrier`（或挂载了有效 `GPUModelRefComponent` 的项），**跳过基础单图元桶分配**（不再要求 `ids->geometry_id != 0`，也不再浪费 `+1` 代表几何体桶容量！），直接进入下方的模板桶组分配逻辑，按 `(template_id, shading_group)` 追加 `slot_geometry` 并累加 `slot_count`。
   - **P2/P3 主帧收集快照（`MainFrameTopologySnapshot`）适配**：
     - `SnapshotItemRecord` 增加 `uint32_t template_id = 0; uint16_t shading_group = 0;`（或首版在检测到世界存在无 `GeometryData` 模板实体时安全旁路快照回放，待 Step 1.4 稳固后补上这 2 个字段，保证快照快路径同样支持纯模板实体）。

---

### 3.4 修正 `entity_gpu_cull.glsl` 外层剔除与 `CPUCullReferenceSystem` 对拍

1. **`entity_gpu_cull.glsl::main()` 精确流转修正**：
   - 将 `expansion_on` 判定提前到 `render_ids.geometry_id == 0u` 早退之前！
   - 分支逻辑：
     - **若 `expansion_on == true`（模板实体）**：
       - 不读 `render_ids.geometry_id`，直接读 `ModelTemplateRowData base_tpl = model_templates[entity.model_template_id];`（若 `base_tpl.part_count == 0u || base_tpl.part_offset == 0u` 则 `return`）。
       - 用 `base_tpl.local_bound_center_radius` 与 `base_tpl.local_bound_extents` 乘 `l2w_matrix` 执行与原 AABB 完全同式的 **6 平面视锥测试**（`w < 0.0` 仍作为「始终可见」哨兵），产出 `visible` 并写入全局 `visible_indices` / `draw_count`！
       - **代理包围盒防误剔**：在 `ModelTemplateManager::SetModelTemplateProxies` 登记代理时，把 `base_tpl.local_bound_*` 更新为「本体包围盒 ∪ 所有 LOD 代理包围盒」的并集，确保外层 6 平面粗剔除绝不会误杀比本体略大的低模代理。
     - **若 `expansion_on == false`（传统单图元实体）**：
       - 保持 `if (render_ids.geometry_id == 0u) return;` 及 `geometry_aabbs[geometry_id]` 6 平面剔除**逐字节不变**。
   - Pass 循环（`blk = 0..1`）：
     - 模板实体只需检查 `model_bucket_bases[blk * pc.max_visible_count + entity_index] != MODEL_BUCKET_BASE_INVALID` 即可进入 `expand_template_entity`，不再受限于单批 `batch0 / batch1` 标签。

2. **`CPUCullReferenceSystem` 同步对齐**：
   - `PerformCPUCulling()`：若 `row->model_template_id != 0`（且世界已启用模型展开），直接从 `global_registry` 读取 `ModelTemplateRow` 的 `local_bound_*` 执行完全一致的 6 平面 AABB 视锥测试，不再因 `ids->geometry_id == 0` 跳过；保证每 61 帧的 CPU/GPU 位图对拍 **0 mismatch**。
   - `CheckShadowFacts()`：
     - 若 `l2w_row >= transform_buffer->GetModelExpansionBaseIndex()`，识别为「模板展开合成矩阵行」：通过 `mtl_rows[r]` 校验或单独统计展开部件行，不报 `l2w_row_to_entity_ 反查失败`；
     - 将 `sum > gpu_count_` 越界断言修正为：`非展开实例数 <= gpu_count_`（或仅保留 `overflow == 0` 作为切片容量硬判据，并在统计日志中分别打印 `global_entities` 与 `emitted_instances`）。

---

### 3.5 `.model` 自包含多材质模型一键模板装配器

在 `inc/hgl/graph/asset/` 或 `inc/hgl/ecs/support/` 提供标准高层构建入口（与 `StaticMeshModelInstance::Instantiate` 平行对照）：

```cpp
/**
 * 将已解析的 `.model` 内容（节点树 + PrimitiveAsset 数组 + 自包含材质表）
 * 一键注册为多材质 GPU 模型模板，并在目标 ECSContext 中绑定材质运行期行。
 *
 * 成功后调用方只需创建 1 个根 Entity，挂载 Transform + GPUModelRefComponent{out_template_id}，
 * 即可在任意多实例位置渲染完整模型（无需创建子节点 Entity、无需挂 GeometryData/MaterialData）。
 */
bool BuildMultiMaterialModelTemplate(
    ecs::ECSContext                        *world,
    const GeometryVertexFormat             &gvf,
    const hgl::ValueArray<PrimitiveAsset>  &assets,   // 或兼容 LoadStaticMesh 产出的数组视图
    const hgl::ValueArray<StaticMeshNode>  &nodes,
    const SelfContainedModelMaterials      &materials,
    uint32_t                               &out_template_id);
```

内部流程：
1. 遍历 `assets`：根据每个 `PrimitiveAsset::GetMaterialIndex()` 从 `materials.recipes` 与 `materials.bindings` 取得配方与 `GlobalSSBOBinding`；
2. 调用世界材质物化入口（复用 `RenderPrimitiveCollectSystem` / `MaterialRuntimeTable` 同一套材质变体解析与纹理配置落池逻辑），取得每个图元的 `MaterialRuntimeRowID`、`material_id`（`data_index_row`）、`texture_id`（`material_texture_configuration.row_index`）及阴影投射属性；
3. 按 `ShaderProgramPipelineKey`（Forward + Shadow 变体组合）对部件归组，生成 `shading_group`（`0 .. G-1`）与组内按 `geometry_id` 去重的 `bucket_slot`；
4. 写入 `GlobalSSBOBufferRegistry` 的 `ModelPartPool` 与 `ModelTemplatePool`，并将 `TemplateRenderBinding` 登记到 `world` 的 `ModelTemplateMaterialTable`。

---

## 4. 分步执行路线图（Step 1.1 ~ Step 1.6）

> **执行要求**：严格按以下 6 步顺序推进。每步结束必须编译通过、运行所列验证命令并确认全绿，再做一次独立 Git Commit。

### Step 1.1：数据结构无损扩展与 BufferGen 重新生成（纯布局层，零行为变更）
- **修改文件**：
  - `inc/hgl/graph/ssbo/ModelTemplateRows.h`：`ModelPartRow` 增加 `texture_id`（@112）、`shading_group`（@116）、`kModelPartFlagHasMaterial`；`ModelTemplateRow` 将 `_pad0[0]` 具名为 `shading_group_count = 1`（@44）；补齐全部 `static_assert`。
  - `inc/hgl/graph/ubo/ModelExpansionArena.h`：将 `reserved3` 具名为 `group_bucket_base_addr`（@40），保持 48B 大小不变。
  - `src/SceneGraph/asset/ModelTemplateManager.cpp`：`RegisterModelTemplate` 默认填 `trow.shading_group_count = 1u`、`row.shading_group = 0u`、`row.texture_id = 0u`；`SetModelTemplateProxies` 在合并代理时同步取本体与代理 `local_bound` 的并集包围盒。
  - 运行 `BufferGen` 工具更新 `ShaderLibrary/module/descriptor/ssbo/model_template_row.glsl`、`model_part_row.glsl`、`model_expansion_arena.glsl`。
  - `src/ecs/tests/TestGPUModelTemplate.cpp`：新增新字段偏移、默认值及代理并集包围盒断言。
- **验收命令**：
  - 构建 `BufferGen`、引擎库与 `TestGPUModelTemplate`；
  - 运行 `ctest --test-dir build -R "TestGPUModelTemplate|parity" --output-on-failure`。

---

### Step 1.2：打通二级 `group_bucket_bases` 表与部件显式 `(material_id, texture_id)` 着色器消费
- **修改文件**：
  - `inc/hgl/ecs/support/cull/GPUCullSystem.h` & `src/ecs/support/cull/GPUCullSystem.cpp`：
    - 在模型展开缓冲中为每帧槽追加二级表 `group_bucket_bases`（例如每 Pass `4096` 个 `uint32_t` = 32KB/帧槽，绰绰有余），在 `WriteModelExpansionArena` 中下发 `arena.group_bucket_base_addr`；
    - 提供 `AllocateModelGroupBucketBases(pass, group_count)` / `SetModelGroupBucketBase(pass, offset, group_idx, bucket_base)` 接口，保留单组快捷包装兼容旧调用。
  - `src/ecs/support/pipeline/PrimitiveBatchPipeline.cpp`：模板组分配接入二级表（单组模板写 `group_count = 1`）。
  - `ShaderLibrary/module/descriptor/ecs/entity_gpu_cull.glsl`：
    - 接入 `group_bucket_base_addr` 二级寻址（若某组为 `0xFFFFFFFFu` 则 `continue` 跳过该部件）；
    - 接入 `MODEL_PART_FLAG_HAS_MATERIAL` 分支，显式选择 `part.material_id` 与 `part.texture_id`。
  - `src/ecs/tests/TestGPUModelExpansionParity.cpp`：同步更新 CPU 镜像函数，新增「同模板多部件不同 `(material_id, texture_id)`」与「跨 `shading_group` 分流 / 阴影 Pass 局部组跳过」手算表对拍用例。
- **验收命令**：
  - 运行 `ctest --test-dir build -R "TestGPUModel" --output-on-failure`；
  - 运行 `$env:CB_SELFCHECK="1"; .\build\bin\GPUModelChessboard.exe`（确认摘要与改造前逐字一致）。

---

### Step 1.3：拔除「伪代表图元（Dummy `GeometryData`）」、修复外层误剔除、对齐 `CPUCullReferenceSystem`
- **修改文件**：
  - `ShaderLibrary/module/descriptor/ecs/entity_gpu_cull.glsl`：
    - `main()` 中提前判定 `expansion_on`：模板实体不再要求 `render_ids.geometry_id != 0u`，外层视锥剔除改用 `model_templates[entity.model_template_id].local_bound_*`；
    - Pass 循环允许模板实体凭有效的 `model_bucket_bases` 直接进入 `expand_template_entity`，不强依赖单批 `batch_tag`。
  - `src/ecs/support/cull/CPUCullReferenceSystem.cpp`：
    - `PerformCPUCulling()` 对 `model_template_id != 0` 实体采用模板 `local_bound_*` 做 6 平面剔除；
    - `CheckShadowFacts()` 正确处理展开段 `l2w_row` 与展开后实例计数关系。
  - `src/ecs/systems/render/collect/RenderPrimitiveCollectSystem.cpp` & `src/ecs/support/pipeline/PrimitiveBatchPipeline.cpp`：
    - 支持不挂 `GeometryData` 的 `GPUModelRefComponent` 实体被收集，且在 `TryApplyGPUCullOverride` 基础聚类中不再为模板实体分配多余的代表几何体桶。
  - `example/Instancing/GPUModelChessboard.cpp` & `example/Benchmark/01_ChessboardStress.cpp`：
    - 删掉每个棋盘实体上寄生的 `GetOrCreateGeometryData`（`cube_asset`）代码，验证 4 部件棋盘每实例竞技场占用从 **5 行精确回归为 4 行**！
- **验收命令**：
  - `$env:CB_SELFCHECK="1"; .\build\bin\GPUModelChessboard.exe`（像素摘要必须 100% 不变，且 Debug 下 `CPUCullReferenceSystem` 0 mismatch、0 error）；
  - 跑 `01_ChessboardStress`（有限帧参数）确认无黑屏、无切片溢出。

---

### Step 1.4：落地世界级 `ModelTemplateMaterialTable` 与多材质 / 跨批次模板装配器
- **新增 / 修改文件**：
  - 新增 `inc/hgl/ecs/support/material/ModelTemplateMaterialTable.h`（及 `.cpp`，严格使用 HGL 容器与日志宏）：管理 `template_id → TemplateRenderBinding` 生命周期与 `MaterialRuntimeRow` 引用计数。
  - `inc/hgl/ecs/core/Context.h` / `Context.cpp`：持有 `ModelTemplateMaterialTable`，析构/清理时有序释放引用。
  - 新增高层打包接口（如 `inc/hgl/ecs/support/material/BuildModelTemplate.h` + `.cpp`）：输入 `(ECSContext*, GeometryVertexFormat, PrimitiveAsset[], StaticMeshNode[], SelfContainedModelMaterials)`，自动物化各材质 `MaterialRuntimeRow`、划分 `shading_group`、填充 `ModelPartRow { geometry_id, material_id, texture_id, shading_group, bucket_slot, part_flags }` 并注册进 `ModelTemplateManager` 与 `ModelTemplateMaterialTable`。
  - `src/ecs/systems/render/collect/RenderPrimitiveCollectSystem.cpp`：
    - 对纯模板实体（仅 `Transform + GPUModelRefComponent`，无 `GeometryData`、无 `MaterialData`），按 `TemplateRenderBinding::groups` 将载体项分别投入各自对应的 `MaterialBatch`（主帧 Pass 与阴影 Pass 各自路由），并适配 `MainFrameTopologySnapshot` 快照记录。
- **验收命令**：
  - 新增单测/集成测试验证：
    1. 单模板含多个不同 `(material_id, texture_id)` 同 Shader 部件；
    2. 单模板含 2 个不同 `MaterialPipelineConfig`（例如单面 Opaque + 双面 Opaque 或 AlphaMask）跨 `MaterialBatch` 部件，验证 `group_bucket_bases` 分流到两个不同 `MaterialBatch` 的各自桶内且 `overflow == 0`。

---

### Step 1.5：`GLTF_FlightHelmet`（6 材质多节点模型）双轨像素级 A/B 对拍与多实例验证
- **修改文件**：
  - `example/Material/GLTF_FlightHelmet.cpp`：
    - 支持环境变量切换装配模式（例如 `FHF_MODE=tree|template`，并在稳定后可将 `template` 设为默认或新增 `FHF_GRID=NxN` 阵列压测参数）：
      - `tree` 模式：走原有 `StaticMeshModelInstance`（创建全部子节点 CPU Entity + 独立 `GeometryData`/`MaterialData`）；
      - `template` 模式：调用 Step 1.4 的 `BuildMultiMaterialModelTemplate`，场景中**只创建 1 个根 `Entity`**（挂单位 `Transform` + `GPUModelRefComponent`）！
    - 增加 `FHF_GRID=N`（默认 `1`）：当 `N > 1`（如 `FHF_GRID=10` 即 100 顶头盔阵列）时，用同 1 个 `template_id` 创建 $N \times N$ 个根 Entity（验证 CPU 仅 $N^2$ 个实体即可在多处复用完整多材质模型树）。
  - （可选顺手衔接）`src/SceneGraph/asset/LevelLoader.cpp`：当静态关卡节点引用 `.model` 且无需逐子节点 CPU 挂脚本时，可选择走 GPU 模板化装配。
- **硬核验收判据（最关键里程碑）**：
  1. **1:1 像素与摘要对拍**：
     - `$env:FHF_MODE="tree"; $env:FHF_SELFCHECK="1"; .\build\bin\GLTF_FlightHelmet.exe` ⇒ PASS（FNV `0xC419F5C2A0796C8B`）；
     - `$env:FHF_MODE="template"; $env:FHF_SELFCHECK="1"; .\build\bin\GLTF_FlightHelmet.exe` ⇒ **同样 PASS**（若因浮点结合律微差 `(Instance_L2W * Part_Local) * v` 在单位根矩阵下有任何 1 ULP 差异，先用 `FHF_DUMP` 对比两张 PNG 的 `max_abs_diff` 与 `mean_lum`——在根为单位阵时 `I * node.worldMatrix == node.worldMatrix`，理论应当 **逐字节 0 差异**！）。
  2. **多实例阵列验证**：
     - `$env:FHF_MODE="template"; $env:FHF_GRID="10"; $env:FHF_FRAMES="15"; .\build\bin\GLTF_FlightHelmet.exe` ⇒ 100 顶 6 材质头盔正常渲染，世界存活模型 Entity 数 = 100（外加相机/灯光），GPU 剔除与阴影无任何 `overflow` 或 Validation Error。

---

### Step 1.6：全量回归门禁（Debug + Release Debug-API）
- **验证清单**：
  1. 运行全量 ECS CTest：
     `ctest --test-dir build --output-on-failure`
  2. 运行代表性示例 `SELFCHECK` 冒烟（全部带自退出）：
     - `CB_SELFCHECK=1` → `GPUModelChessboard.exe`
     - `FHF_SELFCHECK=1` → `GLTF_FlightHelmet.exe`（`tree` 与 `template` 双模式）
     - `DH_SELFCHECK=1` → `GLTF_DamagedHelmet.exe`
     - `CULL_FRAMES=15` → `GPUDrivenCull.exe`
     - `CSM_FRAMES=15` → `CascadeShadowMap.exe`
  3. 检查编译期开关门禁：`scripts/ci/verify_debug_api_release.ps1`（确保 `#if ULRE_ECS_DEBUG_API` 下所有新增对拍断言在 Release 强制开启配置下零编译错误、零运行告警）。
  4. 检查新增/修改文件零违规 STL 头文件引入。

---

## 5. 常见陷阱备忘（防踩坑清单）

1. **AMD / Vulkan BDA 空指针陷阱**：
   - 在 `entity_gpu_cull.glsl` 中，凡是新增的 BDA 地址（如 `arena.group_bucket_base_addr`），**必须先判 `!= uint64_t(0)` 再解引用**，且不要依赖逻辑与短路将解引用写在局部初始化里（GLSL 编译器可能把 load 外提导致 `VK_ERROR_DEVICE_LOST`；严格仿照现有 `custom_data_addr` 的先判空提取模式）。
2. **4 行对齐约束（`row_cursor = (row_cursor + capacity + 3u) & ~3u`）**：
   - `PrimitiveBatchPipeline.cpp:1084` 中每个桶的 `row_base` 必须保持 **4 的整数倍**（保证 `mtl_out` 16B/行 与 `l2w_out` 4B/行 均满足 16B buffer_reference 对齐），新增跨组桶分配时切勿破坏该对齐推进式。
3. **非均匀缩放下的法线与包围球半径**：
   - 矩阵合成顺序严格为 `part_l2w = root_l2w * part.local_matrix`（左乘实例根矩阵、右乘部件相对模型根矩阵，`TestGPUModelExpansionParity` A 组已锁死），切勿写反。
   - 包围球缩放取三轴列向量长度最大值 `max(length(m[0].xyz), max(length(m[1].xyz), length(m[2].xyz)))`，C++（`GetMaxWorldScaleRaw`）与 GLSL 两端必须保持完全同式，不得单边加 `epsilon` 保护以免破坏对拍。
4. **模板部件超过 64 个时的可见掩码语义**：
   - `GPUModelRefComponent::part_visibility_mask` 为 `uint64_t`（默认全 `1` 即 `~0ull`）。**注意 GLSL 中 `uint64_t(1) << p` 或 `mask >> p` 当 `p >= 64u` 时移位是未定义行为**！
   - 必须在 `entity_gpu_cull.glsl` 中把部件掩码判据加固为：
     ```glsl
     if (p < 64u && ((entity.model_part_mask >> p) & uint64_t(1)) == uint64_t(0))
         continue;
     ```
     这样 `p >= 64` 的部件默认保持可见（不被未定义移位随机剔除），彻底解除模型部件数 > 64 时的渲染隐患！
