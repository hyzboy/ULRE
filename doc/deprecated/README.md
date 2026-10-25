# 过时历史文档归档目录 (doc/deprecated)

本目录存放引擎演进过程中已被**彻底废除或取代**的历史 ShaderGen / 材质方案设计文档。
保留这些文档仅供翻阅历史背景与设计演进脉络，**严禁作为现行实现或未来设计的参考依据**。

---

## 现行权威架构设计文档（请阅读以下最新文档）

1. **`doc/shadergen-elimination-domain-fixed-architecture.md`**（2026-10 最新权威）
   - **彻底废除 ShaderGen 终极架构设计：基于 MaterialDomain 的固定着色器方案**
   - 确立 `MaterialDomain` 物理固定着色器，消灭 C++ AST 与拼接生成器，C++ 仅输出极简 `options.glsl`。
2. **`doc/ultra-thin-fixed-shader-architecture.md`**
   - 超薄极简着色器架构设计正文（Domain 契约、多态包含契约、消灭 `#if` 条件编译）。
3. **`doc/material-domain-contract-architecture.md`**
   - 表面领域契约层设计。
4. **`doc/shadergen-reduction-closure-report.md`**
   - ShaderGen 削减工程实测结项报告。

---

## 归档文档清单及废弃说明

| 文档名称 | 原主题 | 废弃原因说明 |
|---|---|---|
| `ShaderGen_MaterialGLSL_DesignAnalysis.md` | 早期材质 GLSL 生成器设计分析 | 基于旧版 5 层生成器模型与复杂 recipe 处理，现行架构已走向固定 Domain 着色器。 |
| `ShaderGen_ComposableTemplate_TechnicalDesign.md` | 受控组合模板技术方案 | 详述动态 AST 拼装与 FragmentTemplateComposer 拼接机制，该机制已整体定性为待废除。 |
| `simple-sphere-material-shadergen-overview.md` | SimpleSphere 到 ShaderGen 的工作链分析 | 详细追踪旧版 ShaderGen 生成流程，已过时。 |
| `material-lit-recipe-to-shadergen-dataflow.md` | Lit 材质数据流全链路分析 | 描述了通过生成器解析材质与生成专属着色器的历史链路，不符合「资产仅填参、不编译 SPV」的现行设计。 |
| `shadergen-generator-diagnostics-layering.md` | 真实生成器与诊断工具分层设计 | 试图对旧版 ShaderGen 做生成态与诊断态分层，现行方向为整体剔除生成器。 |
| `shadergen-refactoring-methodology.md` | 复杂度削减方法论 (2026-08) | 早期局部优化方法论，现行架构已升级为顶层固定 Domain 体系。 |
