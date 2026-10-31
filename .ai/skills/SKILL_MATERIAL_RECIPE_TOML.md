# MaterialRecipe TOML 资产规范

## 用途与边界

`*.recipe.toml` 描述某个材质实例的静态声明：目标材质领域（`domain`）、可选顶点节点配置、渲染状态覆盖和纹理资源 URI。领域契约（`MaterialDomainContract`：能力、纹理槽、默认渲染状态、入口文件）是 C++ 内建数据，没有中间的定义/MI 文件层——`domain` 直接连到契约，解析结果即 `ResolvedMaterial`。

`MaterialRecipeFile` 的解析器位于 `ULRE.ShaderMaterialData`；依赖 `TextureManager` 的 `MaterialRecipeRegistry` 位于 `ULRE.SceneGraph`。Registry 通过 `LoadFile` / `LoadDirectory` 解析文件、校验领域与纹理槽、创建 TextureAsset 句柄并调用 `NormalizeRecipe`。纹理文件由 `TextureStreamingSystem` 延迟异步加载；`rt:` URI 由 RenderTarget 管理器解析。

应用级 `GraphicsContext` 持有 Registry，recipe 和纹理句柄随图形上下文共同管理。应用可通过 `WorkObject::GetRecipe` 访问；首次未命中时会扫描默认目录 `ShaderLibrary/asset/recipe/`。自定义目录或单文件则显式调用 `LoadRecipeDirectory` / `LoadRecipeFile`。recipe 指针仍是观察指针，不能越过所属 GraphicsContext 的生命周期使用。

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
- `[transform]` 中所有字段可省略，未写字段保留 `MaterialRecipe` 的现有默认值。枚举名以 `VertexShaderNodeConfig.h` 中的 `Parse*Mode` / `Get*ModeName` 为准。
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

`TestMaterialRecipeFile` 覆盖 schema 解析、枚举、渲染状态、pipeline 字段级局部覆盖（含显式 `false` 生效）、纹理字段、未知键/无效枚举诊断、配方归一化和重复配方名拒绝。
