# Descriptor Semantic 自动绑定设计（固定管线）

## 1. 目标与边界

### 1.1 目标
- 材质仅声明“渲染语义需求”，不直接依赖 ECS 系统。
- 运行时自动完成“语义需求 -> 资源来源 -> Descriptor 绑定”。
- 缺失资源支持受控降级，并能被日志与统计审计。

### 1.2 固定管线边界（严格限制）
- 项目是“高级可定制的固定管线”，不是全自由材质图系统。
- 语义白名单严格受控，禁止任意字符串语义扩散。
- 降级档位有限且固定，不允许运行时无限组合。

---

## 2. 核心设计：四层依赖关系

本设计采用分层依赖图，避免材质直接耦合系统实现。

### 2.1 Layer A：Material Requirement（材质业务语义）
- 表达“材质业务上需要什么”，例如：`NeedSkyLighting`、`NeedCameraView`。
- 不出现任何 ECS 系统名。

### 2.2 Layer B：Binding Requirement（绑定语义契约）
- 将业务语义映射为具体绑定语义：`SkyInfoUBO`、`CameraInfoUBO`、`LocalToWorld` 等。
- 在本层定义：
  - `required/optional`
  - `allow_fallback`
  - 期望 set（View/Draw/Material）

### 2.3 Layer C：Provider Requirement（来源映射）
- 每个绑定语义对应一个 provider（数据来源），例如：
  - `SkyInfoUBO -> EnvironmentSystem`
  - `CameraInfoUBO -> CameraSystem`
  - `ViewportInfoUBO -> RenderTarget`
- 通过 provider 注册表统一维护，不写死在材质代码中。

### 2.4 Layer D：ECS System Dependency（执行时序）
- `RenderDescriptorBindingSystem` 声明依赖系统顺序，保证 provider 先于绑定执行。
- 系统依赖只出现在渲染绑定层，不出现在材质定义层。

### 2.5 一句话关系
- `MaterialSemantic -> BindingSemantic -> Provider -> ECS Dependency`

---

## 3. 当前实现状态（2026-03）

### 3.1 已完成
1. ECS 统一绑定入口：`RenderDescriptorBindingSystem`。
2. Provider 回调机制：支持注册多个 `BindingSource`。
3. 默认 provider：RenderTarget / CameraUBO / SkyUBO。
4. `CameraSystem` 已去除 cmd 绑定职责与 DescriptorBinding 持有职责。
5. 语义契约骨架：`DescriptorBindingContract.h`（header-only）。
6. `MaterialCompiler` 已接入契约校验诊断（不改变行为）。
7. `BindingContract` 已挂载到：
   - `MaterialCreateInfo`
   - 运行时 `Material`（只读元数据）

### 3.2 尚未启用（计划中）
- 运行时按 `BindingContract` 驱动实际 resolve/bind（当前仍是 provider 直绑主路径）。
- 统一 fallback 资源注册表。
- 降级统计与策略配置。

---

## 4. 语义白名单（v1）

### 4.1 DescriptorSemantic
- `ViewportInfo`
- `CameraInfo`
- `SkyInfo`
- `LocalToWorld`
- `MaterialInstance`
- `MaterialTexture`
- `MaterialSampler`
- `Custom`（迁移过渡）

### 4.2 固定 set 映射
- `ViewportInfo / CameraInfo / SkyInfo -> View`
- `LocalToWorld -> Draw`
- `MaterialInstance / MaterialTexture / MaterialSampler -> Material`

备注：旧 set 命名兼容仍存在，但语义层统一按 Scene/View/Draw/Material 解释。

---

## 5. 自动查找与自动绑定机制

### 5.1 编译/创建阶段
- `FixedDescriptorEntry[] -> BindingContract`
- 校验语义与 set 一致性，输出诊断（当前为 warning）。

### 5.2 渲染阶段（RenderFrameSync）
- `RenderDescriptorBindingSystem` 执行 provider 流程：
  - Resolve：语义需求解析来源
  - Apply：统一注入 descriptor binding

### 5.3 设计准则
- 禁止在业务系统里直接执行 `cmd->SetDescriptorBinding`。
- 新资源来源通过 provider 注册，不改系统主循环。

---

## 6. 降级策略（严格可控）

### 6.1 缺失分类
- **Required**：缺失即跳过绘制并限频告警。
- **Optional**：缺失可 fallback。

### 6.2 v1 默认规则
- `CameraInfo`：Required
- `LocalToWorld`：Required（若材质声明需要）
- `SkyInfo`：Optional + allow fallback
- `MaterialTexture/Sampler`：Optional + allow fallback

### 6.3 固定降级档位
- `Full`
- `NoSky`
- `NoTexture`
- `UnlitFallback`

---

## 7. 重新制定实现计划

### Phase A（已完成）
1. 集中绑定系统与 provider 机制。
2. 语义契约骨架与编译诊断。
3. contract 挂载到创建对象与运行时材质。

### Phase B（下一阶段，低风险）
1. 在 `RenderDescriptorBindingSystem` 增加“contract 只读解析通道”（旁路，不影响现绑定结果）。
2. 建立 `BindingSemantic -> Provider` 映射表（集中管理）。
3. 输出解析诊断：未命中 provider、依赖未就绪、使用 fallback。

### Phase C（主路径切换）
1. 启用 contract 驱动 resolve 为主路径。
2. provider 直绑路径保留为兼容回退开关。
3. 引入 fallback 资源注册表（DefaultSkyUBO/WhiteTexture/DefaultSampler）。

### Phase D（严格化与治理）
1. 在 CI 开启严格校验（语义/set 不一致可 fail）。
2. 发布版本默认 warning（可配置 strict 模式）。
3. 增加降级统计（frame/material/group 维度）。

---

## 8. 验收标准

### 8.1 功能
- 材质声明不出现系统耦合。
- 常见语义可自动绑定成功。
- 资源缺失能按策略降级，不出现崩溃/未定义行为。

### 8.2 可维护性
- 新来源通过注册 provider 扩展。
- 日志可定位：需求、来源、fallback、降级层级。

### 8.3 稳定性
- 核心样例与测试无行为回归（除新增诊断输出）。
- 可通过开关回退旧路径。

---

## 9. 风险与回退

### 9.1 风险
- 语义推断误判。
- 旧材质命名不规范导致 `Unknown/Custom` 偏高。
- 过早强校验导致存量内容失败。

### 9.2 回退策略
- 先旁路观测，再主路径切换。
- `Custom` 作为迁移缓冲语义继续保留。
- `strict` 仅在开发/CI 默认开启，发布按配置。

---

## 10. 开发约束（必须遵守）

1. 材质层禁止直接声明系统名。
2. provider 依赖表是系统层唯一真相来源。
3. 业务系统禁止直接做 descriptor 注入。
4. 新语义进入白名单前必须定义：set、required、fallback、provider。

---

## 11. 当前结论

该设计满足固定管线的三项核心诉求：
- **可预测**：语义与 set 关系严格。
- **可扩展**：通过 provider 注册扩展来源。
- **可治理**：可诊断、可降级、可回退。
