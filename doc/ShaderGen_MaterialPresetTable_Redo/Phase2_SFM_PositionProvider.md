# Phase 2 技术文档：SFM 自描述与 PositionProvider 扩展

## 1. 阶段目标

建立“shader 需求可自描述、可机器校验、可回归审计”的基础能力，为后续 Matcher 路由提供可靠输入。

本阶段重点：

1. SFM 注解规范落地并覆盖核心 surface。
2. `ShaderRequirementSet` 解析链路稳定。
3. PositionProvider 扩展（含整数域 provider）可用。
4. lint/self-check 可阻断明显错误配置。

### 1.1 启动条件与当前状态（2026-06-20）

1. 进入条件已满足：
   - Phase0R Day 1 骨架已完成（四轴类型、桥接、最小 VT 测试与日志接入）。
   - `RecipeToKeyTest`、`MaterialRecipeStoreTests`、`01_Billboard` 已通过。
2. 本阶段输入边界：
   - 本阶段聚焦 SFM/Provider 的“需求可描述 + 可校验”。
   - billboard size 资源化（UBO/SSBO）仍属 Phase0R 收尾并行项，不在本阶段主线内做结构性改造。

---

## 2. 范围定义

### 2.1 纳入

1. SFM 解析：
   - `@sfm:require`
   - `@sfm:optional`
   - `@sfm:derive`
   - `@sfm:supports_phase`
   - `@sfm:surface_type`
2. Shader requirement 数据结构与解析实现。
3. ProviderManifest 与 position provider 的对齐。
4. 对核心 surface.glsl 的注解补齐。
5. SelfCheck / Audit 测试。

### 2.2 不纳入

1. Matcher runtime 路由接管（在 Phase 3）。
2. 大规模材质结构重构。

---

## 3. SFM 语法规范（本阶段基准）

建议采用如下模式：

1. `@sfm:surface_type <TypeName>`
2. `@sfm:supports_phase <PhaseA> <PhaseB> ...`
3. `@sfm:require va <AttribA> <AttribB> ...`
4. `@sfm:optional va <AttribX> ...`
5. `@sfm:derive va <AttribY> ...`
6. `@sfm:require tex <SlotNameA> <SlotNameB> ...`
7. `@sfm:require ubo <camera|transform|viewport|sky...>`
8. `@sfm:require ssbo <Name...>`

约束：

1. `derive` 必须是 `required U optional` 的子集。
2. 未声明 `supports_phase` 的 surface 视为仅支持默认前向阶段（按项目默认值）。
3. 不允许同一键重复矛盾声明。

---

## 4. PositionProvider 设计点

1. provider 仅负责位置输入语义，不直接承载材质逻辑。
2. 新增整数域 provider（如 `vab_ivec2`、`vab_uvec2`）时，必须在：
   - ProviderManifest
   - 解析器
   - 代码生成模板
   - 测试样例
   四处保持一致。
3. 对 PCG/fullscreen triangle 场景，需单独验证输出坐标语义。

## 4.1 LogCode 契约（复用模板）

1. 本阶段日志规则复用 `LogCode_Contract_Template.md`。
2. 解析/审计失败必须输出 `VT-ERR-*`，并带缺失或冲突键名。
3. 进入候选评估链路时，筛选与退化分别输出 `MT-MATCH-*` / `MT-FALLBACK-*`。

---

## 5. 建议改动文件

1. 头文件：
   - `inc/hgl/shadergen/ShaderRequirement.h`
   - `inc/hgl/shadergen/ShaderRequirementSet.h`
   - `inc/hgl/shadergen/ProviderManifest.h`
2. 源文件：
   - `src/ShaderGen/ShaderRequirementSet.cpp`
   - `src/ShaderGen/ProviderManifest.cpp`
   - `src/ShaderGen/PositionProviderRegistry.cpp`
3. ShaderLibrary：
   - `ShaderLibrary/surface/*.glsl`（核心 surface 顶部注解）
   - `ShaderLibrary/position_provider/*.glsl`
4. 测试：
   - `src/ShaderGen/tests/ShaderRequirementSetTests.cpp`
   - `src/ShaderGen/tests/SFMAuditSnapshotTests.cpp`

---

## 6. 执行步骤

1. 定义 SFM 解析 AST/结构体与错误码。
2. 完成注解解析与冲突检测。
3. 给核心 surface 批量补注解。
4. 为 PositionProvider 新增项补 manifest 与实现。
5. 建立 SFM 审计快照测试。
6. 跑示例回归并核验日志。

### 6.1 首批执行清单（Week 1）

1. 建立 SFM 键名白名单与错误码表（`require/optional/derive/supports_phase/surface_type`）。
2. 在 `ShaderRequirementSet` 中实现冲突检测（重复声明、矛盾声明、derive 越界）。
3. 选取核心 surface 子集补注解（先覆盖 billboard/unlit/standard 最常用路径）。
4. 为 PositionProvider 增加整数域样例并补齐 manifest/解析/模板三处一致性。
5. 增加两类测试：
   - 正向：注解完整时 requirement 集合稳定。
   - 负向：缺失/冲突时返回 `VT-ERR-*` 且给出冲突键名。
6. 输出第一版 SFM 审计快照，作为进入 Phase3 前的基线样本。

---

## 7. 验收标准

1. SFM 解析测试通过。
2. 审计测试能发现注解缺失/冲突问题。
3. 新增 PositionProvider 在目标示例中行为正确。
4. 不引入新增渲染错误或黑屏。
5. 日志前缀与字段满足 `LogCode_Contract_Template.md`。

---

## 8. 风险与防错

1. 风险：SFM 注解与实际 shader include 依赖不一致。
   - 防错：审计测试对 include 图与 requirement 交叉校验。
2. 风险：批量补注解出现拼写漂移。
   - 防错：phase/attrib/ubo 名称采用枚举映射或白名单校验。
3. 风险：provider 扩展破坏旧 provider 编号语义。
   - 防错：对外 ID 稳定性加兼容映射。

---

## 9. 回滚方案

1. 若解析器不稳定，先仅启用“宽松解析 + 警告模式”。
2. 如出现系统性回归，回滚本阶段并保留注解文本修改单独评估。

---

## 10. 阶段完成定义（DoD）

1. SFM 注解与解析链路稳定可测。
2. PositionProvider 扩展完成并可回归验证。
3. 具备进入 Phase 3（Matcher 路由接入）的输入条件。

---

## 11. 当前实现进展（2026-06-21）

已落地：

1. `CompositorAssembler` 已接入 surface 级 SFM 约束检查：
   - `@sfm:surface_type` 与 `MaterialVariantKey.surface_type` 一致性
   - `@sfm:supports_phase` 与 `PassType` 对应 phase 一致性
   - 非法/冲突注解通过 `VT-ERR-SFM-*` 失败返回
2. 已补首批核心 surface 注解基线：
   - `billboard_texture_surface.glsl`
   - `unlit_texture3d_surface.glsl`
   - `unlit_color3d_surface.glsl`
   - `standard_surface.glsl`
   - `sky_minimal_surface.glsl`
3. 已新增测试：
   - `CompositorAssemblerSFMConstraintTests.cpp`
   - `SFMAuditSnapshotTests.cpp`

当前阻塞：

1. 本地 `windows-ninja-debug` 仍受 `ShaderVariableType.h` 既有 constexpr/union 编译问题影响，导致新增测试目标无法在该终端独立构建验收。
2. 代码级与文件级校验已完成；建议在已知可通过链路执行上述测试目标确认。 