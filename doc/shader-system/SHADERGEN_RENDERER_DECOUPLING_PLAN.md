# ShaderGen 与渲染器彻底分离重构计划（可行性 + 执行方案）

**版本**：v1.0  
**日期**：2026-03-02  
**状态**：建议执行（分阶段）

---

## 1. 结论（先答复）

结论：**可行**，而且从当前代码结构看，已经具备拆分基础。  
你提出的目标模型可定义为：

- 渲染器只提交 `需求结构（Request）`
- ShaderGen 输出：
  - descriptor 信息（set/binding + type + stage）
  - vertex input 信息
  - UBO/SSBO 结构信息
  - 按 stage 的 SPV

这在工程上可落地，但需要先把当前 ShaderGen 中对渲染器/Vulkan实现细节的直连依赖改为“契约对象 + 适配器”。

---

## 2. 现状耦合点（为什么现在还没彻底分离）

当前主要耦合来自：

1. ShaderGen 侧直接引用渲染层类型
   - `MaterialCreateInfo` 里直接出现 `VulkanDevAttr`、`UBODescriptor/SSBODescriptor`
   - `ShaderCreateInfo` / `ShaderDescriptorInfo` / `MaterialDescriptorInfo` 依赖 `VK*` 描述符与 stage 枚举

2. 渲染器直接消费 ShaderGen 内部对象
   - `MaterialManager` 直接拿 `MaterialCreateInfo`、`ShaderCreateInfoMap`、`MaterialDescriptorInfo`、`VIAArray`
   - 这意味着 ShaderGen 产物是“类对象接口”，不是“稳定 DTO/契约”

3. 编译和生成流程仍与当前图形后端语义混在一起
   - 例如 descriptor 结构布局与 Vulkan 类型在同一层

---

## 3. 目标架构（分离后）

采用 **Contract + Engine + Adapter** 三层：

- `shader-contract`（纯契约层）
  - 只放 POD/DTO、枚举、错误码、版本号
  - 不依赖渲染器对象，不依赖具体设备类

- `shadergen-core`（纯生成层）
  - 输入 `ShaderGenRequest`
  - 输出 `ShaderGenResult`
  - 负责逻辑校验、布局生成、源码组装、SPV 编译

- `renderer-adapter`（渲染器适配层）
  - 把 `ShaderGenResult` 映射到 `MaterialDescriptorManager / PipelineLayout / VertexInput`
  - 这是唯一了解渲染器内部资源管理的层

---

## 4. 建议契约（最小可用）

> 下面是建议形状，不要求一次到位；先建立字段与责任边界。

### 4.1 输入（Renderer -> ShaderGen）

```cpp
struct ShaderGenRequest {
    uint32_t contract_version;

    // 材质与变体
    InlineMaterial material_id;
    MaterialCreateConfigLite material_cfg;
    ShaderPermutationKey permutation;
    PipelineMode pipeline_mode;

    // 资源需求（渲染器声明“我需要什么”）
    Span<const ResourceRequirement> required_resources;

    // 顶点输入需求
    Span<const VertexInputRequirement> vertex_requirements;

    // 平台/质量档
    PlatformTier platform_tier;
    QualityLevel quality_level;

    // 编译选项
    bool enable_debug_info;
    bool enable_fallback;
};
```

### 4.2 输出（ShaderGen -> Renderer）

```cpp
struct ShaderGenResult {
    uint32_t contract_version;

    // 1) SPV
    Span<const StageSpvBlob> spv_per_stage;

    // 2) 资源布局
    ShaderResourceLayout layout;         // descriptor set/binding/type/stage

    // 3) 顶点布局
    VertexInputLayout vertex_layout;     // location/type/rate/group

    // 4) 结构体信息（UBO/SSBO）
    Span<const BufferStructDesc> buffer_structs;

    // 5) 诊断
    ShaderDiagnostics diagnostics;

    // 6) 缓存键
    ShaderCacheKey cache_key;
};
```

### 4.3 关键原则

- Contract 层禁止出现 `VulkanDevAttr`、`VulkanDevice`、`MaterialManager` 等对象
- `VIAArray/SVArray` 作为内部专用结构可保留在 core，但**对外只暴露 DTO**
- 字符串对外统一 `std::string` / `string_view`

---

## 5. 分阶段重构计划

## Phase 0：冻结边界与契约草案（1~2 天）

- 产出 `shader-contract` 初版结构定义（仅头文件）
- 标记“禁止新增耦合点”规则
- 确认与现有规范文档一致：
  - `SHADER_LOGIC_CONSTRAINTS_SPEC.md`
  - `RESOURCE_LAYOUT_BINDING_STRATEGY.md`

**验收**：契约头文件可独立编译，无渲染器 include。

## Phase 1：输出镜像（不改行为）（2~4 天）

- 在 ShaderGen 内部生成现有对象的同时，额外组装 `ShaderGenResult`（镜像）
- 渲染器暂时仍走旧接口

**验收**：旧路径 100% 不变，新增镜像与旧对象字段一致性校验通过。

## Phase 2：渲染器适配器接入（3~5 天）

- 新增 `RendererShaderGenAdapter`
- `MaterialManager` 增加新入口：从 `ShaderGenResult` 构建材质
- 保留旧入口并行（双轨）

**验收**：同一材质双轨输出一致（layout/SPV/hash）。

## Phase 3：切主路径（2~3 天）

- 默认切换到 `ShaderGenResult` 路径
- 旧 `MaterialCreateInfo` 路径保留为 fallback（编译开关控制）

**验收**：默认路径稳定运行，旧路径可回退。

## Phase 4：移除反向耦合（2~4 天）

- 从 ShaderGen public 头中移除渲染器实现对象依赖
- 旧对象接口降级为内部实现细节或删除

**验收**：ShaderGen 可作为独立静态库被非渲染模块链接（仅依赖 contract）。

## Phase 5：清理与文档统一（1~2 天）

- 删除弃用接口、更新文档索引与规范
- 增加 CI 校验：禁止新代码跨层 include

**验收**：文档、代码、CI 规则一致。

---

## 6. 迁移风险与控制

### 风险 A：布局偏差（set/binding 不一致）

- 控制：双轨阶段做逐材质布局 diff（文本化输出）

### 风险 B：SPV 不一致

- 控制：按 stage 比较 hash + 反汇编结构关键段

### 风险 C：性能回退

- 控制：缓存键稳定化，确保新路径命中率不下降

### 风险 D：接口震荡

- 控制：contract version 字段 + 适配器层隔离

---

## 7. 验收标准（必须满足）

1. 渲染器只依赖 `shader-contract` + `shadergen-core` 的对外 DTO
2. ShaderGen 对外 API 不再暴露渲染器实现类型
3. 结果完整覆盖你要求的 4 类产物：
   - descriptor 信息
   - vertex input 信息
   - UBO/SSBO 信息
   - SPV
4. `03_BasicLitSunDirectionECS` 与 `ULRE.ShaderGen` 构建/运行回归通过

---

## 8. 推荐落地顺序（与你当前代码最匹配）

优先从 `MaterialManager` 消费面切：

1. 先定义 `ShaderGenResult`（不动内部生成逻辑）
2. 在 `MaterialCreateInfo` 末端导出 `ShaderGenResult`
3. 新建 `RendererShaderGenAdapter` 消费 `ShaderGenResult`
4. `MaterialManager` 切换调用新适配器
5. 最后再收缩/删除 `MaterialCreateInfo` 旧暴露面

这个顺序风险最低，因为先“加新桥”，再“撤旧桥”。

---

## 9. 建议新增文件（执行期）

- `inc/hgl/shadergen/contract/ShaderGenContract.h`
- `src/ShaderGen/contract/ShaderGenResultBuilder.cpp`
- `inc/hgl/graph/module/RendererShaderGenAdapter.h`
- `src/SceneGraph/module/RendererShaderGenAdapter.cpp`
- `doc/shader-system/SHADERGEN_RENDERER_DECOUPLING_PLAN.md`（本文）

---

## 10. 最终建议

- **是可行的**，而且值得做。  
- 建议采用“**双轨镜像 -> 适配器切换 -> 拆除旧耦合**”路线，不要一步到位硬切。  
- 保持你现有规范（Logic/Binding/Whitelist）不变，只重构边界与交付形态。
