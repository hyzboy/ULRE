# Material / ResourceDomain 渐进改造计划（稳态优先）

## 1. 目标与约束

### 目标
- 将现有职责拆分为：
  - Material：Shader + Pipeline/Layout 模板
  - ResourceDomain：可共享的资源域（MI 数据、Texture/Sampler、后续 Layer SSBO）
  - DomainMaterialBinding：ResourceDomain 与某个 Material 的绑定视图
- 支持以下两类复用：
  - 同一 Shader，多套资源域（UI 图标 vs 头像 Billboard）
  - 同一资源域，多个 Material/Pipeline（Opaque + Masked）

### 约束
- 每一步都可单独编译通过。
- 每一步都可单独运行并做回归。
- 以兼容旧接口为前提，先加能力，再迁移调用方，最后收敛旧路径。

## 2. 总体迁移策略

采用“双轨并行 + 开关切换”：
- 旧轨：Material + MaterialInstance 原路径保持可用。
- 新轨：ResourceDomain + DomainMaterialBinding 渐进接入。
- 每阶段引入明确 Feature Flag，默认保守配置。

建议新增编译/运行开关：
- HGL_ENABLE_RESOURCE_DOMAIN（默认 0）
- HGL_ENABLE_DOMAIN_BINDING_IN_RENDER_SYSTEM（默认 0）
- HGL_ENABLE_DOMAIN_BATCH_KEY（默认 0）

## 3. 阶段计划（每阶段可独立编译运行）

## Phase 0：准备与护栏（不改行为）

### 改动
- 新增类型声明与空实现（不接入主流程）：
  - ResourceDomain
  - DomainMaterialBinding
- 在 MaterialManager 增加创建接口草案，但不被业务调用。
- 增加日志与统计点：
  - 每帧 Material 数
  - 每帧 MaterialInstance 数
  - 每帧 descriptor 绑定次数

### 编译/运行预期
- 行为 100% 不变。

### 测试
- 全量编译。
- 启动主场景，检查渲染结果与基线一致。

### 回滚
- 可整段删除新增类型文件，无联动风险。

## Phase 1：引入 ResourceDomain（仅管理 MI 数据）

### 改动
- 将 MI 数据池逻辑从 Material 中“复制一份”到 ResourceDomain（先不移除 Material 旧逻辑）。
- MaterialInstance 增加可选 domain 指针：
  - 无 domain 时走旧逻辑
  - 有 domain 时读写 domain 的 MI 池
- MaterialManager 新增：
  - CreateResourceDomain(Material*)
  - CreateMaterialInstance(ResourceDomain*, ...)

### 编译/运行预期
- 默认仍走旧路径。
- 打开 HGL_ENABLE_RESOURCE_DOMAIN 后，可在小范围场景验证新路径。

### 测试
- 用同一 Material 创建两组实例：
  - 组 A 走旧路径
  - 组 B 走 domain 路径
- 验证两组 MI 数据互不影响。

### 回滚
- 关闭 HGL_ENABLE_RESOURCE_DOMAIN 即恢复旧行为。

## Phase 2：引入 DomainMaterialBinding（资源绑定按域隔离）

### 改动
- DomainMaterialBinding 持有：
  - ResourceDomain*
  - Material*
  - 该 pair 的资源绑定表（Texture/Sampler/后续 SSBO）
- RenderDescriptorBindingSystem 新增注册接口：
  - RegisterDomainTextureSampler(binding_view, name, tex, sampler)
- 旧接口 RegisterMaterialTextureSampler 保留并继续可用。

### 编译/运行预期
- 默认旧接口继续生效。
- 开关开启后，新接口优先生效。

### 测试
- 同一 Material，两个 ResourceDomain，各自绑定不同 Texture2DArray（先可用 simple/2D 验证）。
- 画面应呈现两套不同贴图，且互不串绑。

### 回滚
- 关闭 HGL_ENABLE_DOMAIN_BINDING_IN_RENDER_SYSTEM。

## Phase 3：支持“同一资源域 + 多 Material/Pipeline”（Opaque + Masked）

### 改动
- 一个 ResourceDomain 可创建多个 DomainMaterialBinding：
  - (domain, opaque_material)
  - (domain, masked_material)
- 增加兼容性检查：
  - MI stride 兼容
  - 必要 descriptor semantic 可映射
  - 不兼容时输出诊断并拒绝绑定视图创建

### 编译/运行预期
- 不影响旧路径。

### 测试
- 同一 domain 绑定 Opaque 和 Masked 两个 material：
  - 两个 pass 都可渲染
  - 共享同一组 MI 数据与资源域内容

### 回滚
- 不创建多 material binding 即可退回单 material 使用。

## Phase 4：批次键切换到 Domain 维度（避免误合批）

### 改动
- 扩展 MaterialPipelineKey：
  - Material* + Pipeline* + ResourceDomain*（或 DomainMaterialBinding*）
- RenderFrameCache/materialBatches 改为新键。
- 增加过渡适配：domain 为空时使用 default domain。

### 编译/运行预期
- 旧数据可兼容（default domain）。

### 测试
- 同 Material/Pipeline，不同 domain 的对象不应被错误合批。
- draw call 与批次统计应符合预期。

### 回滚
- 关闭 HGL_ENABLE_DOMAIN_BATCH_KEY，恢复旧键。

## Phase 5：收敛旧路径（谨慎收口）

### 改动
- 将 Material 中运行态资源职责逐步下放到 ResourceDomain：
  - mi_data_manager
  - 与资源绑定有关的状态
- 保留 Material 作为模板对象。
- 标记旧 API 为 deprecated，并分版本移除。

### 编译/运行预期
- 新路径成为默认。

### 测试
- 全场景回归 + 性能回归 + 内存回归。

### 回滚
- 通过 compatibility shim 保持至少一个版本可回退。

## 4. 每阶段通用验收清单

- 编译：Debug/Release 全通过。
- 运行：基础样例、UI、角色场景三类 smoke test 通过。
- 正确性：
  - 纹理绑定不串域
  - MI 数据不串实例
  - Opaque/Masked 可并存
- 稳定性：
  - 无新增崩溃
  - 关键日志无持续告警

## 5. 推荐实施顺序（最稳）

1. Phase 0
2. Phase 1
3. Phase 2
4. Phase 3
5. Phase 4
6. Phase 5

理由：先保证“能力可用”，再改“系统关键路径（批次键）”，最后再“职责收口”，风险最小。

## 6. 首批最小落地任务（两周建议）

### Week 1
- 完成 Phase 0 + Phase 1
- 目标：ResourceDomain 可创建并承载 MI 数据，不影响旧逻辑

### Week 2
- 完成 Phase 2 + Phase 3 的最小闭环
- 目标：
  - 同 shader 多域可用
  - 同域多 material（Opaque/Masked）可用

Phase 4/5 放到后续迭代，避免一次性大改造成联动风险。
