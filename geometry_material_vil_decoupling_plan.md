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

- 总进度：约 100%（Phase 0、Phase 1、Phase 2、Phase 3、Phase 5 已完成，Phase 4 的主要调用点迁移已完成）
- 当前阶段：Phase 5（已完成），Phase 4（基本完成，待专项路径处理）

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

状态：已完成（100%）

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
8. 已完成：补充 direct/deferred 绑定与 runtime VIL 生命周期回归（释放旧 runtime VIL、active/owned 指针切换、direct rebind 与 deferred rebind 转移语义），并通过 `test_vertex_binding_compat_helper` 实跑验证。
9. 已完成：修复 `test_vertex_binding_compat_helper` 的输入构造（`VertexInputAttribute.storage_format` 显式初始化），消除默认 VIL 构建阶段的未初始化格式断言；当前该测试集可完整通过。
10. 已完成：构建并运行 Phase 2 关键回归包（`ULRE.SceneGraph`、`test_vertex_type_format_compat`、`test_vertex_binding_compat_helper`）全部通过。

## Phase 3：运行时 Resolve 对齐

状态：已完成（100%）

1. 已完成首项：`MaterialAssetRegistry::ResolveMI` 的 legacy/entity cache-hit 路径已收口到统一运行时 slot 更新流程（统一 domain 变更重分配语义）。
2. 已完成首项：cache-hit 刷新阶段新增 `resolved_vil` 失败保护（失败时释放旧 slot 并返回无效），避免保留陈旧绑定状态。
3. 已完成首项：entity cache-miss 分配已改为使用 resolve 后 domain（`handle.domain`），与运行时域决策保持一致。
4. 已完成本轮：legacy/entity 的 miss 分支已与 hit 分支统一走同一 runtime slot 应用逻辑（统一 `resolved_vil` 判空、domain 重分配、实例数据写回与失败返回语义），resolve 分支行为进一步收口。
5. 已完成本轮：对 deferred 签名新增兜底稳定化——当 `vil_hash == 0 && geometry_layout_hash == 0` 且存在 geometry 指针时，运行时按 geometry 计算并补全 `geometry_layout_hash`，提升 key 稳定性与可复现性。
6. 已完成：在保留 fallback 策略前提下提升 schema 推导优先级；miss/hit 路径统一改为先走 `ResolveRuntimeVIL`（geometry/schema 推导）再做分配，减少 default VIL 直落路径差异。
7. 已完成验证：构建并运行回归（`ULRE.SceneGraph`、`test_vertex_type_format_compat`、`test_vertex_binding_compat_helper`）全部通过。

## Phase 4：调用点分批迁移

状态：基本完成（85%）

1. 已完成：`draw_triangle` 已迁移到无 `VIL` 的 `CreateGeometry` 入口。
2. 已完成：示例中 9 处 `VertexDataManager` 创建已从 `MakeGeometryVertexFormatMap(material_vil/standard_vil/solid.vil)` 改为显式 `VertexFormatMap` 常量。
3. 已完成：清理其它仍通过 `MakeGeometryVertexFormatMap(vil)` 进行 bridge 的示例与运行时路径，并删除旧 helper 入口。
4. 特殊路径暂挂起：`LoadScene/LoadGeometry` 属于文件驱动加载链路，仍保留基于文件元数据与 VIL 的混合读取逻辑，需要单独重构，不纳入当前通用迁移收尾。
5. 未完成：每批调用点迁移后的整项目编译与运行时冒烟验证。

## Phase 5：遗留路径清理

状态：已完成（100%）

1. 已完成：高频 VIL-first 入口已标记 deprecated（`GraphicsGeometryFactory::CreateGeometry(..., const VIL *, ...)`、`GraphicsGeometryFactory::CreateCreater(const VIL *)`、`GraphicsGeometryFactory::CreatePrimitive(..., SemanticMaterialId, builder)`），并附迁移指引到 schema-first 入口。
2. 已完成：在迁移窗口内完成 deprecated 路径调用清理；`GraphicsGeometryFactory` 的 schema-first `CreateGeometry(...)` 内部实现已改为直接基于 `VertexFormatMap` 构建，不再回调 VIL-first 重载；语义 builder 模板入口内部已改为直接构建 `GeometryCreater(device, format_map, buffer_manager)`，去除对 deprecated `CreateCreater(const VIL *)` 的依赖；示例调用点已全部迁移到显式“创建 geometry + CreatePrimitive(geometry, semantic_id)”路径（`SimpleCube.cpp`、`SimpleCylinder.cpp`、`auto_instance.cpp`、`SimpleCube_AutoTransparency.cpp`、`SimpleTube.cpp`、`BasicLitMeshesECS.cpp`、`PBRColor3DSpheresECS.cpp`、`PBRSpheresECS.cpp`、`TextureBlinnPhongMeshesECS.cpp`）。
3. 已完成验证：清理与迁移同步回归全部通过；示例目标构建验证覆盖 `07a_SimpleCube_AutoTransparency`、`07c_SimpleTube`、`08_PBRSpheresECS`、`09_BasicLitMeshesECS`、`10_TextureBlinnPhongMeshesECS`、`14_PBRColor3DSpheresECS`，兼容性测试 `test_vertex_type_format_compat` 与 `test_vertex_binding_compat_helper` 持续通过。
3. 清理注释与文档，防止新代码回流旧路径。

## 验证计划

1. 每个阶段完成后构建 SceneGraph/Vulkan/示例目标。
2. 扩展兼容性测试，覆盖混合存储格式（例如 vec2 对 RG16F/RG32F）。
3. 每个迁移批次执行代表性样例冒烟测试。
4. 注入故意不兼容输入，验证诊断信息可定位。

## 范围

- 包含：Geometry 创建 API、Primitive 绑定、运行时 VIL resolve、示例迁移。
- 不包含：新增渲染特性、超出顶点输入解耦范围的大规模材质系统重设计。
