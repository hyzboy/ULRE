# Shader System 重构执行计划（唯一版）

> 更新时间：2026-02-25  
> 文档定位：**本仓库 Shader 重构的唯一事实来源**。其它历史计划文档可删除。

---

## 1. 当前真实状态（与代码对齐）

### 1.1 运行主路径（稳定可用）

- 当前生产材质工厂仍走 `Std3DMaterial` 体系（稳定路径）：
  - `src/ShaderGen/3d/M_PureColor3D.cpp`
  - `src/ShaderGen/3d/M_VertexColor3D.cpp`
  - `src/ShaderGen/3d/M_Gizmo3D.cpp`
- 该路径可用，作为当前基线。

### 1.2 新架构代码状态（已存在但未全面接入）

- 已有纯逻辑接口与样例：
  - `inc/hgl/graph/mtl/ShaderLogic.h`
  - `src/ShaderGen/3d/S_PureColor3D_Logic.h`
  - `src/ShaderGen/3d/S_VertexColor3D_Logic.h`
  - `src/ShaderGen/3d/S_Gizmo3D_Logic.h`
- 已有桥接入口与生成器能力：
  - `BuildComposedMaterialDefFromLogic(...)`
  - `ComposedShaderGenerator` 中 `PipelineMode`、`PerVertex`、`MobileSubpass`、法线压缩等占位/路由能力。
- 已有布局生成器：
  - `ResourceLayoutGenerator` 可统一生成 descriptor / vertex input 布局，并做重复绑定检测。

### 1.3 关键事实（避免误判）

- `S_*_Logic.h` 目前主要是“样例与试验资产”，**尚未成为 3D 材质工厂默认编译路径**。
- `BuildComposedMaterialDefFromLogic(...)` 当前主要在测试中被使用，不是生产入口。
- 结论：现状是“**稳定主线 + 新体系半接入**”，不是“新体系已替换旧体系”。

---

## 2. 顶层约束（不变）

1. 以“单 Drawcall 全场景”为长期硬目标，禁止引入破坏批处理一致性的自由度。
2. 3D 材质走“现代固定管线 + 受控扩展”，不走完全自由拼接。
3. 开发者输入限定为：
   - 业务逻辑函数（受限签名）
   - `required_resources`
   - `required_helpers`
4. 运行时数据链路固定：`TransformID/MI_ID` 间接寻址（对象级 descriptor 不切换）。
5. 所有新增能力必须满足“可回退到稳定路径”的发布要求。

---

## 3. 阶段路线图（按“先接入、再扩展、后清理”）

## Phase A：主线接入（M1，必须完成）

**目标**：让至少一个生产材质真正走“Logic -> Bridge -> ComposedGenerator -> CompileFixedMaterial”。

### A.1 任务

- [ ] 选择首个迁移材质：`PureColor3D`（优先，复杂度最低）。
- [ ] 在 `CreatePureColor3D` 路径接入桥接：
  - base: `PURE_COLOR_3D_COMPOSED_DEF`
  - logic: `PURE_COLOR_3D_LOGIC`
  - bridge: `BuildComposedMaterialDefFromLogic`
  - compile: `CompileFixedMaterial`
- [ ] 增加回退开关：桥接失败/编译失败时回退 `Std3DMaterial` 旧路径。
- [ ] 补充日志：输出缺失资源、缺失 helper、实际走的路径（new/old）。

### A.2 验收

- [ ] `PureColor3D` 在新路径下渲染正确。
- [ ] 强制制造缺失资源时能自动回退到旧路径。
- [ ] 不破坏现有材质加载与渲染流程。

---

## Phase B：一致性修正（M1.5，紧随 A）

**目标**：把接口语义和 helper 行为对齐，避免“看起来能用，实则脆弱”。

### B.1 任务

- [ ] 统一 helper 签名：
  - 明确 `GetWorldPos()` / `GetWorldPos(vec3)` 最终规范，只保留一种主签名。
- [ ] 统一 `required_resources` 命名规则（是否以 descriptor `name` 为准）。
- [ ] 明确 `ShaderLogic.h` 的最小必填约束与错误信息格式。
- [ ] 修正 `ResourceLayoutGenerator` 的 binding 分配策略文档与实现一致性（自动分配 vs 固定映射）。

### B.2 验收

- [ ] 三个逻辑样例（PureColor/VertexColor/Gizmo）都能通过桥接校验。
- [ ] helper 注入行为可预测、无重复定义。

---

## Phase C：批量迁移（M2）

**目标**：把典型 3D 材质批量切到新路径，并保持稳定。

### C.1 迁移顺序

1. [ ] `VertexColor3D`
2. [ ] `Gizmo3D`
3. [ ] `BasicLit`
4. [ ] `TextureBlinnPhong`

### C.2 每个材质统一流程

- [ ] 接入 bridge + fallback。
- [ ] 生成 GLSL 编译通过（VS/FS）。
- [ ] 样例渲染通过（截图或自动比对）。
- [ ] 补充一条回归测试。

---

## Phase D：模式矩阵能力收敛（M3）

**目标**：将已有占位能力变成可约束、可验证的产品级能力。

### D.1 任务

- [ ] `PipelineMode` 组合白名单与非法组合错误报告。
- [ ] `PostProcess` 输出掩码必须为 `GBufferFormat` 子集。
- [ ] `Forward PerVertex` 与 `PerPixel` 的误差阈值定义与测试。
- [ ] `MobileSubpassGBuffer` 路径最小可运行样例。

### D.2 验收

- [ ] 非法组合在生成阶段被拦截（可读错误）。
- [ ] 模式切换不改变同一材质的参数语义。

---

## Phase E：收口与清理（M4）

**目标**：减少双轨成本，明确长期主入口。

### E.1 任务

- [ ] 当迁移覆盖率达到阈值（建议 >= 80% 3D 标准材质）后，冻结旧路径新增功能。
- [ ] 旧路径仅保留兼容与紧急回退，不再扩展。
- [ ] 生成“迁移覆盖率报告 + 未迁移清单”。

### E.2 验收

- [ ] 新入口成为默认入口。
- [ ] 旧入口可选、可控、可最终下线。

---

## 4. 测试与发布门禁

## 4.1 最低门禁（每次合并必须满足）

- [ ] `ULRE.ShaderGen` 可编译。
- [ ] `test_ComposedShaderGenerator` 通过。
- [ ] 至少 1 个迁移材质运行时渲染验证通过。
- [ ] 出错时可自动 fallback 到旧路径。

## 4.2 回归维度

- [ ] 资源缺失诊断。
- [ ] helper 注入冲突检测。
- [ ] descriptor 重复 binding 检测。
- [ ] 关键模式路由（Forward / MobileSubpass / PerVertex）。

---

## 5. 风险清单与应对

1. **双轨长期并存导致维护成本高**  
   - 应对：Phase E 设置覆盖率阈值与冻结策略。

2. **接口漂移导致业务代码与 helper 不匹配**  
   - 应对：Phase B 先统一签名和命名规范再批量迁移。

3. **生产接入失败影响可用性**  
   - 应对：每个材质保留 fallback；新路径失败不影响渲染。

---

## 6. 本周执行清单（可直接开工）

- [ ] A.1 接入 `PureColor3D` 新路径（含 fallback）。
- [ ] 增加“路径选择日志 + bridge 失败诊断日志”。
- [ ] 完成 `PureColor3D` 运行验证。
- [ ] 修正文档中的 helper 签名规范（GetWorldPos 统一）。
- [ ] 更新本文件状态勾选并记录结果。

---

## 7. 文档治理规则

- 本文件是 Shader 重构唯一计划文档。  
- 其它同主题计划文档删除后，不再恢复多文档并行维护。  
- 状态更新只改本文件，禁止在其它 markdown 记录“另一个版本的进度”。
