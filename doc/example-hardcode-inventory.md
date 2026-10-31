# 示例硬编码可迁移清单（纯调研，不含任何改动）

本文是一次**只收集不处理**的盘点：把 `example/` 下仍留在 C++ 里的"数据"逐类量化，
并标注每一类**是否已有现成通道**（`mtldata` / 配方 `parent` / `geo:` / ShaderLibrary / `.scene`）。
目的是在动手前先看清"到底还有多少能搬"、"搬到哪个通道"、"哪些不值得搬"。

> 统计口径：`example/` 下 **52 个活跃 `.cpp` / 16,277 行**，不含 `*_Obsolete`（3 个文件）。
> 涉及废弃文件时会单独标注 `[废弃]`，不计入合计。

---

## 0. 基线

| 指标 | 数值 |
|---|---|
| 活跃 `.cpp` 文件 | 52 |
| 活跃总行数 | 16,277 |
| 构建 target | 55（含部分多文件 target） |
| 最大单文件 | `Shadow/CascadeShadowMap.cpp` 1447 行 / 80 KB |
| 次大 | `Shadow/AlphaTestShadow.cpp` 866 行、`Offscreen/RenderToTextureColorDepth.cpp` 658 行、`Offscreen/RenderToTexture.cpp` 622 行 |

### 已确认"完成"的通道（本清单的对照组）

| 通道 | 现状 |
|---|---|
| 内建几何 `geo:builtin:<name>` | **105 处 / 29 文件**，全部走 URI |
| 几何预设真源 | **已是文件**：`res/geo/presets/*.geo.toml`，共 **33 个** |
| 纹理 / 配方 URI | `tex:` / `rcp:` / `mtldata:` 覆盖 29 文件 |
| 材质数据行 | 仅 `PBRSpheres` 走 `mtldata:file:...` + `WithMaterialDataRow` |
| 场景文件 | `.scene` 已可加载（`Geometry/LoadScene` 演示） |

其中「几何预设文件化」这点要特别说明：`src/SceneGraph/asset/GeometryPreset.cpp` 里
`FindOrLoad()` 拼的是 `res/geo/presets/<name>.geo.toml`，**不是 C++ 表**。
所以"内建几何参数"这一类**已经搬完了**，无需再动。

---

## 1. 材质 SSBO 数据行（最高价值）

### 现状

| 指标 | 数值 |
|---|---|
| 声明 `ssbo::*Row` 变量的文件 | **31**（活跃 28） |
| `ssbo::*Row` 声明总数 | **83** |
| 已封装成函数的样板 | **14 个函数 / 380 行** |
| 内联直接写（无函数封装） | 另 14 文件 |

### 1.1 已封装成函数、可直接换 `mtldata` 的（7 行样板 → 1 行）

| 文件 | 函数 | 行范围 | 行数 |
|---|---|---|---|
| `Material/PBRSpheres.cpp` | `InitMaterialDataBlock` | 296-343 | 48（**已迁移**，留作对照） |
| `Material/CoverageVariants.cpp` | `CreateSurfaceRows` | 213-258 | 46 |
| `Offscreen/RenderToTexture.cpp` | `InitMaterialDataSSBO`(world) | 127-162 | 36 |
| `Environment/IBLEnvironment.cpp` | `CreateMeshMaterialRows` | 363-395 | 33 |
| `Material/AlphaModes.cpp` | `CreateSurfaceRows` | 228-255 | 28 |
| `Offscreen/RenderToTextureColorDepth.cpp` | `InitMaterialDataSSBO`(world) | 160-187 | 28 |
| `Material/MaterialErrorFallback.cpp` | `InitMaterialDataSSBO` | 232-257 | 26 |
| `Offscreen/RenderToTexture.cpp` | `InitMaterialDataSSBO`(cube) | 529-550 | 22 |
| `Environment/AtmosphereSkyAmbient.cpp` | `InitMaterialDataSSBO` | 102-122 | 21 |
| `Geometry/MixedBufferMeshes.cpp` | `InitMaterialDataSSBO` | 108-128 | 21 |
| `Material/FullPrecisionLitMeshes.cpp` | `InitMaterialDataSSBO` | 90-110 | 21 |
| `GettingStarted/SimpleSphere.cpp` | `InitMaterialDataSSBO` | 74-91 | 18 |
| `Material/MaterialRecipeEntry.cpp` | `InitMaterialDataSSBO` | 103-119 | 17 |
| `Offscreen/RenderToTextureColorDepth.cpp` | `InitMaterialDataSSBO`(cube) | 477-491 | 15 |

典型形态（`SimpleSphere.cpp` 74-91，18 行只为了写 1 行的数据）：

```cpp
bool InitMaterialDataSSBO()
{
    auto* domain_manager = GetManager<GlobalSSBOBufferRegistry>();
    if (!domain_manager) return false;
    material_data_ssbo_accessor = domain_manager->GetAccessor<graph::ssbo::PBRSurfaceRow>();
    if (!material_data_ssbo_accessor) return false;

    graph::ssbo::PBRSurfaceRow material_data{};
    material_data.base_color   = Color4f(1.0f);
    material_data.metallic     = 0.08f;
    material_data.roughness    = 0.92f;
    material_data.normal_scale = 0.35f;

    return material_data_ssbo_accessor.Write(material_data);
}
```

> 有 **6 个文件是逐字相同的样板**（`SimpleSphere`、`MixedBufferMeshes`、
> `FullPrecisionLitMeshes`、`MaterialRecipeEntry`、`AtmosphereSkyAmbient`、
> `MaterialErrorFallback`），只是 4 个数值不同。这 6 个是收益最直接的批量替换对象。

### 1.2 内联直接写行（没封装成函数）

| 文件 | 行号 | 说明 |
|---|---|---|
| `Shadow/CascadeShadowMap.cpp` | 917-936 | **循环生成 24 行**（`metallic=0.05+0.1*(i%5)`、`roughness=0.2+0.08*i`）+ 地面行 |
| `Material/CoverageVariants.cpp` | 213-258 | **双层循环 GRID_ROWS×GRID_COLS** + 压力测试行 |
| `Material/AlphaModes.cpp` | 228-255 | 按列生成 alpha 行 |
| `Environment/BasicLitSunDirection.cpp` | 160 | 单行 |
| `Material/SingleSphereMaterialSwitch.cpp` | 271 | 单行（4 行切换） |
| `Shadow/DualWorldShadows.cpp` | 223 | 单行 |
| `Shadow/AlphaTestShadow.cpp` | 529 | 单行 |
| `Transform/ClockUse.cpp` | — | 4 处 |
| `Interaction/GizmoUsage.cpp` | — | 4 处 |
| `Interaction/RayPicking.cpp` | — | 4 处 |
| `Debug/PlaneGrid3D.cpp` | — | 2 处 |
| `Debug/RenderBoundBox.cpp` | — | 2 处 |
| `Cull/GPUDrivenCull.cpp` | — | 2 处 |
| `ApiValidation/ComputeIndirectCount.cpp` | — | 2 处 |
| `Geometry/LoadGeometry/LoadGeometry.cpp` | — | 2 处 |
| `Geometry/LoadScene/LoadScene.cpp` | — | 2 处 |
| `GettingStarted/SimpleCube.cpp` | — | 2 处 |
| `Transform/RecursiveCube.cpp` | — | 2 处 |
| `Instancing/AutoMergeMaterialInstance.cpp` | — | 2 处 |

### 通道与难度

- **通道**：`mtldata:file:...`（`ShaderLibrary/asset/material_data/*.material_data.toml`）+ `RenderableBuilder::WithMaterialDataRow`
- **难度**：★★☆☆☆ —— 通道已通、`PBRSpheres` 已跑通，剩下是机械替换
- **量化收益**：14 个函数 380 行样板 → 可压到约 14~20 行；循环生成的 3 处需先落成数据块文件，代码里只留 `mtldata:` 引用
- **优先级**：**P0**（最高，通道最成熟）

---

## 2. 配方（`MaterialRecipe`）在 C++ 里拼装/改写

### 现状

| 指标 | 数值 |
|---|---|
| `GetRecipe(...)` 调用 | **62 处 / 43 文件** |
| `AddTexture(` 调用 | **35 处 / 17 文件**（活跃 16） |
| `render_state_overrides` 出现 | **5 处 / 2 文件** |
| 两类"就地改造"合计 | **49 处操作 / 18 文件** |

从 `GetRecipe("Xxx.Lit")` 拿到配方后，在 C++ 里继续 `AddTexture` 补纹理绑定 —— 这是
"配方文件已经存在，但绑纹理这一步还留在代码里"的半成品状态。

| 文件 | `AddTexture` | `render_state_overrides` |
|---|---|---|
| `Shadow/CascadeShadowMap.cpp` | 7 | 0 |
| `Material/CoverageVariants.cpp` | 6 | 4 |
| `Offscreen/RenderToTexture.cpp` | 6 | 0 |
| `Material/SingleSphereMaterialSwitch.cpp` | 4 | 0 |
| `Offscreen/RenderToTextureColorDepth.cpp` | 4 | 0 |
| `Shadow/AlphaTestShadow.cpp` | 3 | 0 |
| `Material/AlphaModes.cpp` | 2 | 0 |
| `Material/PBRSpheres.cpp` | 2 | 0 |
| `Texture/AsyncTextureUpload.cpp` | 2 | 0 |
| `Environment/IBLEnvironment.cpp` | 1 | 0 |
| `Environment/SkyCubeSphere.cpp` | 1 | 0 |
| `Shadow/DualWorldShadows.cpp` | 1 | 0 |
| `Texture/TextureArrayStreaming.cpp` / `TextureMipLevels.cpp` / `TextureRect.cpp` / `TextureRectArray.cpp` | 各 1 | 0 |
| `Material/MaterialErrorFallback.cpp` | 0 | 1 |

### 关键观察：`CoverageVariants` 是 `parent` 继承的正牌用例

`example/Material/CoverageVariants.cpp` L180-210 在循环里：

```cpp
MaterialRecipe r = opaque_recipe;      // C++ 本地复制
r.alpha_cutoff = STRESS_CUTOFF_STEP * (i + 1);
```

这正好是配方继承（`parent = "rcp:@..."` + 只改 `alpha_cutoff`）要取代的场景，
目前停在"代码里复制结构体再改字段"的临时做法上。

### 通道与难度

- **通道**：配方 `parent` 继承（`ShaderLibrary/material/**/*.material.toml` 里 `parent = "rcp:@Lit"`）、`tex:` URI 直接写进配方
- **难度**：★★★☆☆ —— 需要先把示例里的纹理选择逻辑映射成 `tex:` URI（大部分已经是）
- **优先级**：**P1**（`parent` 通道刚落地，正好用它验证）

---

## 3. 内联 GLSL 源码

| 文件 | 行数与位置 | 状态 |
|---|---|---|
| `ApiValidation/ComputeTransformHierarchy.cpp` | **74 行** | 活跃 |
| `ApiValidation/ComputeIndirectCount.cpp` | **25 行** | 活跃 |
| `ApiValidation/ComputeDispatch.cpp` | **23 行** | 活跃 |
| `ApiValidation/_Obsolete/ComputeAsteroidBelt.cpp` | 129 + 34 行 | `[废弃]` |

活跃合计 **122 行**内联 GLSL（`R"(...)"` 原始字符串）。

### 通道与难度

- **通道**：ShaderLibrary（`.glsl` / `GlslModule`），与材质 shader 同一套加载路径
- **难度**：★★☆☆☆ —— compute 管线已有 shader 加载能力，只是示例图省事写成字符串
- **优先级**：**P2**（数量少但收益明确：改 shader 不用重编译示例）

---

## 4. 内建几何 C++ 直建残留

上一轮 URI 化后只剩 **8 处（+1 处注释）**：

| 文件 | 行号 | 类型 |
|---|---|---|
| `Interaction/GizmoUsage.cpp` | 93 | `inline_geometry::PlaneGridCreateInfo` |
| `Interaction/GizmoUsage.cpp` | 134 | `inline_geometry::CubeCreateInfo` |
| `Debug/PlaneGrid3D.cpp` | 88 | `PlaneGridCreateInfo` |
| `Debug/SimplestAxis.cpp` | 67 | `inline_geometry::AxisCreateInfo` |
| `Debug/RenderBoundBox.cpp` | 298 | `RoundedBoxCreateInfo`（**已注释掉**） |
| `Offscreen/RenderToTexture.cpp` | 483 | `inline_geometry::CubeCreateInfo` |
| `Offscreen/RenderToTextureColorDepth.cpp` | 542 | `inline_geometry::CubeCreateInfo` |
| `Geometry/LoadGeometry/LoadGeometry.cpp` | 201 | `inline_geometry::BoundingBoxCreateInfo` |
| `Environment/AtmosphereSkyMinimal.cpp` | 69 | `HexSphereCreateInfo` |

注意 `CubeCreateInfo` / `PlaneGridCreateInfo` / `AxisCreateInfo` / `BoundingBoxCreateInfo` /
`HexSphereCreateInfo` **都已经有对应预设文件**（`cube.geo.toml`、`axis.geo.toml`、
`bounding_box.geo.toml` 等 33 个之一），只是这些示例传了**非默认参数**才没用 URI。

### 通道与难度

- **通道**：`res/geo/presets/<新名>.geo.toml`（新增预设文件，不动代码逻辑）
- **难度**：★☆☆☆☆ —— 纯新增 TOML 文件 + 改一行 URI
- **优先级**：**P3**（量很少，主要是"消除最后几处例外"的一致性收益）

---

## 5. 场景布局 / 硬编码摆位与颜色常量

### 现状

| 指标 | 数值 |
|---|---|
| `glm::vec3(<数字>...)` 字面量 | 约 **150 处** |
| `Color4f(<数字>/COLOR::...)` 字面量 | 约 **68 处** |
| 合计 | **243 处 / 46 文件**（活跃 218 处 / 45 文件） |

Top 文件（活跃）：

| 文件 | vec3 | color |
|---|---|---|
| `ApiValidation/ComputeTransformHierarchy.cpp` | 11 | 0 |
| `Geometry/LineRender.cpp` | 11 | 0 |
| `Environment/IBLEnvironment.cpp` | 8 | 6 |
| `Environment/BasicLitSunDirection.cpp` | 8 | 2 |
| `Transform/RecursiveCube.cpp` | 8 | 2 |
| `Shadow/CascadeShadowMap.cpp` | 7 | 3 |
| `Offscreen/RenderToTexture.cpp` | 6 | 3 |
| `Geometry/MixedBufferMeshes.cpp` | 6 | 2 |
| `Material/FullPrecisionLitMeshes.cpp` | 6 | 2 |
| `Debug/RenderBoundBox.cpp` | 6 | 1 |
| `Environment/AtmosphereSkyAmbient.cpp` | 6 | 1 |
| `Transform/ClockUse.cpp` | 6 | 5 |

### 程序化摆放循环（结构化的"布局表"）

| 文件 | 行号 | 说明 |
|---|---|---|
| `Transform/RecursiveCube.cpp` | 233 | `const glm::vec3 dirs[6]` 六向布局表 |
| `Shadow/CascadeShadowMap.cpp` | 1180 | `const glm::vec3 alpha_positions[4]` |
| `Material/CoverageVariants.cpp` | — | GRID_ROWS × GRID_COLS 双层循环 |
| `Cull/GPUDrivenCull.cpp` | 140, 197 | `CUBE_COUNT` 循环 |
| `Instancing/AutoMergeMaterialInstance.cpp` | 149, 209 | `DRAW_OBJECT_COUNT` 循环 |
| `ApiValidation/ComputeDispatch.cpp` | 129, 161 | `DATA_COUNT` 循环 |
| `Material/PBRSpheres.cpp` | 453-455, 645-647 | 10×10 网格（数据行已迁移，摆位未迁） |

### 通道与难度

- **通道**：`.scene`（已可加载，`Geometry/LoadScene` 为样板）；配方文件承载颜色
- **难度**：★★★★☆ —— 需要先定义"节点摆位/实例数组"在 `.scene` 里的表达，属于**新 Spec 工作量**
- **优先级**：**P4**（价值高但成本最高；建议只对"布局表"型的 3~4 个示例试点，其余保留）

---

## 6. 纹理与天空/光照参数

| 子类 | 现状 | 通道 | 难度 |
|---|---|---|---|
| 纹理规格（`ColorTextureCreateInfo` 等） | 少量，如 `Texture/AsyncTextureUpload.cpp:358`（运行时生成，非静态数据） | 不适合文件化 | — |
| 天空/大气参数 | 参数主要在 shader/recipe 里；示例只硬编码 1 个 `sun_dir`（`AtmosphereSkyAmbient.cpp:248` 等 3 文件） | 配方/场景属性 | ★☆☆☆☆ |
| 光照方向 | 每示例 1 处 `glm::vec3` 常量 | 场景文件属性 | ★★☆☆☆ |

这一类**量极小**，不建议单独立项，随 §5 场景化一起收。

---

## 7. 汇总与建议优先级

| # | 类别 | 规模 | 通道 | 难度 | 优先级 |
|---|---|---|---|---|---|
| 1 | 材质 SSBO 数据行 | **380 行样板 / 14 函数 + 19 文件内联** | `mtldata` + `WithMaterialDataRow` | ★★☆☆☆ | **P0** |
| 2 | 配方 C++ 拼装/改写 | **49 处 / 18 文件** | 配方 `parent` + `tex:` | ★★★☆☆ | **P1** |
| 3 | 内联 GLSL | **122 行 / 3 文件** | ShaderLibrary | ★★☆☆☆ | **P2** |
| 4 | 内建几何直建残留 | **8 处 / 8 文件** | 新增 `*.geo.toml` 预设 | ★☆☆☆☆ | **P3** |
| 5 | 布局/摆位/颜色常量 | **218 处 / 45 文件** | `.scene`（需新 Spec） | ★★★★☆ | **P4** |
| 6 | 纹理/天空/光照参数 | 极少（~10 处） | 场景属性 | ★☆☆☆☆~★★☆☆☆ | 随 P4 |

### 粗估"可消除的 C++ 行数"

| 类别 | 可减少行数（估） |
|---|---|
| §1 材质数据行 | **~360 行**（380 → ~20） |
| §2 配方拼装 | **~120 行**（49 处 × 平均 2.5 行，含 `AddTexture` 参数行） |
| §3 内联 GLSL | **~130 行**（含包裹代码） |
| §4 几何残留 | **~40 行** |
| §5 布局/常量 | **~200 行**（试点部分，非全部） |
| **合计** | **~850 行 / 16,277 行 ≈ 5.2%** |

> 比例不高，但**结构性收益**远大于行数：P0+P1 做完后，"示例里怎么造材质数据"
> 就只剩一种写法，不会再出现第 15 个 `InitMaterialDataSSBO` 复制品。

---

## 8. 调研方法与复现命令

统计脚本落在会话临时目录，均为只读扫描：

```powershell
# 1) 枚举活跃示例文件与行数
Get-ChildItem -Recurse -Filter *.cpp | Where-Object { $_.FullName -notmatch '_Obsolete' }

# 2) 材质数据行：声明数 / Accessor 数
#    正则：ssbo::\w+Row  与  GetAccessor<
# 3) 材质数据函数体行数：按 { } 深度配对，匹配
#    bool|void \w*(Material|Surface|Data)\w*\(  ... {
# 4) 配方改造：\.AddTexture\( 与 render_state_overrides
# 5) 内联 GLSL：R"\((.*?)\)" 内的换行数（>5 才算）
# 6) 摆位常量：glm::vec3\(\s*-?[\d.]  与  Color4f\(\s*[\d.]
# 7) 几何残留：\w+CreateInfo
```

几何预设清单：

```powershell
Get-ChildItem res\geo\presets -Filter *.geo.toml   # 33 个
```

---

## 9. 未覆盖 / 需进一步取证

| 项 | 状态 |
|---|---|
| `Text/` 目录字体与文本效果配置是否可文件化 | **未统计** |
| `Environment/` 大气散射细参数（`turbidity` / `rayleigh` 等）真源在哪 | 初步看**已在 shader/recipe**，未逐一确认 |
| `.scene` 现有 Spec 是否支持"实例数组 + 每实例覆写" | **未确认** —— 决定 §5 的成本 |
| `ApiValidation/_Obsolete` 是否应从构建中移除 | 未处理（`ExtrudedPolygonTest` / `WallsFromPolyline` 仍在 target 列表） |

---

*本文仅为调研记录。所有数字来自对 `example/` 的只读 grep 扫描，未修改任何代码。*
