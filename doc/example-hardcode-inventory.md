# 示例硬编码可迁移清单（纯调研，不含任何改动）

本文是一次**只收集不处理**的盘点：把 `example/` 下仍留在 C++ 里的"数据"逐类量化，
并标注每一类**是否已有现成通道**（`mtldata` / 配方 `parent` / `geo:` / ShaderLibrary / `.scene`）。
目的是在动手前先看清"到底还有多少能搬"、"搬到哪个通道"、"哪些不值得搬"。

> 统计口径：`example/` 下 **52 个活跃 `.cpp` / 16,277 行**，不含 `*_Obsolete`（3 个文件）。
> 涉及废弃文件时会单独标注 `[废弃]`，不计入合计。

> **状态（2026-11）**：**第 1 类（材质 SSBO 数据行）已迁移完毕**。28 个示例块文件
> 落在 `ShaderLibrary/asset/material_data/`，示例里不再出现任何逐行 `Write`；
> 门禁见 `src/ecs/tests/TestExampleMaterialDataBlocks.cpp`（台账：块名 / 行类型 /
> 行数 / 抽样数值）。第 2~6 类仍按本文盘点原样待办。

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
| `ApiValidation/ComputeTransformHierarchy.cpp` | **74 行** | ✅ 已迁移 → `ShaderLibrary/compute/compute_transform_hierarchy.glsl` |
| `ApiValidation/ComputeIndirectCount.cpp` | **25 行** | ✅ 已迁移 → `ShaderLibrary/compute/compute_indirect_count.glsl` |
| `ApiValidation/ComputeDispatch.cpp` | **23 行** | ✅ 已迁移 → `ShaderLibrary/compute/compute_dispatch.glsl` |
| `ApiValidation/_Obsolete/ComputeAsteroidBelt.cpp` | 129 + 34 行 | `[废弃]`，跳过 |

活跃 **122 行已全部文件化**，`example/` 下非废弃代码中不再有 `R"(...)"` GLSL 字面量。

### 通道与难度

- **通道**：`ShaderLibrary/compute/*.glsl` + `mtl::LoadShaderLibrarySourceText()`
  （`inc/hgl/mtl/ShaderLibrarySource.h`，按 `GetShaderLibraryPath()` 解析根）
- **工期**：★★☆☆☆ —— compute 管线本来就有「直接给源码」这条路径，只是示例图省事写成字符串
- **优先级**：**P2 ✅ 完成**

### 落地要点（后续新增 compute 示例照此办理）

- 走 `CreateComputePipeline(name, glsl, pc_size)`：GLSL 从文件读入，仍不经 ShaderGen 生成器，
  因此 `push_constant_size` 必须继续和 C++ 结构用 `static_assert` 对拍。
- **`.glsl` 文件必须以 `#version` 开头**：glslang 不接受 `#version` 之前出现注释，
  文件级说明只能放在 `#version` 之后或写进 C++ 侧注释。
- `GlslModuleRegistry` 扫的是整个 `ShaderLibrary` 根，无模块指令的文件返回 `Skipped`（不报错），
  所以把纯 compute 源放在 `ShaderLibrary/compute/` 是安全的。

---

## 4. 内建几何 C++ 直建残留

**✅ 已清零**。上一轮 URI 化后列出的 9 条，逐条核实后实际只有 1 处是「真在用」：

| 文件 | 原行号 | 类型 | 处理 |
|---|---|---|---|
| `Interaction/GizmoUsage.cpp` | 99 | `PlaneGridCreateInfo` | **死代码**（几何走 `geo:builtin:plane_grid_lod`）→ 删 |
| `Interaction/GizmoUsage.cpp` | 125 | `CubeCreateInfo` | **死代码**（走 `geo:builtin:cube_2x2x2`）→ 删 |
| `Debug/PlaneGrid3D.cpp` | 94 | `PlaneGridCreateInfo` | **死代码**（走 `geo:builtin:plane_grid_lod`）→ 删 |
| `Debug/SimplestAxis.cpp` | 67 | `AxisCreateInfo` | **真在用** → 改 `CreateGeometryPreset(pc,"axis")` |
| `Debug/RenderBoundBox.cpp` | 298 | `RoundedBoxCreateInfo` | 早前已注释 → 保持注释 |
| `Offscreen/RenderToTexture.cpp` | 427 | `CubeCreateInfo` | **死代码**（走 `geo:builtin:cube`）→ 删 |
| `Offscreen/RenderToTextureColorDepth.cpp` | 490 | `CubeCreateInfo` | **死代码**（走 `geo:builtin:cube`）→ 删 |
| `Geometry/LoadGeometry/LoadGeometry.cpp` | 182 | `BoundingBoxCreateInfo` | **死代码**（走 `geo:builtin:bounding_box`）→ 删 |
| `Environment/AtmosphereSkyMinimal.cpp` | 69 | `HexSphereCreateInfo` | **死代码**（走 `geo:builtin:hex_sphere_sky`）→ 删 |

所谓「传了非默认参数才没用 URI」是个误判：这些示例**早就**改走 `GetGeometry(geo:builtin:...)`，
只是旧的 `*CreateInfo` 声明+赋参没删干净，参数全是死数据（且与对应 `.geo.toml` 的值一致）。
所以这一类的改造不是「补预设文件」，而是**删死代码**。

唯一需要真改造的 `SimplestAxis` 用的是共享 `GeometryCreater`（几何要自己 `GeometryManager::Add`），
`CreateGeometryPreset(creater, name)` 正是为这种场合准备的：参数进文件，GPU 资源归属不变。

### 通道

- 独立资产（默认）：`GetGeometry("geo:builtin:<preset>")` —— 注册表持有几何
- 共享 creater（示例批次）：`CreateGeometryPreset(creater, "<preset>")` —— **调用方**持有几何
- 预设文件：`res/geo/presets/<name>.geo.toml`（33 个）

### 优先级：**P3 ✅ 完成**

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

### 结论：**P4 判定不迁移**（类型已清点完毕，不做改造）

原计划"只对布局表型的 3~4 个示例试点"，实际逐个核实后发现**根本没有可迁移的布局表**：

全 `example/`（排除 `_Obsolete`）只有两处 `const glm::vec3 name[N] = {...}`：

| 文件 | 行 | 内容 | 性质 |
|---|---|---|---|
| `Transform/RecursiveCube.cpp` | 227 | `dirs[6]` 六轴向 | **生成参数**：喂给 `SpawnChain` 递归生成 `kMaxDepth` 层子立方（每向一条链），文件里要展开成数十个节点才等价 |
| `Shadow/CascadeShadowMap.cpp` | 1064 | `alpha_positions[4]` | **A/B 对比用的四个观察位**：运行时切换视角看 alpha 差异，不是场景内容 |

其余 216 处 `glm::vec3(...)` / `Color4f(...)` 全部是**逐对象程序化摆位**，形态是
`for (i < COUNT) { pos = f(i); ... }`（`PBRSpheres` 10×10、`GPUDrivenCull` `CUBE_COUNT`、
`CoverageVariants` `GRID_ROWS×GRID_COLS`、`AutoMergeMaterialInstance` `DRAW_OBJECT_COUNT`）。
把 `f(i)` 换成文件，等价于把**循环**换成**上千行节点表**：

- 文件体积从几十行膨胀到数千行，可读性与 diff 质量都变差；
- 循环里的缩放/旋转/材质行索引同样是 `f(i)`，要一并展开才等价，等于把程序变成数据快照；
- 这些示例的**教学价值恰恰是那段循环**（演示 instancing / GPU-driven / 多批次），抽走就失去意义；
- `.scene` 当前只有显式 `[[nodes]]`，没有"阵列/重复"表达；要真文件化得先造 Generative Spec，
  而收益（~200 行 C++）远小于成本与风险。

**因此这一类保持原样。** 真正的数据（几何、材质配方、SSBO 数据行、纹理、GLSL）已全部文件化，
剩下的"摆位"是**示例自身的程序逻辑**，属于该留在代码里的部分。

### 若将来要动（前置条件）

先给 `.scene` 增加生成式节点表达（如 `[[instances]] { geometry, recipe, count, rows, cols, spacing }`），
并让 `SceneLoader` 支持展开；届时 `PBRSpheres` / `CoverageVariants` 这类网格阵列才能真正受益。
这属于**新功能**，不是"消除硬编码"，应单独立项。

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

| # | 类别 | 规模 | 通道 | 难度 | 状态 |
|---|---|---|---|---|---|
| 1 | 材质 SSBO 数据行 | **380 行样板 / 14 函数 + 19 文件内联** | `mtldata` + `WithMaterialDataRow` | ★★☆☆☆ | ✅ **P0 完成** |
| 2 | 配方 C++ 拼装/改写 | **49 处 / 18 文件** | 配方 `parent` + `tex:` | ★★★☆☆ | ✅ **P1 完成** |
| 3 | 内联 GLSL | **122 行 / 3 文件** | ShaderLibrary | ★★☆☆☆ | ✅ **P2 完成** |
| 4 | 内建几何直建残留 | **9 处 / 8 文件** | 预设 + `CreateGeometryPreset` | ★☆☆☆☆ | ✅ **P3 完成** |
| 5 | 布局/摆位/颜色常量 | **218 处 / 45 文件** | 无（程序化摆位，非数据） | — | ⛔ **P4 判定不迁移**（见 §5 结论） |
| 6 | 纹理/天空/光照参数 | 极少（~10 处） | 场景属性 | ★☆☆☆☆~★★☆☆☆ | ⛔ 随 P4 一并判定不迁移 |

### 粗估"可消除的 C++ 行数"

| 类别 | 可减少行数（估） | 实际 |
|---|---|---|
| §1 材质数据行 | **~360 行**（380 → ~20） | ✅ |
| §2 配方拼装 | **~120 行**（49 处 × 平均 2.5 行，含 `AddTexture` 参数行） | ✅ |
| §3 内联 GLSL | **~130 行**（含包裹代码） | ✅ |
| §4 几何残留 | **~40 行** | ✅ ~35 行 |
| §5 布局/常量 | **~200 行**（试点部分，非全部） | ⛔ 0（判定不迁移） |
| **合计** | **~650 行 / 16,277 行 ≈ 4.0%** | |

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
