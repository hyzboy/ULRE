# Shader System 文档索引（现行基线）

本目录仅保留当前有效的 Shader System 设计与规范文档。  
历史阶段文档、旧方案分析文档、迁移执行卡已移除，不再作为开发依据。

**最后更新**：2026-03-01  
**总体计划**：[../../SHADER_SYSTEM_REFACTOR_PLAN.md](../../SHADER_SYSTEM_REFACTOR_PLAN.md)

---

## 1) 基线入口（先读）

1. [ModernFixedRenderPipeline.md](ModernFixedRenderPipeline.md)  
   现代固定管线现行基线：当前唯一开发口径与禁止项。

2. [ModernFixedRenderPipeline_VariantMatrix.md](ModernFixedRenderPipeline_VariantMatrix.md)  
   平台 × 档位 × 组合白名单矩阵（人工可读版本）。

3. [ModernFixedRenderPipeline_VariantMatrix.draft.json](ModernFixedRenderPipeline_VariantMatrix.draft.json)  
   机读草案（工具链校验/可视化入口）。

---

## 2) 规范文档

4. [SHADER_HELPER_FUNCTION_SPEC.md](SHADER_HELPER_FUNCTION_SPEC.md)  
   Helper 函数签名与注入约束。

5. [SHADER_RESOURCE_NAMING_SPEC.md](SHADER_RESOURCE_NAMING_SPEC.md)  
   资源命名、Descriptor 引用一致性规范。

6. [SHADER_LOGIC_CONSTRAINTS_SPEC.md](SHADER_LOGIC_CONSTRAINTS_SPEC.md)  
   ShaderLogic / MaterialLogicDef 约束与校验规则。

7. [RESOURCE_LAYOUT_BINDING_STRATEGY.md](RESOURCE_LAYOUT_BINDING_STRATEGY.md)  
   ResourceLayoutGenerator 的 binding 分配策略。

8. [SKYLIGHT_MODEL_UNIFIED_SPEC.md](SKYLIGHT_MODEL_UNIFIED_SPEC.md)  
   SkyLight 统一接口与模型切换规范。

9. [RUNTIME_WHITELIST_INTEGRATION_DRAFT.md](RUNTIME_WHITELIST_INTEGRATION_DRAFT.md)  
   运行时白名单接入草案（实现侧约束）。

---

## 3) 设计补充

10. [ShaderTemplateEngine.md](ShaderTemplateEngine.md)  
    模板引擎设计与生成机制说明。

11. [Ambient.md](Ambient.md)  
    环境光相关补充说明。

---

## 4) 使用建议

- 新增材质功能前：先对齐 `ModernFixedRenderPipeline.md` 的当前约束。
- 调整组合策略时：同步修改 `VariantMatrix.md` 与 `VariantMatrix.draft.json`。
- 修改 helper / 资源命名 / 逻辑结构时：同步对应 SPEC 文档。

---

## 5) 维护规则

1. 本目录文档仅维护“现行可执行方案”。
2. 本目录不维护与现行基线无关的内容。
3. 涉及行为变更的文档更新，必须与代码改动同批提交。

---

## 变更历史

- 2026-03-01：移除旧阶段/旧方案文档，README 重写为现行基线索引。