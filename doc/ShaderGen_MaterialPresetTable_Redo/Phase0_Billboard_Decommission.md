# Phase 0 技术文档：Billboard/Quad 专有路径退场

## 1. 阶段目标

在 Phase0R（VertexPolicy 解耦）完成后，将 Billboard/Quad 从“专有材质 preset + 专有 shader 路径”彻底退场，统一到“通用 surface 语义 + 可组合变换策略”模型。

本阶段是退场清理阶段，必须做到：

1. 行为等价（视觉与交互效果不退化）。
2. 不破坏 Phase0R 已接入的两轴策略（2D->3D / 旋转扭正）。
3. 所有示例保持可运行。

### 1.1 前置依赖

1. `Phase0R_VertexPolicy_Decouple.md` 已完成并通过门禁。
2. 旧 Billboard/Quad 调用点已具备到新策略轴的桥接映射。

---

## 2. 变更范围

### 2.1 纳入

1. `MaterialPreset` 中删除 Billboard/Quad 专有枚举项（或降级为仅兼容入口）。
2. 移除 Billboard/Quad 专有 surface / compositor shader 文件与引用。
3. 示例和调用点改为：
   - `preset = 通用 surface preset`（如 `UnlitTexture3D` / Text / 其它）
   - 几何升维与朝向策略由 Phase0R 两轴组合提供

### 2.2 不纳入

1. Matcher/PresetTable 逻辑。
2. MaterialVariantKey 与 GeometryMode 结构改动。
3. ColorSource、SFM、Overlay 等后续阶段内容。

---

## 3. 设计决策

1. Billboard/Quad 是“变换策略组合”，不是“材质类别”。
2. 材质 preset 保持关注“表面着色语义”（如 UnlitTexture3D），避免承载几何朝向语义。
3. 2D/3D 的分类与“旋转扭正”解耦：分别由几何升维轴与朝向轴控制。

---

## 4. 代码改动清单（建议）

1. 头文件与枚举：
   - `inc/hgl/mtl/MaterialPreset.h`
2. 3D 材质构建：
   - 删除 `src/ShaderGen/3d/M_BillboardDynamicSize.cpp`
   - 删除 `src/ShaderGen/3d/M_BillboardFixedSize.cpp`
3. ShaderLibrary 资源（按实际引用判定）：
   - 删除 `ShaderLibrary/compositor/main_forward_billboard_dynamic.vert.glsl`
   - 删除 `ShaderLibrary/compositor/main_forward_billboard_fixed.vert.glsl`
   - 删除 `ShaderLibrary/surface/billboard_texture_surface.glsl`
4. 示例与 ECS：
   - Billboard 示例构造 recipe 的调用点
   - 任何直接依赖 Billboard preset 的系统配置

---

## 5. 执行步骤

1. 盘点 Billboard/Quad 专有入口的所有引用（代码 + 资源路径 + 测试）。
2. 确认调用点均已映射到 Phase0R 新策略轴。
3. 替换示例与测试中的构造参数。
4. 删除未被引用的 Billboard/Quad 专有 shader 资源。
5. 编译并运行回归。

---

## 6. 验收标准

1. 编译通过：ShaderGen、SceneGraph、相关示例目标。
2. Billboard/Quad 场景显示正确：
   - 面向摄像机模式正常。
   - 轴锁定模式正常。
3. 材质创建链路无“找不到 Billboard/Quad 旧 preset”错误（兼容入口可降级提示）。
4. 性能无明显退化（至少无数量级变化）。

---

## 7. 风险与防错

1. 风险：示例仍使用旧 Billboard/Quad 入口，导致桥接绕回旧路径。
   - 防错：先全局替换调用点，再删枚举项。
2. 风险：删除 shader 文件后仍有硬编码路径引用。
   - 防错：执行路径扫描与运行时日志核验。
3. 风险：视觉结果轻微差异（例如 billboard 尺寸策略）。
   - 防错：保留尺寸参数语义，在 policy/参数映射层保持等价。

---

## 8. 回滚方案

1. 若出现系统性渲染异常，整体回滚 Phase 0 提交。
2. 回滚后先保留文件，仅恢复引用，再逐点修复。
3. 禁止带病进入 Phase 1。

---

## 9. 阶段完成定义（DoD）

1. Billboard 专有 preset 与专有 shader 路径全部退场。
2. 示例以通用 preset + vertex policy 运行稳定。
3. 阶段回归记录完成，允许进入 Phase 1。

---

## 10. 实施状态（2026-06-20，首轮）

### 10.1 已完成

1. `QuadResourcePrepareSystem` 的 billboard recipe 已切换为通用 preset：
   - preset 固定为 `UnlitTexture3D`
   - 使用 `vertex_policy` 区分固定尺寸与动态朝向
2. `QuadMaterialBindingSystem::BuildLegacyQuadRecipe` 已同步切换为通用 preset + `vertex_policy`。
3. `ShaderMaterialProgramManager` 已按 `prim/vertex_policy` 识别 billboard recipe，避免仅依赖 Billboard 专有 preset。
4. `BillboardTest` 示例 recipe 已迁移为 `UnlitTexture3D + BillboardAxisLocked`。
5. `RecipeToKey` 已支持 billboard-by-policy 语义：
   - blend 覆写不再只依赖 Billboard 专有 preset
   - geometry_mode 与 schema 可由 billboard prim/policy 正确推导
6. `EnumerateRecipeKeysTests` 已迁移到新表达方式（不再依赖 Billboard 专有 preset）。
7. `VariantRegistry` 已补齐 `UnlitTexture3D + Billboard*` 的兼容行（含 5 种 blend x 2 种 billboard policy），确保迁移调用链在当前 runtime 下可路由。

### 10.2 本轮保留（未删除）

1. `MaterialPreset::Billboard2DDynamic/Billboard2DFixed` 仍保留（兼容旧调用点）。
2. Billboard 专有 VS/surface 资源仍保留（兼容现有 row）。
3. `M_BillboardDynamicSize.cpp` / `M_BillboardFixedSize.cpp` 暂未从构建中移除。

### 10.3 下一步（Phase 0 收尾）

1. 完成旧 Billboard preset 调用点全量迁移。
2. 确认无运行期回归后，删除 Billboard 专有 preset 与专有 shader 路径。
3. 清理构建脚本中的 Billboard 专用源文件。

### 10.4 验证备注

1. 任务入口 `cmake --preset=default` 因本地生成器缓存不一致（VS17/VS18）未能完成构建。
2. 已对本轮改动文件执行编辑器级错误检查，未发现新增语义错误（RecipeToKey 存在与工程 includePath 配置相关的已知环境告警）。

### 10.5 实施状态（2026-06-20，第二轮）

1. `QuadResourcePrepareSystem::GetBillboardPresetForWorld` 已改为固定返回 `UnlitTexture3D`。
2. `QuadMaterialBindingSystem::BuildLegacyQuadRecipe` 已改为 `UnlitTexture3D + Billboard* vertex_policy`。
3. `ShaderMaterialProgramManager` 的 billboard 分支识别已改为 `prim/vertex_policy` 语义，不再依赖 Billboard preset。
4. `BillboardTest` 示例 recipe 已切换为 `UnlitTexture3D + BillboardAxisLocked`。

### 10.6 剩余收尾项

1. 删除/降级 `MaterialPreset::Billboard2DDynamic/Billboard2DFixed` 在 runtime registry 的参与路径（当前仅保留兼容用途）。
2. 从 `src/ShaderGen/CMakeLists.txt` 清理 `M_BillboardDynamicSize.cpp` / `M_BillboardFixedSize.cpp`，并确认无回归调用点。
3. 评估 `VertexProgramTemplates` / `SurfaceFragmentTemplates` 中 Billboard 专有模板是否可移除或重定向到通用路径。