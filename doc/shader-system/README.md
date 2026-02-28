# Shader System 规范文档索引

本目录包含 ULRE Shader System 重构的所有规范文档与设计分析。

**最后更新**：2026-02-28  
**总体计划**：[../../SHADER_SYSTEM_REFACTOR_PLAN.md](../../SHADER_SYSTEM_REFACTOR_PLAN.md)

---

## ✅ 最新里程碑（2026-02-28）

- `BasicLit / TextureBlinnPhong / Gizmo3D` 已完成 Composed-first 路径落地（保留 legacy fallback）
- SkyLight 统一接口已落地：`ULRE_GetSkyLightDir/Color/Ambient`，并支持 `SIMPLE / IBL / ENVMAP / SH` 模型切换
- 修复 DescriptorSet 写入生命周期问题（悬挂指针/扩容后指针失稳），运行期 descriptor invalid 错误已清除
- 示例 `06b_BasicLitMeshesECS` 与 `06c_TextureBlinnPhongMeshesECS` 已稳定运行并完成 Brickwall 三贴图链路
- 两示例已对齐 `RenderBoundBox` 风格：`VertexDataManager` + 同款模型集合 + 圆环布局
- 法线强度已升级为材质实例参数：`MaterialInstance.normal_strength`（默认 `0.35`，运行时可调）
- Helper 冲突诊断已机读闭环：新增 `test_ComposedDiagnosticsAggregation`，gate 可稳定导出 `gate-artifacts/composed-diagnostics.jsonl`（`count>=1`）
- 模板一致性门禁已扩展到 `BasicLit / TextureBlinnPhong / Gizmo`：新增 `test_Gizmo3DTemplateConformance` 后，当前 gate 聚焦集为 `10/10` 通过
- 现代固定管线文档已补齐：新增平台/档位白名单矩阵与机读草案（JSON），用于编辑器阶段校验/可视化；运行时以硬编码白名单为准
- Surface Set 后续扩展方向已切换为 Filament 语义参考（非 Unreal 主规范）；`Mask/DitherMask` 归 Composition 层统一托管

---

## 📋 核心规范文档（Phase B 产出）

按阅读顺序：

1. **[SHADER_HELPER_FUNCTION_SPEC.md](SHADER_HELPER_FUNCTION_SPEC.md)**  
   Helper 函数签名统一规范  
   - Position 获取：`GetLocalPosition()` / `GetWorldPosition()` / `GetClipPosition()` / `GetScreenPosition()`
   - MaterialInstance 获取：`GetMI()`
   - Normal 变换、矩阵获取规范
   - 废弃旧签名列表与迁移指南

2. **[SHADER_RESOURCE_NAMING_SPEC.md](SHADER_RESOURCE_NAMING_SPEC.md)**  
   资源命名与引用规范  
   - Descriptor 命名约定（camelCase：`camera` / `viewport` / `l2w` / `mtl`）
   - `required_resources` 与 `descriptor.name` 一致性要求
   - GLSL uniform block 实例名规范
   - 常见错误与修复指南

3. **[SHADER_LOGIC_CONSTRAINTS_SPEC.md](SHADER_LOGIC_CONSTRAINTS_SPEC.md)**  
   ShaderLogic 最小必填约束与验证规范  
   - `ShaderLogicBlock` 必填字段约束
   - `MaterialLogicDef` 结构体规范
   - 错误消息格式（`[Error]` / `[Warning]` / `[Info]`）
   - 运行时校验规则与伪代码

4. **[RESOURCE_LAYOUT_BINDING_STRATEGY.md](RESOURCE_LAYOUT_BINDING_STRATEGY.md)**  
   ResourceLayoutGenerator Binding 分配策略  
   - Descriptor Set 分配约定（Set 0-7 用途划分）
   - 固定映射 vs 自动分配策略说明
   - 冲突检测机制（位图算法）
   - Binding 分配最佳实践与调试技巧

5. **[PHASE_B_COMPLETION_SUMMARY.md](PHASE_B_COMPLETION_SUMMARY.md)**  
   Phase B 完成总结（含 2026-02-27 稳定化进展）  
   - 已完成任务清单
   - 待执行验收工作
   - 与 Phase A/C 的关系
   - 未解决问题与风险应对

6. **[SKYLIGHT_MODEL_UNIFIED_SPEC.md](SKYLIGHT_MODEL_UNIFIED_SPEC.md)**  
   SkyLight 统一模型接口规范（Phase C 扩展）  
   - 统一函数契约：`ULRE_GetSkyLightDir/Color/Ambient`
   - 模型切换：`SIMPLE / IBL / ENVMAP / SH`
   - Legacy/Composed 接入与分支写法统一

7. **[NEXT_STEPS_2026-02-27.md](NEXT_STEPS_2026-02-27.md)**  
   下一步执行清单（P0/P1）  
   - 运行时 normal_strength 参数化
   - descriptor 生命周期自动回归
   - shader business/main 一致性校验

8. **[PHASE_C_KICKOFF_2026-02-27.md](PHASE_C_KICKOFF_2026-02-27.md)**  
   Phase C 启动执行入口（本轮范围、门禁、首批任务、DoD）

9. **[PHASE_C_MATERIAL_MIGRATION_TEMPLATE_V1.md](PHASE_C_MATERIAL_MIGRATION_TEMPLATE_V1.md)**  
   Phase C 首批材质迁移模板（PureColor / VertexColor / Gizmo 共性约束与检查清单）

10. **[PHASE_C_BATCH1_MIGRATION_CHECKLIST_2026-02-28.md](PHASE_C_BATCH1_MIGRATION_CHECKLIST_2026-02-28.md)**  
   Phase C Batch-1 执行卡（BasicLit / TextureBlinnPhong）

11. **[PHASE_C_BATCH2_MIGRATION_CHECKLIST_2026-02-28.md](PHASE_C_BATCH2_MIGRATION_CHECKLIST_2026-02-28.md)**  
   Phase C Batch-2 执行卡（VertexLuminance3D / VertexPattleColor3D）

12. **[ModernFixedRenderPipeline.md](ModernFixedRenderPipeline.md)**  
   现代固定管线 Shader 生成器实现草案（特性轴、档位、缓存键与验收口径）

13. **[ModernFixedRenderPipeline_VariantMatrix.md](ModernFixedRenderPipeline_VariantMatrix.md)**  
   平台 × 档位 × 组合白名单矩阵（含 `AtmosphereSimple`、Composer 托管 `Mask/DitherMask` 约束）

14. **[ModernFixedRenderPipeline_VariantMatrix.draft.json](ModernFixedRenderPipeline_VariantMatrix.draft.json)**  
   机读配置草案（状态字段、白名单矩阵、预留轴与降级规则），用于编辑器阶段校验与工具链消费（非运行时强依赖）

15. **[RUNTIME_WHITELIST_INTEGRATION_DRAFT.md](RUNTIME_WHITELIST_INTEGRATION_DRAFT.md)**  
   运行时硬编码白名单接入草案（`constexpr` 结构、校验顺序、降级策略与日志约定）

---

## 📊 设计分析与历史文档

### ShaderGen 系统分析

- **[ShaderGen_Analysis_Report_EN.md](ShaderGen_Analysis_Report_EN.md)**  
  ShaderGen 系统架构分析报告（英文版）  
  - 现有系统的优势与问题
  - 重构建议与实施路线图

- **[ShaderGen_分析报告与改进建议.md](ShaderGen_分析报告与改进建议.md)**  
  ShaderGen 系统分析报告（中文版）  
  - 当前问题诊断
  - 改进方向建议

### 模板引擎设计

- **[ShaderTemplateEngine.md](ShaderTemplateEngine.md)**  
  Shader 模板引擎设计文档  
  - 模板语法设计
  - 变量替换与代码生成机制
  - 与 ShaderGen 的集成方案

---

## 🔗 相关文档

### 根目录重构计划

- [../../SHADER_SYSTEM_REFACTOR_PLAN.md](../../SHADER_SYSTEM_REFACTOR_PLAN.md)  
  Shader System 总体重构计划（Phase A - Phase E）

### 其他子系统文档

- [../ecs/](../ecs/) - ECS 系统文档
- [../refactor/](../refactor/) - 通用重构文档
- [../CMATH_DOCUMENTATION_INDEX.md](../CMATH_DOCUMENTATION_INDEX.md) - CMath 数学库文档索引

---

## 📖 快速导航

### 我是材质开发者

**阅读顺序**：
1. [SHADER_HELPER_FUNCTION_SPEC.md](SHADER_HELPER_FUNCTION_SPEC.md) - 了解可用的 helper 函数
2. [SHADER_RESOURCE_NAMING_SPEC.md](SHADER_RESOURCE_NAMING_SPEC.md) - 学习资源命名规则
3. [SHADER_LOGIC_CONSTRAINTS_SPEC.md](SHADER_LOGIC_CONSTRAINTS_SPEC.md) - 掌握 ShaderLogic 定义约束
4. [SKYLIGHT_MODEL_UNIFIED_SPEC.md](SKYLIGHT_MODEL_UNIFIED_SPEC.md) - 天光模型统一接口与切换规则
5. 参考示例：`src/ShaderGen/3d/S_PureColor3D.h` / `S_VertexColor3D.h`

### 我是框架维护者

**阅读顺序**：
1. [../../SHADER_SYSTEM_REFACTOR_PLAN.md](../../SHADER_SYSTEM_REFACTOR_PLAN.md) - 了解重构整体规划
2. [PHASE_B_COMPLETION_SUMMARY.md](PHASE_B_COMPLETION_SUMMARY.md) - 当前进度与待办事项
3. [NEXT_STEPS_2026-02-27.md](NEXT_STEPS_2026-02-27.md) - 当前迭代执行顺序与验收标准
4. [PHASE_C_KICKOFF_2026-02-27.md](PHASE_C_KICKOFF_2026-02-27.md) - Phase C 启动入口（执行视图）
5. [PHASE_C_MATERIAL_MIGRATION_TEMPLATE_V1.md](PHASE_C_MATERIAL_MIGRATION_TEMPLATE_V1.md) - 首批材质迁移模板（执行模板）
6. [PHASE_C_BATCH1_MIGRATION_CHECKLIST_2026-02-28.md](PHASE_C_BATCH1_MIGRATION_CHECKLIST_2026-02-28.md) - Batch-1 迁移执行卡（BasicLit/TextureBlinnPhong）
7. [PHASE_C_BATCH2_MIGRATION_CHECKLIST_2026-02-28.md](PHASE_C_BATCH2_MIGRATION_CHECKLIST_2026-02-28.md) - Batch-2 迁移执行卡（VertexLuminance3D/VertexPattleColor3D）
8. [RESOURCE_LAYOUT_BINDING_STRATEGY.md](RESOURCE_LAYOUT_BINDING_STRATEGY.md) - 理解 binding 分配机制
9. [ShaderGen_Analysis_Report_EN.md](ShaderGen_Analysis_Report_EN.md) - 系统架构深度分析
10. [ModernFixedRenderPipeline.md](ModernFixedRenderPipeline.md) - 固定管线生成策略总览
11. [ModernFixedRenderPipeline_VariantMatrix.md](ModernFixedRenderPipeline_VariantMatrix.md) - 白名单矩阵（人工维护）
12. [ModernFixedRenderPipeline_VariantMatrix.draft.json](ModernFixedRenderPipeline_VariantMatrix.draft.json) - 机读草案（工具消费入口）
13. [RUNTIME_WHITELIST_INTEGRATION_DRAFT.md](RUNTIME_WHITELIST_INTEGRATION_DRAFT.md) - 运行时硬编码接入草案（实现入口）

### 我想了解历史演进

**阅读顺序**：
1. [ShaderGen_分析报告与改进建议.md](ShaderGen_分析报告与改进建议.md) - 问题诊断
2. [../../SHADER_SYSTEM_REFACTOR_PLAN.md](../../SHADER_SYSTEM_REFACTOR_PLAN.md) - 重构计划
3. [PHASE_B_COMPLETION_SUMMARY.md](PHASE_B_COMPLETION_SUMMARY.md) - 当前成果
4. 各规范文档 - 最终规范

---

## 📝 文档维护规则

1. **唯一规范来源**：本目录是 Shader System 规范的唯一权威来源，不允许在其他位置维护"另一个版本"
2. **文档更新流程**：规范修改需经过 Phase 负责人审核，重大变更需更新 `SHADER_SYSTEM_REFACTOR_PLAN.md`
3. **版本控制**：每个文档底部维护变更历史表格
4. **冻结策略**：Phase B 完成后规范冻结，Phase E 前不再接受新增规范

---

## 🛠️ 工具与验证

### 编译时检查

- `ValidateMaterialLogicDef()` - ShaderLogic 结构体验证
- `ResourceLayoutGenerator::CheckAndMarkBinding()` - Descriptor binding 冲突检测
- Helper 注入冲突检测（Phase C 实现）

### 运行时诊断

- `PrintMaterialLogicDiagnostics()` - 材质逻辑诊断信息输出
- Binding Map 可视化（调试用）
- 详细错误消息（`[Error]` / `[Warning]` / `[Info]`）

### 测试用例

- `test/test_FSHelperConsistencyValidation.cpp` - FS business/main helper 一致性校验
- `test/test_DescriptorSetLifecycleRegression.cpp` - descriptor 生命周期回归
- `test/ShaderLogicValidationTest.cpp` - ShaderLogic 结构约束校验
- `test/run_shader_system_gate.ps1` - Shader System 门禁脚本（构建 + 聚焦执行）
- `test/ComposedDiagnosticsAggregationTest.cpp` - 诊断 JSON 行聚合触发与回归
- `test/Gizmo3DTemplateConformanceTest.cpp` - Gizmo 模板一致性语义断言回归

---

## 🚑 常见视觉问题排查

### 1) 画面暴白

- 优先检查材质实例参数是否已初始化（尤其是 `BaseColor`、`specular`、`roughness`、`normal_strength`）。
- 对照示例 `06b/06c` 的稳定默认值：`normal_strength` 建议 `0.30~0.45`。
- 确认光照模型分支与输入法线空间一致（切线空间法线图应走 TBN 路径）。

### 2) 凹凸方向反了（法线看起来内凹/外凸颠倒）

- 检查 normal 贴图 Y 分量方向（OpenGL/DX 约定差异）；当前示例已采用修正路径。
- 若素材来源混杂，优先通过材质参数或导入流程统一 normal Y 约定。

### 3) 画面全黑

- 先确认 descriptor 绑定有效（`imageView/sampler` 非空），再检查纹理回退是否生效。
- 确认 SkyLight 接口统一函数可用：`ULRE_GetSkyLightDir/Color/Ambient`。
- 用门禁脚本先跑回归：`test/run_shader_system_gate.ps1`，排除 helper/descriptor 基础问题。

---

## 🚀 06b/06c 快速复现（15 分钟）

### Step 1: 构建示例

- `cmake --build build --config Debug --target 06b_BasicLitMeshesECS 06c_TextureBlinnPhongMeshesECS -j 8`

### Step 2: 运行示例

- `./build/out/Windows_64_Debug/06b_BasicLitMeshesECS.exe`
- `./build/out/Windows_64_Debug/06c_TextureBlinnPhongMeshesECS.exe`

### Step 3: 预期画面要点

- 两个示例均为 `VertexDataManager` 路径，模型集合与 `RenderBoundBox` 对齐。
- `06c` 使用 Brickwall 三贴图（Albedo/Normal/Roughness），法线细节可见且方向正确。
- 默认 roughness/normal 生效，`normal_strength` 运行时可调（当前稳定值约 `0.35`）。

### Step 4: 快速排错顺序

- 先看是否“全黑”→ 检查 descriptor 与贴图回退。
- 再看是否“暴白”→ 检查材质实例参数初始化。
- 再看“凹凸反向”→ 检查 normal Y 约定与素材来源。

### 15 分钟上手检查清单

- [ ] 能成功构建并运行 `06b/06c`。
- [ ] 能确认 Brickwall 三贴图链路生效（不是仅 albedo）。
- [ ] 能通过 `normal_strength` 观察凹凸强度变化。
- [ ] 遇到异常时，能按“全黑→暴白→凹凸反向”顺序定位。
- [ ] 能运行 `test/run_shader_system_gate.ps1` 并看到 `[Gate] PASS`。

---

**文档负责人**：Shader System 维护团队  
**反馈渠道**：项目 Issue Tracker  
**最后审核**：2026-02-28（Phase C 文档口径同步）

---

## 变更历史

- 2026-02-28：文档口径同步（状态、门禁覆盖与时间戳对齐，无功能变更）。
- 2026-02-28：新增固定管线矩阵文档与机读 JSON 草案入口；同步 Filament 参考与 Composer 托管 Mask/DitherMask 口径。
