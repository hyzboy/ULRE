# Phase 0R 技术文档：VertexPolicy 解耦（Vertex Source 统一入口）

## 1. 阶段目标

将现有 Billboard/Quad 的专用处理重构为“Vertex Source 统一入口 + 后置独立策略处理”，并在不破坏现有示例的前提下完成桥接接入：

1. VertexSourcePolicy（顶点来源）：`3D VBO` / `2D VBO->3D` / `PCG`。
2. GeometryLiftPolicy（几何升维）：负责 `2D->3D` 的升维规则。
3. OrientationPolicy（朝向）：负责“旋转扭正/面向摄像机”。
4. SizePolicy（尺寸）：负责世界尺寸/像素尺寸/混合尺寸策略。

本阶段只做“可组合策略模型 + 运行时桥接”，不直接删除旧路径。

---

## 2. 设计原则

1. 表面语义与变换语义分离：surface 只描述着色，不描述几何来源与朝向。
2. 输入先统一：所有输入先进入 Local3D，再进入姿态/尺寸处理。
3. 策略可组合：输入来源、升维、朝向、尺寸可独立组合。
4. 兼容优先：旧 Billboard/Quad 入口先映射到新策略，不一次性硬删。
5. 可观测：日志中必须看到四类策略轴的最终值。

---

## 3. 策略模型（建议）

### 3.1 VertexSourcePolicy（顶点来源）

1. `VBO3DDirect`
   - 3D 顶点流直接进入 Local3D。
2. `VBO2DTo3D`
   - 2D 顶点流先按升维规则转换为 Local3D。
3. `PCG`
   - 程序化生成顶点（全屏三角/矩形、地形 2D 网格+高度、贴地投影等）。

### 3.2 GeometryLiftPolicy（几何升维）

1. `None`
   - 输入已是 Local3D，或 PCG 直接产出 Local3D。
2. `XY_To_XY0`
   - 2D `(x,y)` -> 3D `(x,y,0)`。
3. `XY_To_X0Y`
   - 2D `(x,y)` -> 3D `(x,0,y)`。
4. `Screen2DToWorld`（可选）
   - 屏幕空间 2D 映射到世界空间约定平面。

### 3.3 OrientationPolicy（朝向/扭正）

1. `None`
2. `FaceCameraFull`
3. `FaceCameraAxisLocked`

### 3.4 SizePolicy（尺寸）

1. `WorldScale`
2. `PixelFixed`
3. `PixelFixedTimesWorldScale`

### 3.5 语义边界

1. SurfaceShadingModel 只承担表面语义（纹理、颜色、文字等）。
2. Billboard 不再是材质类别，而是策略组合（常见为 `VBO2DTo3D + FaceCamera* + PixelFixed*`）。

---

## 4. 分层处理链（强约束）

统一处理链固定为：

1. Vertex Source Stage：获取原始顶点（3D VBO / 2D VBO / PCG）。
2. Local Basis Stage：执行 GeometryLiftPolicy，产出 Local3D。
3. Pose Stage：执行 OrientationPolicy（与来源无关）。
4. Size Stage：执行 SizePolicy（与来源无关）。

在 Stage2 之后，不允许再通过“2D/3D 来源分支”修改姿态或尺寸逻辑。

---

## 5. 代码接入点（桥接）

1. recipe 组装层
   - `QuadResourcePrepareSystem`
   - `QuadMaterialBindingSystem`
   - 将旧 Billboard/Quad 输入映射为新策略轴。

2. runtime 入口
   - `ShaderMaterialProgramManager`
   - 不再用 Billboard/Quad 特判主导路由，仅做兼容映射。

3. ShaderGen 路由
   - `RecipeToKey`
   - `VariantRegistry / RegistryQuery`
   - 使 VS 路由按 `VertexSourcePolicy + GeometryLiftPolicy + OrientationPolicy + SizePolicy` 决策，FS 仅按 surface 语义。

4. 输入契约与输出契约
   - Stage2 输出必须满足 Local3D 顶点契约（position 必备，法线/切线按需求提供或生成）。
   - VS/FS varying 契约由策略组合自动导出，禁止分散特判。

---

## 6. 执行步骤

1. 定义四类策略枚举与默认值，保留旧字段兼容映射。
2. 在 recipe 生成处统一输出四类策略。
3. 在 ShaderGen 路由中增加策略日志与匹配分支。
4. 保持旧 Billboard/Quad 入口可运行，验证桥接正确。
5. 完成回归后进入 Phase0 做专用路径退场。

### 6.1 日志埋点最小清单（函数级）

以下埋点为本阶段最低要求，缺一不可：

1. recipe 组装函数（如 Quad 绑定/准备系统）
   - 输出四轴最终值与来源（默认值、旧字段映射、显式配置）。
   - 约定日志前缀：`VT-OK-*` 或 `VT-ERR-*`（组合校验结果）。

2. 策略校验函数（组合合法性检查）
   - 输出真值表 ID（如 `VT-001` / `VT-101`）与判定结论。
   - 非法组合必须带出缺失或冲突键名（如 lift 缺失、camera 缺失）。

3. ShaderGen 路由入口函数（如 RecipeToKey 入口）
   - 输出策略到 VS 选择分支的映射结果。
   - 当策略组合有效但资源不满足时，必须输出 `VT-ERR-*` 与 fallback 去向。

4. Matcher/候选评估函数（若本阶段已接入）
   - 输出候选枚举与失败原因。
   - 约定日志前缀：`MT-MATCH-*`（筛选/降级）、`MT-FALLBACK-*`（最终退化）。

5. 最终创建链路函数（ProgramManager/MaterialLibrary 桥接点）
   - 输出最终采用路径来源（命中候选、fallback、旧路兼容）。
   - 严禁无日志的静默替换。

日志合规规则：

1. 单次决策链至少出现 1 条阶段级 `VT-*` 或 `MT-*` 日志。
2. 命中 Phase0R 组合验证时，`VT-*` 必须与 7.1 真值表语义一一对应。
3. 失败日志必须同时包含：preset、phase、quality、候选标识、缺失能力键名。

---

## 7. 组合规则（首版）

1. `VBO3DDirect` 默认搭配 `GeometryLiftPolicy=None`。
2. `VBO2DTo3D` 必须显式指定升维策略（`XY_To_XY0` 或 `XY_To_X0Y`）。
3. `PCG` 可直接产出 Local3D，也可产出 2D 后再升维。
4. OrientationPolicy 不依赖来源类型，仅依赖 Local3D 与相机/轴约束。
5. SizePolicy 不依赖来源类型，仅依赖当前姿态结果与尺寸参数来源。

非法组合必须在路由层输出明确诊断并进入受控 fallback。

### 7.1 组合真值表（V1）

| ID | VertexSource | GeometryLift | Orientation | Size | Valid | RequiredResources | ShaderContractNotes | Fallback | LogCode |
|---|---|---|---|---|---|---|---|---|---|
| VT-001 | `VBO3DDirect` | `None` | `None` | `WorldScale` | Yes | Transform | 标准 3D VS/FS 契约 | - | `VT_OK_001` |
| VT-002 | `VBO3DDirect` | `None` | `FaceCameraAxisLocked` | `WorldScale` | Yes | Transform, Camera | 需相机朝向向量，varying 不增量 | - | `VT_OK_002` |
| VT-003 | `VBO2DTo3D` | `XY_To_XY0` | `None` | `WorldScale` | Yes | Transform | Stage2 后必须满足 Local3D | - | `VT_OK_003` |
| VT-004 | `VBO2DTo3D` | `XY_To_X0Y` | `None` | `WorldScale` | Yes | Transform | 同上，平面基底不同 | - | `VT_OK_004` |
| VT-005 | `VBO2DTo3D` | `XY_To_XY0` | `FaceCameraFull` | `PixelFixed` | Yes | Transform, Camera, Viewport | 需要像素尺寸换算；VS/FS varying 必须对齐 | - | `VT_OK_005` |
| VT-006 | `VBO2DTo3D` | `XY_To_X0Y` | `FaceCameraAxisLocked` | `PixelFixedTimesWorldScale` | Yes | Transform, Camera, Viewport | 支持 billboard-like 且保留世界缩放倍率 | - | `VT_OK_006` |
| VT-007 | `PCG` | `None` | `None` | `WorldScale` | Yes | (按 PCG 类型) | 全屏三角/程序网格可无 VBO 输入 | - | `VT_OK_007` |
| VT-008 | `PCG` | `Screen2DToWorld` | `None` | `WorldScale` | Yes | Camera, Viewport | 需屏幕到世界映射输入 | - | `VT_OK_008` |
| VT-009 | `PCG` | `Screen2DToWorld` | `FaceCameraAxisLocked` | `PixelFixed` | Yes | Camera, Viewport | 常用于贴地符号类投影 | - | `VT_OK_009` |
| VT-101 | `VBO2DTo3D` | `None` | `Any` | `Any` | No | - | 2D 来源未定义升维规则 | `RejectWithError` | `VT_ERR_NO_LIFT_FOR_2D` |
| VT-102 | `VBO3DDirect` | `XY_To_XY0` or `XY_To_X0Y` | `Any` | `Any` | No | - | 3D 直通不允许再应用 2D 升维 | `RejectWithError` | `VT_ERR_REDUNDANT_LIFT_FOR_3D` |
| VT-103 | `Any` | `Any` | `FaceCamera*` | `Any` | No* | Camera 缺失 | 朝向策略缺少 Camera 资源 | `CheckerboardFallback` | `VT_ERR_MISSING_CAMERA` |
| VT-104 | `Any` | `Any` | `Any` | `PixelFixed*` | No* | Viewport 缺失 | 像素尺寸策略缺少 Viewport 资源 | `CheckerboardFallback` | `VT_ERR_MISSING_VIEWPORT` |

注：`No*` 表示策略组合本身可合法，但在当前资源约束下不可执行。

### 7.2 表驱动测试要求

1. 每个 `VT-OK-*` 至少有 1 条正向冒烟用例。
2. 每个 `VT-ERR-*` 至少有 1 条负向用例，验证日志码与 fallback 行为。
3. 对 `PixelFixed*` 与 `FaceCamera*` 组合增加 VS/FS 契约检查（varying 与 descriptor set/binding 对齐）。

---

## 8. 验收标准

1. 同一套路由可同时驱动：矩形、圆形、文字、任意 2D 模型。
2. “旋转扭正”与“尺寸策略”独立于 2D/3D 输入来源。
3. 关键示例可运行，无新增黑屏、错材质或 descriptor/varying 契约错误。
4. 日志可审计：能看到 VertexSourcePolicy、GeometryLiftPolicy、OrientationPolicy、SizePolicy 的输入、命中与 fallback 原因。

---

## 9. 与后续阶段关系

1. Phase0（Billboard/Quad 退场）依赖本阶段完成。
2. Phase1~Phase3 可直接复用新策略轴，减少 matcher 维度耦合。
3. Phase4（Key/Geometry 清理）将更容易移除 GeometryMode 的运行时关键地位。
