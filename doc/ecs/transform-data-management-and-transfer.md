# ECS Transform 数据管理与传递技术分析

## 1. 分析范围与目标

本文面向当前 ULRE ECS 实现，分析 Transform 数据从 CPU 侧生成、管理、更新，到 GPU 侧绑定与着色器读取的完整链路。

重点回答三个问题：

1. Transform 数据存在哪里、由谁管理。
2. 何时更新，如何区分静态与动态对象。
3. 如何把矩阵与索引传到渲染管线并在 shader 中使用。

---

## 2. 总体架构

当前体系采用了“组件接口 + SOA 存储 + 分组上传”的分层设计：

- 接口层：`TransformComponent`
  - 对外提供 OOP 风格 API（SetLocalPosition/Rotation/Scale 等）。
- 存储层：`TransformDataStorage`
  - 用 SOA 保存 position/rotation/scale/worldMatrix/mobility 等连续数组。
- 分组层：`ECSContext`
  - 分开维护 static 与 movable 的 Transform 列表。
- 更新层：`TransformSystem`
  - Tick 阶段更新矩阵，Render 前提交上传。
- 传输层：`TransformAssignmentBuffer`
  - 维护 LocalToWorld UBO/SSBO + ring 区段，支持脏区间写入。
- 取数层：`PrimitiveBatchPipeline` / `LineRenderPipeline`
  - 为每个渲染项分配 transform_index，并绑定 LocalToWorld 缓冲。

---

## 3. 数据归属与生命周期

## 3.1 TransformComponent 与共享存储

`TransformComponent` 在构造时会从 `TransformDataStorage` 申请一个 `HandleID`。

- 逻辑数据（position/rotation/scale/worldMatrix）放在共享 SOA 中。
- 组件只保留 `storageHandle` 与少量状态（如 `matrixDirty`、`mobility`、parent/child）。
- `SetLocalPosition/Rotation/Scale` 会写入 SOA，并调用 `MarkDirty`。

这种设计把高频批处理数据集中到连续内存，减少了 AOS 布局带来的缓存抖动。

## 3.2 ECSContext 的 static/movable 分离

`ECSContext` 维护两组弱引用列表：

- `static_transforms`
- `movable_transforms`

注册逻辑要点：

- 组件 attach 后由 Context 注册到对应列表。
- mobility 变更会触发迁移（static <-> movable）。
- 首次出现 Transform 组件时，Context 会自动确保 `TransformSystem` 存在并启用。

因此，更新系统不需要每帧遍历全量实体，而是直接遍历已分组的 Transform 集合。

---

## 4. 更新路径（CPU）

## 4.1 脏标记与版本门控

`TransformComponent::MarkDirty` 会：

- 置 `matrixDirty=true`
- 写入 change mask（Position/Rotation/Scale/Parent/WorldMatrix/Mobility 等）
- 递增版本号（在系统侧用于去重判断）

`TransformSystem::ShouldUpdateTransform` 的判定是三段式：

1. 组件必须 dirty。
2. change mask 必须命中 update_mask。
3. 版本号必须与 `last_seen_version` 不同。

这避免了同一版本在同帧或跨阶段重复计算。

## 4.2 Tick 阶段：动态优先

`TransformSystem::Update` 主要处理 movable 列表：

- 默认 `updateMovable=true`。
- 仅遍历 `world->GetMovableTransforms()`。
- 命中条件时调用 `UpdateIfDirty()`，随后 `MarkTransformSeen()`。

静态对象不在每帧 Tick 全量更新，只有在提交阶段检测到需要更新时才递归处理。

## 4.3 静态对象的按需刷新

`SubmitTransformUpdates()` 内先做静态检查：

- 扫描 static 列表，若有命中 `ShouldUpdateTransform` 的对象，则触发 `UpdateStaticDirty()`。
- `UpdateStaticDirty()` 支持父子递归顺序，确保 parent 脏时 child 不会先算。

这使 static 对象具备“几乎不动就不算，动了就全链正确刷新”的行为。

---

## 5. 提交与上传路径（CPU -> GPU）

## 5.1 Handle 顺序与索引表重建

`SubmitTransformUpdates()` 会先保存上一帧 handle 序列，再 `RefreshHandleOrder()`：

- 生成 `static_handles` / `dynamic_handles`
- 生成 `static_index_map` / `dynamic_index_map`

若静态布局变化或静态数量变化，则置 `static_dirty=true`，触发静态全量重写。

## 5.2 Ring 帧段与容量管理

`TransformAssignmentBuffer` 使用 `RingBufferWrapper` 管理动态区，关键点：

- 逻辑槽位 0 是恒等矩阵（identity）。
- 静态对象从槽位 1 开始连续排布。
- 动态对象写入当前帧 ring segment，起始为 `dynamic_base`。

`EnsureCapacity(static_count, dynamic_count)` 会按

- `static_count + 1 + ring(dynamic_count)`

估算总需求并扩容 UBO/SSBO。

## 5.3 脏区间写入策略

提交阶段会构建 dirty index 列表并上传：

- static：
  - `static_dirty=true` 时全量索引；否则按变更筛选。
  - 通过 `WriteStaticDirtyIndices` 合并连续区间，减少 map/unmap 与 flush 次数。
- dynamic：
  - 当前实现 `dynamic_force_full = (dynamic_count > 0)`。
  - 即动态对象每帧会完整写入当前 ring 段，保证 in-flight 各帧数据隔离正确。

最后更新 `last_static_count` / `last_dynamic_count`，并清理 static_dirty 状态。

---

## 6. 渲染侧消费路径

## 6.1 给每个 RenderItem 分配 transform_index

`PrimitiveBatchPipeline::AssignTransformIndices` 依据 Transform 的 handle 与 mobility 计算索引：

- static：`transform_index = group_index + 1`
- dynamic：`transform_index = dynamic_base + group_index`
- 无效 handle：`transform_index = 0`（回退到 identity）

这里的 `+1` 对应槽位 0 保留为恒等矩阵。

## 6.2 TransformID 作为实例属性下发

批处理阶段会把每个 item 的 `transform_index` 写入 TransformID VAB（R16UI）。

含义是：

- 顶点/实例侧仅传一个小 ID。
- shader 通过该 ID 到 LocalToWorld 缓冲取矩阵。

这与项目文档中的“ID 走 Instance Rate，真实数据走 UBO/SSBO”完全一致。

## 6.3 描述符绑定 LocalToWorld

`RenderDescriptorBindingSystem` 在处理材质语义时：

- 发现 `DescriptorSemantic::LocalToWorld`。
- 调用 `batch->transform_buffer->BindTransform(material)`。

`BindTransform` 内部根据编译开关绑定：

- SSBO：`BindSSBO(SBS_LocalToWorld)`
- 或 UBO：`BindUBO(SBS_LocalToWorld)`

因此 draw 阶段材质描述符集已经持有当前帧有效的 LocalToWorld 数据源。

---

## 7. 以时钟示例验证链路

`example/Basic/clock.cpp` 能完整体现该设计：

- 12 个刻度：`Mobility::Static`，初始化后不做每帧逻辑更新。
- 3 根指针：`Mobility::Movable`，每帧根据系统时间旋转并 `MarkDirty()`。
- Tick 中调用 `TransformSystem::Update(delta_time)`，只更新 movable。
- Render 前由系统统一 `SubmitTransformUpdates()` 上传到共享 LocalToWorld 缓冲。

结果是：

- static 的上传成本接近初始化一次。
- dynamic 每帧稳定写 ring 当前段，不与其他 in-flight 帧冲突。

---

## 8. 与 FacingTransform 的协同

`FacingTransformSystem` 在 `TickPostCamera` 阶段改写旋转（LookAt/Billboard 等）。

它本质上仍通过 `TransformComponent::SetLocalRotation` 进入同一条脏标记与提交链路，
不会破坏 TransformSystem 的统一上传路径。

这说明当前架构支持“多个系统写 Transform，单系统集中提交”的模型。

---

## 9. 当前方案的优势与代价

优势：

1. SOA + 分组列表，CPU 侧遍历与缓存命中率更友好。
2. static/movable 分离，避免静态对象无意义每帧更新。
3. 通过版本门控与 change mask，减少重复计算。
4. 动态 ring 段保障多帧并行下的数据一致性。
5. TransformID + SSBO/UBO 方案适合大批量实例。

代价与注意点：

1. 动态对象当前是“每帧全量写当前段”，在超大动态规模下带宽压力较高。
2. 索引依赖 handle 顺序映射，layout 变化会引起批量重写。
3. `TransformID` 若使用 16-bit 通道，需要关注溢出保护与规模上限。

---

## 10. 结论

当前 ECS Transform 通道已经形成闭环：

- 数据存储统一于 `TransformDataStorage`。
- 更新策略由 `TransformSystem` 集中控制（movable 高频、static 按需）。
- 上传由 `TransformAssignmentBuffer` 以“identity + static + dynamic ring”组织。
- 渲染侧通过 TransformID + LocalToWorld 描述符完成矩阵取数。

该体系兼顾了可维护性（组件接口清晰）与性能基础（SOA、分组、脏区间、ring），
是后续做更激进合批与实例化渲染的可靠底座。
