# Phase C 材质迁移模板（首批：PureColor / VertexColor / Gizmo）v1

本模板用于把 legacy/分散写法收敛到统一的 Composed-first 迁移路径。  
目标：新材质和存量材质都按同一约束落地，减少“能跑但不可维护”的分叉实现。

**最后更新**：2026-02-28  
**适用范围**：Phase C 第一批 3D 材质迁移  
**依赖文档**：
- [SHADER_LOGIC_CONSTRAINTS_SPEC.md](SHADER_LOGIC_CONSTRAINTS_SPEC.md)
- [SHADER_RESOURCE_NAMING_SPEC.md](SHADER_RESOURCE_NAMING_SPEC.md)
- [PHASE_C_KICKOFF_2026-02-27.md](PHASE_C_KICKOFF_2026-02-27.md)

---

## 1. 公共约束（所有迁移材质必须满足）

### 1.1 结构约束

- 必须同时提供：`FixedMaterialDef`（布局/资源契约）与 `ComposedMaterialDef`（业务函数入口）。
- 业务 GLSL 必须采用 `VertexShaderBusiness` / `FragmentShaderBusiness` 约定签名。
- 如提供 `MaterialLogicDef`，必须通过 `ValidateMaterialLogicDef()` 最小约束校验。
- 保留 legacy fallback（`*_DEF`），但新增功能优先在 `*_COMPOSED_DEF` 路径实现。

### 1.2 资源命名与依赖约束

- `required_resources` 名称必须与 `descriptors[].name` 完全一致（大小写敏感）。
- 优先使用标准名：`viewport` / `camera` / `l2w` / `mtl`。
- `required_helpers` 与业务代码实际调用保持一致，不允许“声明有、代码无”或“代码有、声明无”。

### 1.3 Helper 冲突约束

- 若业务代码自定义 helper 与 builtin 同名，框架应跳过 builtin 注入并产生冲突诊断。
- strict 模式（`ULRE_HELPER_CONFLICT_STRICT=1`）下应注入 `#error ULRE_HELPER_CONFLICT`。
- gate 工件必须可提取诊断 JSON 行到 `composed-diagnostics.jsonl`。

### 1.4 回归约束

- 必须接入 `test/run_shader_system_gate.ps1` 聚焦集。
- 至少具备 1 条“语义断言”级别回归（非仅编译通过）。
- 与示例相关材质需保证 `06b/06c` 运行不回退。

---

## 2. 首批三材质 Profile（模板实例）

## 2.1 PureColor3D（最小材质模板）

**用途**：最小闭环验证（无光照、无纹理、MaterialInstance 直出）。

**最小输入**：
- Vertex：`Position` + `TransformID` + `MaterialInstanceID`

**最小资源**：
- `viewport`、`camera`、`l2w`、`mtl`

**业务语义锚点**：
- VS：`return vec4(vi.Position, 1.0)`
- FS：`MaterialInstance mi = GetMI(); return mi.Color;`

**迁移定位**：
- 作为新材质迁移首站，先验证资源命名/业务签名/门禁接入链路。

## 2.2 VertexColor3D（插值通道模板）

**用途**：验证 VS->FS 插值与无 MI 路径。

**最小输入**：
- Vertex：`Position` + `Color` + `TransformID`

**最小资源**：
- `viewport`、`camera`、`l2w`

**业务语义锚点**：
- VS：`Output.Color = vi.Color;`
- FS：`return Input.Color;`

**迁移定位**：
- 作为“带插值输出”的模板，适用于无材质实例依赖的调试类/可视化类材质。

## 2.3 Gizmo3D（光照+helper 模板）

**用途**：验证 helper 依赖、法线路径、光照分支与冲突诊断。

**最小输入**：
- Vertex：`Position` + `Normal` + `TransformID` + `MaterialInstanceID`

**最小资源**：
- `viewport`、`camera`、`l2w`、`mtl`

**关键 helper**：
- `GetNormal`、`GetLocalToWorld`、`GetMI`（以及业务涉及的相机/光照 helper）

**业务语义锚点**：
- VS：法线与世界坐标输出
- FS：方向光 + 高光计算 + MI 颜色融合

**迁移定位**：
- 作为“复杂材质模板”，用于验证 helper 冲突处理与诊断工件导出。

---

## 3. 迁移执行模板（复制即用）

## 3.1 文件组织模板

- `S_<Material>.h`：保留 `*_DEF` 与 `*_COMPOSED_DEF`
- `S_<Material>_Logic.h`：可选，承载 `MaterialLogicDef`
- `test/<Material>...Test.cpp`：最小语义回归

## 3.2 迁移步骤模板

1. 定义/补齐 `FixedVertexEntry` 与 `FixedDescriptorEntry`。
2. 落地 `VertexShaderBusiness` / `FragmentShaderBusiness`。
3. 生成 `ComposedMaterialDef`，保留 legacy `FixedMaterialDef`。
4. 按需补 `MaterialLogicDef`（并校验 `required_resources/helpers`）。
5. 增加 1 条语义断言测试并接入 gate。
6. 运行 gate，确认 `PASS` 与诊断工件输出。

## 3.3 验收模板

- 编译：目标材质相关测试可编译通过
- 运行：示例可运行且无视觉回退
- 语义：关键锚点字符串断言通过
- 诊断：`composed-diagnostics.jsonl` 可提取到诊断 JSON（适用于 helper 冲突场景）

---

## 4. 首批共性检查清单（PureColor / VertexColor / Gizmo）

- [ ] 都具备 `*_DEF` + `*_COMPOSED_DEF` 双轨定义
- [ ] 资源命名统一使用标准名（`viewport/camera/l2w/mtl`）
- [ ] 业务函数签名统一（`VertexShaderBusiness` / `FragmentShaderBusiness`）
- [ ] 至少 1 条语义断言回归已纳入 gate
- [ ] helper 冲突场景可生成结构化诊断（含 strict 模式行为）
- [ ] gate 工件可稳定输出 `composed-diagnostics.jsonl`

---

## 5. 下一批材质接入建议（Phase C）

- 优先迁移“接口最接近 Gizmo 模板”的光照类材质（helper 复杂度中等）。
- 纹理类材质优先复用 VertexColor/PureColor 模板的输入与资源命名骨架。
- 新增材质必须先过模板检查清单，再进入示例联调。

---

## 6. Batch-1 执行卡

- BasicLit / TextureBlinnPhong 迁移清单：
	[PHASE_C_BATCH1_MIGRATION_CHECKLIST_2026-02-28.md](PHASE_C_BATCH1_MIGRATION_CHECKLIST_2026-02-28.md)
