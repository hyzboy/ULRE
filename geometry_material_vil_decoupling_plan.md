# Geometry/Material/VIL 解耦改造计划

## 目标

将几何数据生产与材质消费需求解耦：

- Geometry 只描述实际顶点数据。
- MaterialTemplate 只描述 Shader 侧消费需求。
- Geometry 与 Material 的匹配在 Primitive 绑定阶段完成，使用兼容规则，不在 Geometry 创建阶段硬绑定。

## 当前耦合基线

- Geometry 创建目前仍要求传入 VIL。
- 典型路径为 `material->GetDefaultVIL() -> CreateGeometry(vil, ...) -> CreatePrimitive(...)`。
- Primitive 绑定阶段目前仍存在严格的 format/stride 强一致校验。

## 总体完成度

- 总进度：约 96%（Phase 0、Phase 1 已完成，Phase 2 已完成共享入口、resolver 语义与 direct/deferred 生命周期回归，Phase 4 的主要调用点迁移已完成）
- 当前阶段：Phase 2（进行中，文档与全量回归收尾），Phase 3（未开始），Phase 4（基本完成，待专项路径处理）

## 分阶段计划与完成度

## Phase 0：基线与护栏

状态：已完成（100%）

1. 冻结当前行为与关键失败日志，作为对照基线。
2. 以 `example/Basic/draw_triangle.cpp` 作为首个迁移样例基线。

## Phase 1：非破坏式 API 增量

状态：已完成（100%）

1. 已完成：新增 schema-first 的 `CreateGeometry` 重载（不再显式传 `VIL`）。
2. 已完成：在 `GeometryCreater` 中新增 schema 驱动创建路径。
3. 已完成：在 `VertexDataManager` 中新增 schema 驱动构造路径。
4. 已完成：保留现有 VIL-first API，保障迁移期兼容。

## Phase 2：绑定侧兼容校验改造

状态：进行中（96%）

1. 已完成首轮：将 `Primitive` 绑定中的 `format ==` 与 `stride ==` 强校验替换为基于 shader/storage 的兼容校验，且 direct/deferred 两条绑定路径都已接入首轮兼容逻辑。
2. 使用兼容规则：
   - 语义存在；
   - shader/storage 类型兼容；
   - 可构建绑定布局。
3. 已完成：绑定路径复用 `IsStorageFormatCompatibleWithShaderType`，并在 `Primitive` 内对“兼容但格式不同”的情况生成 geometry-driven 的 runtime `VIL`。
4. 已完成首轮：`Primitive` 已持有并释放运行时生成的 `VIL`，direct/deferred 两条绑定路径统一走 effective `VIL`。
5. 已完成：抽取共享顶点绑定兼容 helper，`Primitive` 与 `MaterialAssetRegistry` 已复用同一套 shader/storage 兼容判断，减少规则漂移。
6. 已完成首轮：`Primitive` effective VIL 与 registry 的 `ResolveVILFromGeometry` 已复用同一 geometry-driven `VILConfig` 构建入口，减少双路径实现差异。
7. 已完成：已新增共享兼容 helper 与共享入口前置语义（reason/失败分支）测试，并补充 `VertexInput` 级共享入口与 resolver 场景验证（成功/布局不匹配/lookup 失败/不兼容存储/必需属性缺失），覆盖 reason 优先级与输出状态 has_any/cfg/has_layout_mismatch 契约。
8. 已完成首轮：补充 direct/deferred 绑定与 runtime VIL 生命周期回归（释放旧 runtime VIL、active/owned 指针切换、direct rebind 与 deferred rebind 转移语义），并通过 `test_vertex_binding_compat_helper` 实跑验证。
9. 已完成：修复 `test_vertex_binding_compat_helper` 的输入构造（`VertexInputAttribute.storage_format` 显式初始化），消除默认 VIL 构建阶段的未初始化格式断言；当前该测试集可完整通过。

## Phase 3：运行时 Resolve 对齐

状态：未开始（0%）

1. 对齐 geometry-first 与 material-first 两条运行时 resolve 路径行为。
2. 确保 deferred 路径（`vil_hash == 0`）行为稳定、可复现。
3. 保留必要回退策略，但优先 schema 推导。

## Phase 4：调用点分批迁移

状态：基本完成（85%）

1. 已完成：`draw_triangle` 已迁移到无 `VIL` 的 `CreateGeometry` 入口。
2. 已完成：示例中 9 处 `VertexDataManager` 创建已从 `MakeGeometryVertexFormatMap(material_vil/standard_vil/solid.vil)` 改为显式 `VertexFormatMap` 常量。
3. 已完成：清理其它仍通过 `MakeGeometryVertexFormatMap(vil)` 进行 bridge 的示例与运行时路径，并删除旧 helper 入口。
4. 特殊路径暂挂起：`LoadScene/LoadGeometry` 属于文件驱动加载链路，仍保留基于文件元数据与 VIL 的混合读取逻辑，需要单独重构，不纳入当前通用迁移收尾。
5. 未完成：每批调用点迁移后的整项目编译与运行时冒烟验证。

## Phase 5：遗留路径清理

状态：未开始（0%）

1. 将 VIL-first 入口标记为 deprecated。
2. 在迁移窗口结束后移除 deprecated 路径。
3. 清理注释与文档，防止新代码回流旧路径。

## 验证计划

1. 每个阶段完成后构建 SceneGraph/Vulkan/示例目标。
2. 扩展兼容性测试，覆盖混合存储格式（例如 vec2 对 RG16F/RG32F）。
3. 每个迁移批次执行代表性样例冒烟测试。
4. 注入故意不兼容输入，验证诊断信息可定位。

## 范围

- 包含：Geometry 创建 API、Primitive 绑定、运行时 VIL resolve、示例迁移。
- 不包含：新增渲染特性、超出顶点输入解耦范围的大规模材质系统重设计。
