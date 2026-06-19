# Phase 0 技术文档：Billboard 专有材质退场

## 1. 阶段目标

将 Billboard 从“专有材质 preset + 专有 shader 路径”改为“通用 preset + 顶点策略（VertexTransformPolicy）组合”，以减少材质分支爆炸与维护成本。

本阶段是低风险清理阶段，必须做到：

1. 行为等价（视觉与交互效果不退化）。
2. 不触及 Matcher 主路径与 key 体系。
3. 所有示例保持可运行。

---

## 2. 变更范围

### 2.1 纳入

1. `MaterialPreset` 中删除 Billboard 专有枚举项。
2. 移除 Billboard 专有 surface / compositor shader 文件与引用。
3. 示例和调用点改为：
   - `preset = UnlitTexture3D`（或项目约定通用 preset）
   - `vertex_transform_policy = BillboardCameraFacing / BillboardAxisLocked`

### 2.2 不纳入

1. Matcher/PresetTable 逻辑。
2. MaterialVariantKey 与 GeometryMode 结构改动。
3. ColorSource、SFM、Overlay 等后续阶段内容。

---

## 3. 设计决策

1. Billboard 是“顶点变换策略”，不是“材质类别”。
2. 材质 preset 保持关注“表面着色语义”（如 UnlitTexture3D），避免承载几何朝向语义。
3. 2D/3D 的分类不通过 Billboard preset 表达，统一由 recipe 的维度和 policy 控制。

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

1. 盘点 Billboard preset 的所有引用（代码 + 资源路径 + 测试）。
2. 在 recipe 组装层将 Billboard preset 改写为通用 preset + policy。
3. 替换示例与测试中的构造参数。
4. 删除未被引用的 Billboard 专有 shader 资源。
5. 编译并运行回归。

---

## 6. 验收标准

1. 编译通过：ShaderGen、SceneGraph、相关示例目标。
2. Billboard 场景显示正确：
   - 面向摄像机模式正常。
   - 轴锁定模式正常。
3. 材质创建链路无“找不到 Billboard preset”错误。
4. 性能无明显退化（至少无数量级变化）。

---

## 7. 风险与防错

1. 风险：示例仍使用旧 preset，导致运行期枚举越界或默认分支错误。
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