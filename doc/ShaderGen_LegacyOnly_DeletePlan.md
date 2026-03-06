# ShaderGen LegacyOnly 删除计划

## 目标

将 SceneGraph 应用层的 ShaderGen 适配器链路移除，仅保留 `ShaderGenPathMode::LegacyOnly` 的材质构建工作路径。

约束：
- 先断调用，再删文件，最后收口 API。
- 不删除 `hgl/shadergen/contract` 底层库本体（仅断开 SceneGraph 应用层接入）。

## 现状关系（调用链）

核心入口在 `src/SceneGraph/module/MaterialManager.cpp`。

主链路：
1. `CreateMaterial` -> `BuildShaderGenContractPathContext`。
2. `CreateMaterial` -> `ApplyShaderCompilerProfile`。
3. `CreateMaterialWithContract` -> `RunReadOnlyValidationGate`。
4. `ExecuteMaterialBuildPipeline` -> `BuildShaderModulesFlow` + `BuildMaterialBindingsFlow`。
5. `ApplyMaterialFinalizePlan` -> `BuildMaterialFinalizePlan`。

适配器层关系：
- `MaterialBuildFlowAdapter` 依赖：
- `ShaderGenSPVModuleAdapter`（SPV镜像/legacy选择）
- `ShaderGenVertexPolicyAdapter` + `ShaderGenVertexInputAdapter`
- `ShaderGenDescriptorPolicyAdapter` + `ShaderGenDescriptorLayoutAdapter`
- `ShaderGenContractGateReporter`
- `ShaderGenValidationStorageService`

校验/观测链关系：
- `ShaderGenReadOnlyValidationGate` -> `RendererShaderGenAdapter`
- `RendererShaderGenAdapter` -> `ShaderGenMirrorDiffPresenter` + `ShaderGenValidationReportUtils` + `ShaderGenValidationStorageService`
- `GraphicsContext` 可通过 `ShaderGenValidationQueryBridge` 做 fallback 查询
- `ECSContext` 在 `src/ecs/core/ContextDebug.cpp` 直接调用 `RendererShaderGenAdapter` 读取统计

## 分阶段删除计划

### Phase 0：先固定行为为 LegacyOnly（低风险）

1. 修改 `inc/hgl/graph/module/ShaderGenPathMode.h`：
- 默认解析/回退均指向 `LegacyOnly`。
- 暂时保留 `MirrorValidate` / `MirrorPreferred` 枚举用于兼容。

2. 修改 `inc/hgl/graph/core/GraphicsContext.h`：
- 默认 `shadergen_path_mode` 改为 `LegacyOnly`。
- 构造函数默认参数改为 `LegacyOnly`。

3. 修改 `src/Work/AppFramework.cpp` 与 `inc/hgl/framework/WorkManager.h`：
- 未传 `--shadergen-path-mode` 时默认 legacy-only。
- 可选：对 `mirror-*` 参数打弃用提示并降级处理。

### Phase 1：MaterialManager 去适配器化（最高优先级）

1. 重构 `src/SceneGraph/module/MaterialManager.cpp`：
- 移除：`ShaderGenContractPathContext`、`ApplyShaderCompilerProfile`、`RunReadOnlyValidationGate`、`CreateMaterialWithContract` 的 mirror 依赖。
- `CreateMaterial` 直接走 precheck + legacy build。

2. 收口 `ExecuteMaterialBuildPipeline` 签名：
- 移除 `mirror_result`、`require_mirror_valid` 参数。
- 内部仅保留 legacy module/vertex/descriptor 构建。

3. 同步 `inc/hgl/graph/module/MaterialManager.h`：
- 删除或收口 mirror 参数接口。

### Phase 2：删除构建策略适配器

在 Phase 1 编译通过后删除：
- `ShaderGenContractPathContext.h/.cpp`
- `ShaderGenCompilerProfileAdapter.h/.cpp`
- `ShaderGenReadOnlyValidationGate.h/.cpp`
- `MaterialBuildFlowAdapter.h/.cpp`
- `ShaderGenSPVModuleAdapter.h/.cpp`
- `ShaderGenVertexPolicyAdapter.h/.cpp`
- `ShaderGenVertexInputAdapter.h/.cpp`
- `ShaderGenDescriptorPolicyAdapter.h/.cpp`
- `ShaderGenDescriptorLayoutAdapter.h/.cpp`

说明：
- `MaterialCreatePrecheckAdapter.*` 与 `MaterialFinalizeFlowAdapter.*` 是通用 legacy 流程，可保留。

### Phase 3：删除校验/报告/统计链

若确认不再需要 ShaderGen 镜像观测：
- 删除 `RendererShaderGenAdapter.h/.cpp`
- 删除 `ShaderGenMirrorDiffPresenter.h/.cpp`
- 删除 `ShaderGenValidationReportUtils.h/.cpp`
- 删除 `ShaderGenValidationStorageService.h/.cpp`
- 删除 `ShaderGenValidationQueryBridge.h/.cpp`
- 删除 `ShaderGenContractGateReporter.h/.cpp`

并同步修改：
- `src/SceneGraph/render/GraphicsContext.cpp`：移除 query bridge fallback，相关查询接口改空结果或 no-op。
- `src/ecs/core/ContextDebug.cpp`：移除对 `RendererShaderGenAdapter` 的直连查询，返回空矩阵/空直方图或 `false`。

### Phase 4：构建系统与头文件收尾

1. 更新 `src/SceneGraph/CMakeLists.txt`：
- 移除已删除头源文件条目。

2. 全局清理 include/前向声明：
- `MaterialManager.*`
- `GraphicsContext.*`
- `ContextDebug.cpp`
- 其他引用上述符号的文件

3. 可选最终收口：
- 在确认外部脚本已迁移后，再移除 `MirrorValidate` / `MirrorPreferred` 枚举与 CLI 文法支持。

## 风险点与顺序建议

高风险点：
1. `MaterialManager` 是主入口，必须先改调用再删适配器文件。
2. `GraphicsContext`/`ECSContext` 仍有统计接口依赖，删校验链前要先退化这两个入口。
3. `CMakeLists.txt` 同步不及时会导致链接/编译失败。

推荐顺序：
1. Phase 0（行为收敛）
2. Phase 1（断调用）
3. Phase 2（删构建适配器）
4. Phase 3（删统计链）
5. Phase 4（构建与API收尾）

## 每阶段验证清单

### 阶段验证 A（Phase 0 后）
1. 搜索默认值：未显式传参时路径为 legacy-only。
2. 启动冒烟：不传参数可正常创建材质。

### 阶段验证 B（Phase 1 后）
1. 搜索调用应为 0：
- `BuildShaderGenContractPathContext`
- `RunReadOnlyValidationGate`
- `CreateMaterialWithContract`（若已删除）
2. 材质创建冒烟：2D/3D preset 均可创建。

### 阶段验证 C（Phase 2 后）
1. 已删文件无残余引用。
2. CMake 配置+编译通过。

### 阶段验证 D（Phase 3/4 后）
1. 搜索应为 0：
- `RendererShaderGenAdapter::`
- `ShaderGenValidationQueryBridge`
- `RecordShaderGenContractPathDecision`
2. `GraphicsContext` 与 `ECSContext` 的相关 debug API 行为可预期（空结果/no-op）。
3. 全量构建与主要示例运行通过。

## 可执行第一批（建议一个 commit）

仅做收敛不删文件：
1. 改 `ShaderGenPathMode.h` 默认。
2. 改 `GraphicsContext.h` 默认。
3. 改 `AppFramework.cpp` 与 `WorkManager.h` 默认入口。

commit message 建议：
`refactor(shadergen): default path mode to legacy-only`
