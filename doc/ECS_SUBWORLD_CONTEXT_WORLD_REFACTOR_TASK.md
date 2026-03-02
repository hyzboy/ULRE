# ECS 子世界架构重构任务卡（Context / World / System）

**版本**: v0.1  
**日期**: 2026-03-03  
**状态**: Backlog（未开始）

---

## 1) 目标（Goal）

- 解决当前架构偏差：`SubWorld` 不再持有独立 `System` 集合。
- 明确并收敛职责边界：
  - `Context`：运行时资源与调度入口（渲染上下文、设备、调度器等）；
  - `World`：实体/组件组织与层级；
  - `System`：仅在主世界（或统一调度域）注册、生命周期管理与执行。
- 提供子世界运行模型：通过“上下文继承 / 只读视图 / 显式注入”访问主调度域能力，避免状态分叉与重复同步。

---

## 2) 验收标准（Acceptance Criteria）

- 架构约束
  - `SubWorld` 侧不再创建或持有独立 `System` 容器。
  - `System` 注册、启停、执行顺序仅由主调度域统一管理。
- 行为一致性
  - 子世界渲染路径中相机、视口、RenderContext 来源单一且可追踪。
  - 不再需要“子世界 camera UBO 镜像同步”这类补丁式逻辑。
- 回归验证
  - `05_GizmoUsageExample` 与 `02_AtmosphereSkySunGizmo` 在交互场景下无相机 UBO 全零问题。
  - 关键错误不再出现：`descriptor ... has never been updated`（与 gizmo 子世界相机链路相关项）。
- 工程质量
  - 提供迁移说明（旧接口到新接口映射）；
  - 提供最小化开关或阶段迁移策略，支持分步落地与回滚。

---

## 3) 风险与缓解（Risks & Mitigations）

- 风险 A：改动面大，可能影响现有 Render/Tick 时序。
  - 缓解：先引入兼容层（Adapter/Facade），再迁移调用点，最后移除旧路径。
- 风险 B：子世界功能（gizmo、overlay、工具链）对隐式 system 依赖较深。
  - 缓解：先做依赖盘点（谁在子世界取 system），逐项替换为显式依赖注入。
- 风险 C：运行时问题隐蔽（仅在 draw-only/子世界路径出现）。
  - 缓解：增加调试可观测性（上下文来源、camera/viewport 来源日志，按示例可开关）。
- 风险 D：一次性重构导致回归面过大。
  - 缓解：分 3 阶段推进：
    1. 约束建立（禁止新子世界 system）
    2. 兼容迁移（主调度域托管）
    3. 清理收尾（删除旧通道与临时同步逻辑）

---

## 4) 备注（Out of Scope）

- 本任务卡不覆盖 ShaderGen/材质 Contract 方案本身；
- 本任务卡不要求当期完成，仅作为后续 ECS 架构治理任务入口。