# MaterialRecipe TOML 资产规范

## 用途与边界

`*.recipe.toml` 描述某个材质实例的静态声明：目标材质领域（`domain`）、可选顶点节点配置、渲染状态覆盖和纹理资源 URI。领域契约（`MaterialDomainContract`：能力、纹理槽、默认渲染状态、入口文件）是 C++ 内建数据，没有中间的定义/MI 文件层——`domain` 直接连到契约，解析结果即 `ResolvedMaterial`。

### 三层视角（先分清再看别的）

| 层 | 类型 | 位置 | 职责 |
|---|---|---|---|
| 规约（作者格式） | `MaterialRecipeSpec` | `inc/hgl/graph/asset/MaterialRecipeSpec.h` | `HGL_ASSET_STRUCT` 注解结构，字段清单由 Clang AST 提取；TOML/FBS/Meta 三套派生物**全部由 AssetGen 生成** |
| 句柄（身份） | `MaterialRecipeAsset` | `inc/hgl/graph/asset/MaterialRecipeAsset.h` | 继承 `hgl::asset::AssetObject`；同时给出规约、运行时结果、来源与状态 |
| 运行时（可渲染） | `MaterialRecipe` | `inc/hgl/mtl/MaterialRecipe.h` | 领域契约校验 + `NormalizeRecipe` 后的结果，带 TextureAsset 句柄 |

**改字段只改 `MaterialRecipeSpec.h` + 跑生成器**。不要手写 TOML 字段抽取，也不要手改
`inc/hgl/asset/generated/` 下任何文件（`run-assetgen.ps1 -Check` 会把差异当门禁失败）。

### 解析与加载

`MaterialRecipeFile.cpp`（`ULRE.ShaderMaterialData`）只做三件事：结构校验（`schema`/`name`/`domain` 存在）、严格键校验（按生成的 `ClassMetaInfo` 递归匹配 `toml_key`，未命中即 `UnknownKey`）、把 `MaterialRecipeSpec` 装配成 `MaterialRecipe`（含枚举专用诊断措辞）。字段抽取本身由生成 codec 完成，失败路径由 `Archive::FailedPath()` 当场拼出（`render_state.pipeline.cull_mode`），因此诊断永远指对表。

依赖 `TextureManager` 的 `MaterialRecipeRegistry` 位于 `ULRE.SceneGraph`，表项是 `MaterialRecipeAsset`：解析 → 按名查重 → 入库 → `SetSource` → `ResolveAndValidate`（领域契约、纹理槽、`NormalizeRecipe`）→ `MarkReady`。任一步失败即整条不入库，因此 `GetCount()` 在失败路径上不会瞬态跳变。

`MaterialRecipeAsset` 的状态机只有三态 `Parsed / Ready / Failed`——配方本体没有 IO 等待，编造加载态没有意义。异步轴落在纹理句柄上，由 `AreTexturesResident()` / `GetPendingTextureCount()` 聚合，未来接统一流式加载时它就是门禁条件。

应用级 `GraphicsContext` 持有 Registry，recipe 和纹理句柄随图形上下文共同管理。`FindMaterialRecipe` 取运行时结果，`FindMaterialRecipeAsset` 取句柄。首次未命中时会扫描默认目录 `ShaderLibrary/asset/recipe/`。自定义目录或单文件则显式调用 `LoadMaterialRecipeDirectory` / `LoadMaterialRecipeFile`。两者返回的指针都是观察指针，不能越过所属 GraphicsContext 的生命周期使用。


## Schema 1

```toml
schema = 1
name = "TextureQuad.UnlitTexture"
domain = "UnlitTexture"

[transform]
source = "Vec2Position"
mapping = "LiftXY_XY0"
orientation = "World"
scale = "World"
projection = "OrthoViewport"

[render_state]
alpha_test = false
alpha_cutoff = 0.5
dither = false

# 只写与契约/结构体默认不同的键
[render_state.pipeline]
cull_mode = "None"
depth_test = false
depth_write = false
depth_compare_op = "Always"
overlay = true

[fragment]
surface_module_name = "surface_sky_cube"

[[textures]]
name = "base_color"
asset = "res/image/lena.Tex2D"
required = true
layer = 0
```

### 字段规则

- `schema`, `name`, `domain` 是必填项。只接受 schema 1；未知键会显式报错。`domain` 必须是 `MaterialDomainContract` 里存在的领域名。
- `[transform]` 中所有字段可省略，未写字段保留 `MaterialRecipe` 的现有默认值。枚举名以 `VertexShaderNodeConfig.h` 中的 `Parse*Mode` / `Get*ModeName` 为准 —— 这些枚举已改为**转发到引擎真源**，因此拼写**大小写敏感**（`"OrthoViewport"` 有效，`"orthoviewport"` 无效）。Vulkan 系枚举同理（`"None"` 有效，`"VK_CULL_MODE_NONE"` 无效）。
- `[render_state]` 的 `alpha_test`、`alpha_cutoff`、`dither` 可分别覆盖；cutoff 必须在 `[0,1]`。
- `[render_state.pipeline]` 是**局部覆盖**：每个键都可省略，只写与默认不同的项；未写的字段保持领域契约的默认渲染状态。哪些键被显式写出由 `pipeline_config_mask` 逐字段记录，因此 `depth_test = false`、`line_width = 1.0` 这类「等于默认值」的显式写法也照常生效。整块与默认值完全一致时直接省略该表。
- `[fragment]` 只接受三个键：`surface_module_name`（仅 `no_material_source` 领域的配方可写，取值须是真实存在的表面模块名）、`samplers`（采样器名，发射为 `#define <name>Sampler <idx>u`）、`defines`（全局宏）。两个数组都是整表替换语义，写 `[]` 表示清空而不是「不覆盖」。
- 每个 `[[textures]]` 必须有非空 `name` 与 `asset`。`asset` 可为文件路径或 `rt:<target>[:<attachment>]`；`required` 默认为 `true`，`layer` 默认为 `0`。层号只能用于数组纹理槽。
- 纹理槽必须存在于引用的领域契约；名称重复、未知槽和不合法数组层会在注册时失败。

## 加载与运行时覆盖

```cpp
const mtl::MaterialRecipe *recipe = GetRecipe("TextureQuad.UnlitTexture");
if (!recipe)
    return false;
```

静态文件 URI 绑定通常无需 C++ 处理。ECS 渲染实体可以用 `RenderableBuilder` 一次配置变换、PrimitiveAsset 和运行时材质资源：

```cpp
Entity *entity = RenderableBuilder(world, "Quad")
    .WithTransform(position, rotation, scale, Mobility::Static)
    .WithPrimitiveAsset(&primitive_asset)
    .Build();
```

`PrimitiveAsset` 仍由调用方持有，必须比实体组件长寿。builder 失败时会销毁它创建的实体；成功 `Build()` 后实体归 ECS 世界管理。需要运行时覆盖的配方/纹理仍经 `MaterialData` 授权，不改变原有解析语义。

材质行采用强类型 `GlobalSSBODataAccessor`。`MaterialData::SetMaterialDataRow(accessor, value)` 将写行与绑定整合为一次调用，但 accessor 是 RAII 行租约，调用方必须持有到实体不再使用该行；不能传临时 accessor。多实体共享一行时，继续复用同一 accessor/binding。

`GlobalSSBOBinding` 中的 SSBO ID 与行号由运行时池分配，不是稳定资产数据，因此不能写入 TOML。当前 TOML schema 不接受 `[material_data]`：未来若要数据化初值，仍须在 schema 中显式映射强类型字段并接入对应行初始化消费者，不能只解析成无消费者的键值表。

## 示例资产

- `ShaderLibrary/asset/recipe/TextureQuad.recipe.toml`：静态文件纹理和 2D 管线局部覆盖。
- `ShaderLibrary/asset/recipe/PBRSpheres.recipe.toml`：静态材质/变换声明（渲染状态全默认，因此没有 `[render_state.pipeline]` 表）；数组纹理与 layer 由程序生成并在运行时覆盖。
- `ShaderLibrary/asset/recipe/Examples/`：示例程序使用的静态 recipe 声明，按示例类别分目录；纹理句柄、SSBO 绑定和数组层等运行时资源仍在 C++ 中接线。
- `MaterialErrorFallback` 与 `CoverageVariants` 的压力测试配方保留少量运行时生成：前者需要构造无效/未知输入以验证回退行为，后者阈值及配方数量由压力参数动态决定。

## 验证

`TestMaterialRecipeFile` 覆盖 schema 解析、枚举、渲染状态、pipeline 字段级局部覆盖（含显式 `false` 生效）、纹理字段、未知键/无效枚举诊断、**嵌套表失败的完整路径诊断**（`[render_state.pipeline]` 内枚举写错 ⇒ `table=render_state.pipeline`、`key=cull_mode`）、配方归一化、重复配方名拒绝，以及句柄层的状态/身份/来源反查/反射描述/就绪度聚合。

`TestMaterialRecipeCodec` 覆盖生成 codec 的往返与三态语义。生成器自身的一致性门禁是
`pwsh -NoProfile -File scripts/run-assetgen.ps1 -Check`（C++ 结构改动未重生成会失败）
与 `python src/Tools/AssetGen/test_asset_gen.py`。

