# MaterialRecipe TOML 资产规范

## 用途与边界

`*.recipe.toml` 描述某个材质实例的静态声明：MaterialDefinition ID、可选顶点节点配置、渲染状态覆盖和纹理资源 URI。它不替代 `*.mi.toml`：MI 文件定义材质能力、资源槽契约和领域；recipe 文件选择定义并提供具体绑定。

`MaterialRecipeFile` 的解析器位于 `ULRE.ShaderMaterialData`；依赖 `TextureManager` 的 `MaterialRecipeRegistry` 位于 `ULRE.SceneGraph`。Registry 通过 `LoadFile` / `LoadDirectory` 解析文件、校验定义引用及纹理槽、创建 TextureAsset 句柄并调用 `NormalizeRecipe`。纹理文件由 `TextureStreamingSystem` 延迟异步加载；`rt:` URI 由 RenderTarget 管理器解析。

应用级 `GraphicsContext` 持有 Registry，recipe 和纹理句柄随图形上下文共同管理。应用可通过 `WorkObject::GetRecipe` 访问；首次未命中时会扫描默认目录 `ShaderLibrary/material/recipes/`。自定义目录或单文件则显式调用 `LoadRecipeDirectory` / `LoadRecipeFile`。recipe 指针仍是观察指针，不能越过所属 GraphicsContext 的生命周期使用。

## Schema 1

```toml
schema = 1
name = "TextureQuad.UnlitTexture"
material_definition = "UnlitTexture"

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

[render_state.pipeline]
cull_mode = "None"
depth_test = false
depth_write = false
depth_compare_op = "Always"
alpha_blend = false
blend_src = "SrcAlpha"
blend_dst = "OneMinusSrcAlpha"
dynamic_line_width = false
overlay = true
wireframe = false
line_width = 1.0

[[textures]]
name = "base_color"
asset = "res/image/lena.Tex2D"
required = true
layer = 0
```

### 字段规则

- `schema`, `name`, `material_definition` 是必填项。只接受 schema 1；未知键会显式报错。
- `[transform]` 中所有字段可省略，未写字段保留 `MaterialRecipe` 的现有默认值。枚举名以 `VertexShaderNodeConfig.h` 中的 `Parse*Mode` / `Get*ModeName` 为准。
- `[render_state]` 的 `alpha_test`、`alpha_cutoff`、`dither` 可分别覆盖；cutoff 必须在 `[0,1]`。
- 如果声明 `[render_state.pipeline]`，必须填写完整的 pipeline 配置。当前 `MaterialRenderStateOverrides` 把 pipeline 作为一个整体覆盖值，没有逐字段 presence mask；要求完整值避免省略字段时意外覆盖材质定义默认值。
- 每个 `[[textures]]` 必须有非空 `name` 与 `asset`。`asset` 可为文件路径或 `rt:<target>[:<attachment>]`；`required` 默认为 `true`，`layer` 默认为 `0`。层号只能用于数组纹理槽。
- 纹理槽必须存在于引用的 MaterialDefinition；名称重复、未知槽和不合法数组层会在注册时失败。

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

- `ShaderLibrary/material/recipes/TextureQuad.recipe.toml`：静态文件纹理和 2D 管线覆盖。
- `ShaderLibrary/material/recipes/PBRSpheres.recipe.toml`：静态材质/变换/管线声明；数组纹理与 layer 由程序生成并在运行时覆盖。
- `ShaderLibrary/material/recipes/Examples/`：示例程序使用的静态 recipe 声明，按示例类别分目录；纹理句柄、SSBO 绑定和数组层等运行时资源仍在 C++ 中接线。
- `MaterialErrorFallback` 与 `CoverageVariants` 的压力测试配方保留少量运行时生成：前者需要构造无效/未知输入以验证回退行为，后者阈值及配方数量由压力参数动态决定。

## 验证

`TestMaterialRecipeFile` 覆盖 schema 解析、枚举、渲染状态、纹理字段、未知键/无效枚举诊断、不完整 pipeline 拒绝、定义归一化和重复配方名拒绝。
