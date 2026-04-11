# MaterialInstance Handle-First 迁移详细计划

## 1. 目标与范围

### 1.1 目标
将当前以 `MaterialInstance*` 为中心的绑定模型，迁移为“稳定逻辑索引 + 可迁移物理槽位”的 Handle-First 模型，满足以下约束：

1. 业务层身份稳定：同一对象全生命周期持有同一个 `MaterialInstanceHandle`。
2. 运行时可重绑定：允许在不变更 handle 的前提下切换 `MaterialTemplate` 与 `MaterialResourceDomain`。
3. 渲染自动跟随：下一帧通过 handle 解析出新的 `PrimitiveMaterialSlot`。
4. 与现有系统兼容：迁移期间保留旧路径，分阶段切换，避免大面积中断。

### 1.2 适用场景

1. RTS 单位由动态骨骼渲染切换到静态残骸渲染。
2. Clock 等示例中，刻度/指针从纯色切换到纹理或简单光照。
3. ECS 系统运行中，根据状态机切换材质模板与资源域。

### 1.3 非目标

1. 本阶段不改 ShaderGen 架构。
2. 本阶段不一次性删除所有旧 MI API。
3. 本阶段不引入复杂字段级布局迁移（`LayoutAware` 可先降级行为）。

## 2. 现状问题（针对实现而非概念）

1. `MaterialAssetRegistry` 仍缓存 `MaterialInstance*`（entity/legacy 两条缓存链）。
2. `mi_id` 是 domain 内物理槽位，不具备全局稳定身份语义。
3. 示例与 ECS 中大量 `CreateMaterialInstance()->ToSlot()` 过渡调用。
4. `clock.cpp` 里 MI 仅用于初始化写入后转 slot，表明旧抽象已冗余。

## 3. 目标数据模型

## 3.1 核心类型

1. `using MaterialInstanceHandle = uint64_t;`
2. `constexpr MaterialInstanceHandle InvalidMaterialInstanceHandle = 0;`

## 3.2 绑定记录（Registry 持有）

`MaterialBindingRecord` 建议字段：

1. `MaterialInstanceHandle handle`
2. 当前物理绑定：`domain_id`, `domain_generation`, `mi_id`
3. 当前渲染快照：`material_template`, `vil`, `preset`, `material_preset`
4. 可迁移参数数据：`instance_payload`, `mit_packed`
5. `binding_version`（每次 rebind 自增）
6. 状态位：`alive`, `pending_rebind`, `last_error`

## 3.3 语义分层

1. Handle：业务层稳定身份。
2. Record：Registry 内部真相源（source of truth）。
3. Slot：提交渲染的瞬时快照，不是稳定身份。

## 4. API 设计（签名级）

## 4.1 新增 API（MaterialAssetRegistry）

1. `MaterialInstanceHandle AllocateHandle(const MaterialBindingInit &init);`
2. `bool BuildSlot(MaterialInstanceHandle handle, PrimitiveMaterialSlot &out_slot) const;`
3. `bool RebindHandle(MaterialInstanceHandle handle, const MaterialBindingRebind &req);`
4. `bool WriteMIData(MaterialInstanceHandle handle, const void *data, uint32_t size);`
5. `bool SetTextureArrayLayer(MaterialInstanceHandle handle, mtl::SamplerSlot slot, uint32_t layer);`
6. `bool ReleaseHandle(MaterialInstanceHandle handle);`

## 4.2 观测 API

1. `bool QueryBindingVersion(MaterialInstanceHandle handle, uint32_t &out_version) const;`
2. `bool QueryHandleAlive(MaterialInstanceHandle handle) const;`
3. `uint64_t GetCrossDomainRebindCount() const;`
4. `uint64_t GetHandleRebindFailCount() const;`

## 4.3 Rebind 拷贝策略

1. `None`：不拷贝旧 payload。
2. `CompatiblePrefix`：按 `min(old_bytes, new_bytes)` 拷贝。
3. `LayoutAware`：字段映射（可先回退到 `CompatiblePrefix`）。

## 5. 文件落位与职责

## 5.1 Registry（主入口）

1. `inc/hgl/graph/module/MaterialAssetRegistry.h`
2. `src/SceneGraph/module/MaterialAssetRegistry.cpp`

职责：

1. Handle 生命周期管理。
2. BindingRecord 真相源维护。
3. Rebind 原子语义保障。
4. 渲染快照构建（BuildSlot）。

## 5.2 Manager（物理槽原语）

1. `inc/hgl/graph/module/MaterialManager.h`
2. `src/SceneGraph/module/MaterialManager.cpp`

职责：

1. 继续提供 `AllocMaterialInstanceSlot(...)`。
2. 可选新增 slot 释放辅助 API（或 Registry 直接调 domain 释放）。
3. 不承载业务稳定身份。

## 5.3 Slot 数据结构

1. `inc/hgl/graph/PrimitiveMaterialSlot.h`

职责：

1. 作为渲染快照 POD。
2. 不作为业务长期 ID。

## 6. 迁移阶段与执行明细

## Stage 0 - 观测与防护（低风险）

目标：先建立统计与日志，避免黑盒迁移。

任务：

1. 增加计数器：`handle_alloc`, `handle_release`, `handle_rebind`, `cross_domain_rebind`, `rebind_fail`。
2. 统一日志前缀：`[MaterialHandle]`。
3. 为 Rebind 失败路径增加错误码/文本。

验收：

1. 不改行为，仅新增观测。
2. `ULRE.SceneGraph` 构建通过。

## Stage 1 - 声明与骨架（中风险）

目标：引入类型与 API 声明，不改变默认调用路径。

任务：

1. 新增 handle 类型、请求结构、枚举、API 声明。
2. `MaterialAssetRegistry.cpp` 增加空实现骨架（返回失败或占位逻辑）。
3. 不迁移调用点。

验收：

1. 全量编译通过。
2. ABI 变化受控。

## Stage 2 - Handle 分配与 BuildSlot（中风险）

目标：打通最小可用链路 `AllocateHandle -> BuildSlot`。

任务：

1. `AllocateHandle` 通过 `AllocMaterialInstanceSlot` 生成初始物理槽。
2. 填充 `MaterialBindingRecord`。
3. `BuildSlot` 从 record 还原 `PrimitiveMaterialSlot`。

验收：

1. 可在单个测试路径上创建 primitive（不依赖 MI*）。
2. slot 内容与旧路径一致性通过（material/domain/mi_id/vil/preset）。

## Stage 3 - Handle 写入 API（中风险）

目标：替代 `mi->WriteMIData` / `mi->SetTextureArrayLayer`。

任务：

1. 实现 `WriteMIData(handle,...)`。
2. 实现 `SetTextureArrayLayer(handle,...)`。
3. 保证越界与空域安全检查。

验收：

1. 参数写入后渲染结果与旧路径一致。
2. 失败路径有明确日志。

## Stage 4 - RebindHandle（关键阶段）

目标：支持“换模板/换域但索引不变”。

任务：

1. 新槽先分配并初始化。
2. 按 copy_policy 迁移 payload/MIT 数据。
3. 成功后释放旧槽并切换 record。
4. `binding_version++`。

关键语义：

1. 失败不破坏旧绑定（强异常安全）。
2. 跨域重绑计数与日志准确。

验收：

1. handle 恒定，domain/template 可切换。
2. 无槽位泄漏（旧槽被正确回收）。

## Stage 5 - 调用点试点迁移（clock 先行）

目标：在低风险示例中验证完整模型。

目标文件：

1. `example/Basic/clock.cpp`

任务：

1. 移除 `mi_tick` 与 `hands[i].mi` 持有。
2. 改为 `AllocateHandle` + `WriteMIData(handle)` + `BuildSlot(handle)`。
3. primitive 创建不再依赖 `ToSlot()`。

验收：

1. 时钟运行效果一致。
2. 句柄稳定，重绑可演示。

## Stage 6 - ECS 系统迁移（Quad/Text）

目标：从示例迁移到真实系统路径。

目标文件：

1. `src/ecs/systems/render/QuadMaterialBindingSystem.cpp`
2. `src/ecs/support/TextRenderPipeline.cpp`
3. `src/SceneGraph/font/TextRender.cpp`

任务：

1. 把短生命周期 MI 创建路径替换为 handle 路径。
2. 每帧更新参数改为 handle 写入。

验收：

1. ECS 运行稳定。
2. 无 MI 新增调用回流。

## Stage 7 - 清理旧路径（最终阶段）

目标：下线 MI 兼容层。

任务：

1. 标记/移除 `AcquireMI`, `CreateMI`, `AcquireMaterialInstance` 外部调用。
2. 评估并删除 `VKMaterialInstance.h/.cpp`。
3. 更新 CMake 源文件列表。

验收：

1. 无旧 API 调用。
2. 全目标编译通过。

## 7. Clock 专项增强与“换域换模板”演示计划

## 7.1 新演示定义

建议演示名：`ClockMaterialRebindDemo`。

目标：验证刻度/指针在不变 handle 下，运行时切换材质模板与资源域。

## 7.2 演示流程

1. Mode A（初始）：
   - Tick: `PureColor2D` 白色
   - Hands: `PureColor2D` RGB
2. 按键切换到 Mode B：
   - Tick: 切 `PureTexture2D`（纹理）
   - Hands: 切“简单光照”模板
   - 同时切换到另一 `ResourceDomain`
3. 再切回 Mode A。

约束：

1. Tick/Hand 的 handle 全程不变。
2. Entity/Primitive 引用不重建（优先走 `BindMaterialSlot` 重绑）。

## 7.3 演示验收

1. 视觉上可见模板与域切换效果。
2. `cross_domain_rebind_count` 与切换次数一致。
3. 无旧槽残留、无内存泄漏、无崩溃。

## 8. 风险与回滚

## 8.1 主要风险

1. Rebind 失败导致渲染状态错乱。
2. 数据迁移策略不兼容导致参数丢失。
3. 调用点混用新旧 API 导致双写冲突。

## 8.2 防护

1. Rebind 采用“两阶段提交”：先新后旧。
2. 引入版本号与 alive 状态检查。
3. 迁移期间保留旧路径 feature flag。

## 8.3 回滚策略

1. 保留旧 `ResolveMI/CreateMI` 路径到 Stage 6 完成后。
2. 开关级回滚：禁用 handle 路径，恢复 MI 路径。
3. 出问题先回滚调用点，不立即回滚底层结构。

## 9. 里程碑与完成定义

M1: Stage 1-2 完成
- DoD：handle 可分配，BuildSlot 可创建 primitive。

M2: Stage 3-4 完成
- DoD：handle 可写入、可重绑，跨域切换稳定。

M3: Stage 5 完成
- DoD：clock 完成无 MI 持有迁移。

M4: Stage 6-7 完成
- DoD：ECS 核心路径迁移完成，旧 API 下线。

## 10. 旧路径代码清理收口清单

## 10.1 废弃 API 下线清单
1. 下线 MaterialAssetRegistry 旧接口：
   - AcquireMI
   - CreateMI
     定义位置见 MaterialAssetRegistry.h。

1. 下线 MaterialManager 旧接口：
   - AcquireMaterialInstance
   - CreateMaterialInstance
   - RebindMaterialInstance
     定义位置见 MaterialManager.h。

## 10.2 兼容类与文件清理
1. 评估并删除 MI 兼容类文件：
    - VKMaterialInstance.h
    - VKMaterialInstance.cpp
2. 删除后同步清理包含与构建条目（头文件 include、CMake 源列表）。

## 10.3 src 调用点迁移（必须清零）
1. QuadMaterialBindingSystem.cpp：CreateMaterialInstance 2 处。
2. TextRenderPipeline.cpp：CreateMaterialInstance 1 处。
3. TextRender.cpp：CreateMaterialInstance 1 处。
4. 验收标准：以上 4 处全部改为 handle/slot-first 路径，不再创建短生命周期 MI 对象。

## 10.4 example 调用点迁移（18 处，14 文件）
1. WallsFromPolyline.cpp
2. SceneTest.cpp
3. GeometryTest.cpp
4. ExtrudedPolygonTest.cpp
5. RenderToTexture.cpp
6. RenderBoundBox.cpp
7. RecursiveCube.cpp
7. PBRSpheresECS.cpp
8. PBRColor3DSpheresECS.cpp
9. clock.cpp
10. DomainIsolationDemo.cpp
11. BillboardTest.cpp
12. BillboardIconECSBase.cpp
13. BillboardECS.cpp

## 验收标准：

1. example 下 registry->CreateMI 调用清零。
2. src 下 CreateMaterialInstance 外部调用清零。
3. 仅保留 handle-first API 作为公开路径。

## 10.5 关门标准（必须同时满足）
1. grep 无旧 API 调用残留（AcquireMI/CreateMI/AcquireMaterialInstance/CreateMaterialInstance/RebindMaterialInstance）。
2. 删除 VKMaterialInstance 后全工程构建通过。
3. clock 与 ECS 关键路径运行通过，跨域重绑稳定，旧槽位回收正常。