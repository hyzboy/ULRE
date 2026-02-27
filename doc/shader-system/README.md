# Shader System 规范文档索引

本目录包含 ULRE Shader System 重构的所有规范文档与设计分析。

**最后更新**：2026-02-27  
**总体计划**：[../../SHADER_SYSTEM_REFACTOR_PLAN.md](../../SHADER_SYSTEM_REFACTOR_PLAN.md)

---

## ✅ 最新里程碑（2026-02-27）

- `BasicLit / TextureBlinnPhong / Gizmo3D` 已完成 Composed-first 路径落地（保留 legacy fallback）
- SkyLight 统一接口已落地：`ULRE_GetSkyLightDir/Color/Ambient`，并支持 `SIMPLE / IBL / ENVMAP / SH` 模型切换
- 修复 DescriptorSet 写入生命周期问题（悬挂指针/扩容后指针失稳），运行期 descriptor invalid 错误已清除
- 示例 `06b_BasicLitMeshesECS` 与 `06c_TextureBlinnPhongMeshesECS` 已稳定运行并完成 Brickwall 三贴图链路
- 两示例已对齐 `RenderBoundBox` 风格：`VertexDataManager` + 同款模型集合 + 圆环布局
- 新增可调法线强度宏：`ULRE_NORMAL_STRENGTH`（当前默认 `0.35`）

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
4. [RESOURCE_LAYOUT_BINDING_STRATEGY.md](RESOURCE_LAYOUT_BINDING_STRATEGY.md) - 理解 binding 分配机制
5. [ShaderGen_Analysis_Report_EN.md](ShaderGen_Analysis_Report_EN.md) - 系统架构深度分析

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

- `test/test_phase_b_validation.cpp` - Phase B 验收测试（计划）
- `test/test_duplicate_binding.cpp` - Binding 冲突测试用例（计划）

---

**文档负责人**：Shader System 维护团队  
**反馈渠道**：项目 Issue Tracker  
**最后审核**：2026-02-27（Phase C 稳定化更新）
