# Phase C Batch-1 迁移清单（BasicLit / TextureBlinnPhong）

本清单将模板文档落到两个真实目标材质上，作为下一轮代码实施的直接执行卡。  
模板来源： [PHASE_C_MATERIAL_MIGRATION_TEMPLATE_V1.md](PHASE_C_MATERIAL_MIGRATION_TEMPLATE_V1.md)

**最后更新**：2026-02-28

---

## 1) 目标与范围

- 目标材质：`BasicLit_v2`、`TextureBlinnPhong_v2`
- 目标文件：
  - `src/ShaderGen/3d/M_BasicLit.cpp`
  - `src/ShaderGen/3d/M_TextureBlinnPhong.cpp`
- 本轮不做：渲染模型升级、IBL 新功能扩展、legacy 路径移除

---

## 2) 当前状态快照（已具备能力）

### 2.1 BasicLit

- 已具备 `FixedMaterialDef + ComposedMaterialDef + MaterialLogicDef`。
- 已接入 `CompileComposedBusinessMaterial(...)`，失败自动回退 legacy。
- 现状缺口：`IBL=true` 分支仍强制走 legacy（显式短路）。
- 现状缺口：逻辑定义与材质定义同文件混排，尚未形成 `S_BasicLit_Logic.h` 模板形态。

### 2.2 TextureBlinnPhong

- 已具备 `FixedMaterialDef + ComposedMaterialDef + MaterialLogicDef`。
- 已接入 `CompileComposedBusinessMaterial(...)` + fallback。
- 现状缺口：逻辑定义与材质定义同文件混排，尚未形成 `S_TextureBlinnPhong_Logic.h` 模板形态。

---

## 3) Batch-1 公共迁移动作（两材质都执行）

- [x] 拆分逻辑定义：将 `*_VS_BUSINESS / *_FS_BUSINESS / *_LOGIC` 提取到 `S_<Material>_Logic.h`。
  - 进度：`TextureBlinnPhong` 与 `BasicLit` 已完成并接回。
- [ ] 保持资源命名契约一致：`required_resources` 与 descriptor `name` 逐项对齐。
- [ ] 固化 helper 依赖清单：避免“业务调用了 helper 但 required_helpers 未声明”。
- [ ] 新增模板一致性测试（语义断言级，不是仅编译级）。
- [ ] 纳入 gate 聚焦集并保持 `PASS`。

---

## 4) 材质专属清单

## 4.1 BasicLit 执行项

- [x] 生成 `S_BasicLit_Logic.h`，迁出 `BASIC_LIT_*_BUSINESS` 与 `BASIC_LIT_LOGIC`。
- [ ] 明确 FS 依赖清单包含：`camera/sky/mtl/TextureBaseColor/TextureNormal/TextureRoughness`。
- [ ] 增加 `BasicLit` 模板一致性语义断言，至少覆盖：
  - `ResolveRuntimeNormalStrength(mi.normal_strength)`
  - `ULRE_GetSkyLightDir/Color/Ambient`
  - `return vec4(color, 1.0)`
- [ ] 定义 IBL 分支迁移策略（二选一并落文档）：
  - A. 继续 legacy（显式标注“已知保留点”）
  - B. 接入 permutation/define 到 composed 路径（优先长期方案）

## 4.2 TextureBlinnPhong 执行项

- [x] 生成 `S_TextureBlinnPhong_Logic.h`，迁出 `TEXTURE_BLINN_PHONG_*_BUSINESS` 与 `*_LOGIC`。
- [ ] 保持 FS 纹理链路语义锚点：`TextureBaseColor/TextureNormal/TextureRoughness`。
- [ ] 增加模板一致性语义断言，至少覆盖：
  - `ResolveSurfaceUV`
  - `ResolveSurfaceNormal`
  - `fresnelSchlick`
  - `return vec4(color, 1.0)`

> 2026-02-28 实测：`06c_TextureBlinnPhongMeshesECS` 可编译，`test/run_shader_system_gate.ps1` 通过且诊断工件 `count=1`。

> 2026-02-28 增量：`06b_BasicLitMeshesECS` 可编译，`test/run_shader_system_gate.ps1` 再次通过且诊断工件 `count=1`。

---

## 5) 建议测试落点（Batch-1）

- 新增：`test/BasicLitTemplateConformanceTest.cpp`
- 新增：`test/TextureBlinnPhongTemplateConformanceTest.cpp`
- 两测试都采用“生成 GLSL 后关键语义锚点断言”模式，与 `BridgeValidation3Materials` 保持一致风格。

可选复用：
- `test/test_ComposedShaderGenerator.cpp`
- `test/test_ComposedShaderGenerator_Verify.cpp`

---

## 6) 验收标准（Batch-1 完成定义）

- [ ] 两个材质都完成“定义层与逻辑层分离”（迁移到模板结构）。
- [ ] 两个模板一致性测试通过并纳入 gate。
- [ ] `06b_BasicLitMeshesECS`、`06c_TextureBlinnPhongMeshesECS` 运行无回退。
- [ ] gate 维持 `PASS`，且诊断工件链路不受影响。

---

## 7) 执行顺序（推荐）

1. 先做 `TextureBlinnPhong`（无 IBL 特判，路径更直）。
2. 再做 `BasicLit`（同步处理 IBL 分支策略）。
3. 最后一次性接入两个 conformance 测试到 gate。
