# ShaderGen：真实生成器与诊断工具分层设计

## 1. 目标

把 `ShaderGen` 的工作拆成三条清晰链路：

1. 真实生成器：负责做决策、构建契约、生成最终 GLSL，并把最终 SPIR-V 写入缓存。
2. 诊断工具：只观测、抓快照、比对输出，不参与真实生成决策，也不写入生产缓存。
3. 约束层：统一 block order、module graph、hash key、schema 约束，确保“模板能跑但最终缓存不稳定”的问题被挡在门外。

核心原则：

- 真实链路只保留一个真源；诊断链路不修改生产状态。
- ShaderDocument 是作者态对象，`ShaderBuildContext` 是编译态对象。
- `SPIR-V`/cache key 依赖的是最终稳定文本，不是模板的“临时可运行状态”。

## 2. 真实生成器的层次

### 2.1 解析与决策层

位置：

- `inc/hgl/mtl/MaterialDefinitionRegistry.h`
- `src/ShaderGen/builder/GenericMaterialBuilder.cpp`
- `inc/hgl/mtl/RenderTemplate.h`

职责：

- 解析 `MaterialRecipe`
- 选定 `MaterialDefinition`
- 解析模板请求 / module roots / render template
- 生成 `ShaderResourceSchema`
- 构造 `ShaderLinkSpec` / descriptor contract / program identity

这层做的是“决策”，不能做“文本拼接”或“缓存落盘”本身；它只产生结构化状态，供下一层消费。

### 2.2 GLSL 作者层

位置：

- `src/ShaderGen/template/MeshTemplateComposer.cpp`
- `src/ShaderGen/template/FragmentTemplateComposer.cpp`
- `src/ShaderGen/compile/MaterialShaderEmitter.cpp`
- `src/ShaderGen/compile/MaterialShaderEmitter.h`

职责：

- 由模板 + module graph + material definition 生成 `ShaderDocument`
- 把已解出的 schema / manifest / contract / macros 转成最终 GLSL 文本
- 负责 `#version`、`#extension`、`#define`、interface、module、function、main body 这些 block 的顺序和结构化输出

这里必须维持“纯函数、零决策”原则：

- 不决定缓存 key
- 不决定 shader 程序实例是否能被缓存
- 不做生产性 fallback
- 只把已确认状态转换成文本

### 2.3 SPIR-V 编译与缓存层

位置：

- `src/ShaderGen/compile/MaterialShaderCompiler.cpp`
- `inc/hgl/mtl/MaterialShaderCompiler.h`
- `inc/hgl/mtl/ShaderBuildContext.h`

职责：

- 把最终 `ShaderDocument` 变成 `ShaderBuildContext`
- 建立 `ShaderResourceSchema`
- 组装 `ShaderProgramArtifactMetadata`
- 调用 `FinalizeShaderBuildContext()`
- 走 `artifact_store` 的 `LoadProgramArtifacts / SaveStageSPV / SaveProgramMetadata`

这是最终的“真实生成器”层：它负责最终编译、最终 key、最终缓存状态。

## 3. 诊断工具应该只抓快照，不参与真实决策

位置：

- `inc/hgl/mtl/MaterialShaderCompiler.h` 中的 `MaterialShaderDocumentCapture`
- `src/Tools/ShaderGen/ShaderDocumentProductionRegression.cpp`
- `src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp`

设计约束：

- 诊断工具通过 `MaterialShaderDocumentCapture *document_capture` 传入，但不改写生产编译路径。
- 它只捕获：
  - `mesh_source_document`
  - `fragment_document`
  - `mesh_final_document`
  - `fragment_final_document`
- 真实生产编译路径仍然走 `CompileMaterial(..., config)`；诊断回归门观察的是同一生成链路的快照，而不是另起一条“能跑但不稳定”的旁路。

重点是：

- 诊断工具不能自行决定 module graph
- 诊断工具不能改 block order
- 诊断工具不能绕过 `ShaderLinkSpec`
- 诊断工具不能写缓存、改 `program_cache_key` 或 `stage_key`

它只负责回答：

- source document 的 block 顺序是否规范？
- final document 的 serialized GLSL 是否稳定？
- 编译后的 `ShaderBuildContext` 与 stage key / metadata 是否一致？

## 4. 为什么这层分离能减少 “模板能跑但最终缓存不稳定” 的问题

这个问题的根因通常有两种：

1. 模板层本身可生成合法 GLSL，但最终 order / hash / key 仍然漂移。
2. 诊断或回归逻辑和真实生成链路混在一起，留下兼容分支、旧路径、缓存关键字的隐藏状态。

分层后，责任自然收敛：

- `TemplateComposer` 只负责生成结构化 source document
- `MaterialShaderEmitter` 只负责最终文本发射
- `MaterialShaderCompiler` 负责最终 key 与 SPIR-V
- `ShaderDocumentProductionRegression` 负责验证最终 hash 与 ordering invariants

这样，一旦最终 cache 不稳定，问题一定落在以下某一层：

- 模块图/模板解算不稳定
- block order 不稳定
- serialization 和 final GLSL 不一致
- `ShaderLinkSpec` / `StageKey` / `ArtifactMetadata` 生成逻辑不一致

而不是“模板能跑，缓存却不稳定”的模糊状态。

## 5. 关键不变量

下面这些应当成为硬约束：

- `ShaderDocument` block order 需遵循稳定顺序：
  - `Version -> Extension -> Define -> Resource -> Interface -> Module -> Function -> MainBody -> Raw`
- 一次生成需要唯一 `source_document` 和唯一 `final_document`
- `program hash` 和 `stage hash` 必须来自最终 GLSL 和最终 metadata
- 诊断引擎不能在生产路径里写入 `artifact_store`
- `MaterialDefinitionRegistry` 的生产路径应当是 file-backed + schema-3-only

## 6. 最终落地建议

该分层设计应保持以下姿态：

- “真实生成器”是唯一作者和编译器，负责产生最终 shader 程序
- “诊断工具”仅观测，不做决策，不写缓存
- shared state 只保留真正的生产状态，不塞隐式兼容分支
- 一旦某条链路“能跑但缓存不稳定”，优先在 `ShaderDocument` / `ShaderLinkSpec` / `artifact metadata` 层查，而不是继续堆新的兼容脚本

这可以把问题从“模板级别”上提到“生成器接口级别”，让每一层都有清晰责任边界，并大幅减少最终缓存漂移的问题。
