# 几何资产与图元资产 TOML 规范 / Geometry & Primitive Asset TOML

> 适用：把内置几何体参数化、把「几何 + 材质配方」装配成图元资产、以及让模型/场景
> 走异步流送而不卡住主线程。

## 1. 三层职责（不要混）

| 层 | 文件 | 只描述 | 绝不描述 |
|----|------|--------|---------|
| 几何 | `*.geo.toml` | 顶点布局、内置形状参数、外部 `.geo` 路径 | 材质 / 着色 / 变换 |
| 材质 | `*.recipe.toml` | 着色逻辑、管线状态、纹理槽 | 几何 / 位置 |
| 图元 | `*.primitive.toml` | 引用哪个几何、引用哪个配方、变体 | 几何参数本身 |

一个几何可以被 N 个图元共享；一个配方可以被 N 个图元共享。图元只是**绑定**。

## 2. `*.geo.toml`（schema = 1）

### 2.1 内置几何体（创建即 Resident，不读盘）

```toml
schema = 1
name = "unit_sphere"        # 可选；缺省用文件名
type = "builtin"
shape = "sphere"

[builtin.sphere]
slices = 64

[vertex_format]            # 可选；缺省 Position=V3F / TexCoord=V2HF / Normal=V2UN8
normal = "VF_V2UN8"        # v2un8 | v2hf | v3f | v3sn16（也接受 VF_ 宏名 / Vulkan 后缀名）
```

支持的 `shape` 与参数表（表名 = `[builtin.<shape>]`）：

| shape | 参数 |
|-------|------|
| `sphere` | `slices` |
| `dome` | `slices` |
| `cube` | `segments_x/y/z`、`ntb`、`tex_coord` |
| `plane_grid` | `grid_x/grid_y`、`sub_x/sub_y`、`lum`、`sub_lum`、`is_3d` |
| `plane_square` | 无 |
| `cylinder` | `half_extend`、`radius`、`slices` |
| `hollow_cylinder` | `half_extend`、`inner_radius`、`outer_radius`、`slices`、`cap_radial_tiles`、`cap_angular_tiles` |
| `torus` | `inner_radius`、`outer_radius`、`slices`、`stacks`、`uv_scale_x/y` |
| `cone` | `half_extend`、`radius`、`slices`、`stacks` |
| `capsule` | `half_height`、`radius`、`slices`、`stacks` |
| `rectangle` | `left`、`top`、`width`、`height`、`segments_x/y`、`ntb`、`tex_coord` |
| `bounding_box` | `ntb` |

`ntb` 取值：`none` / `normal` / `normal_tangent` / `normal_tangent_bitangent`。

### 2.2 外部模型几何（`MetaOnly` → 异步读盘 → `Resident`）

```toml
schema = 1
name = "vulkan_logo"
type = "file"
file = "model/vulkan_logo/scene.StaticMesh/scene.geometry"
```

> `file` 相对**该 TOML 所在目录**以外的调用方工作目录解析（`GeometryAssetSpec.file_path`
> 原样交给 `LoadedFileManager`），因此仓库内的资产统一按工程根相对路径书写。

## 3. `*.primitive.toml`（schema = 1）

```toml
schema = 1
name = "pbr_sphere"
geometry = "../geo/unit_sphere.geo.toml"   # 相对本文件目录解析
recipe = "PBRSpheres.Lit"                  # 材质配方名（MaterialRecipeRegistry 按名查）
primitive_type = "triangles"               # points/lines/line_strip/triangles/triangle_strip/fan

[[variants]]                               # 可选：其它用途配方
recipe = "LoadGeometry.Wire"
purpose = "debug"                          # depth_only / shadow_caster / picking / debug
lod = 0
```

规则：

- 主（Surface）变体**恒为第 0 条**，由 `recipe` 生成；`[[variants]]` 里写 `purpose = "surface"` 会被拒绝。
- `geometry` 与 `recipe` 缺一即解析失败（不猜默认值）。
- 未知 `primitive_type` / `purpose` / `schema != 1` 一律失败并带定位日志。

## 4. 运行时 API

```cpp
// 单文件装配（同名去重，需先加载配方）
auto *prm = gc->GetPrimitiveAssetRegistry()->GetOrLoad(OS_TEXT("res/primitive/pbr_sphere.primitive.toml"));

// ECS 侧一步建实体
RenderableBuilder(world, "GoldSphere")
    .WithTransform({0,0,0})
    .WithPrimitiveAssetPath(OS_TEXT("res/primitive/pbr_sphere.primitive.toml"))
    .Build();
```

`GraphicsContext` 上的两个注册器（设备级、拥有句柄）：

| 注册器 | 拥有 | 关停顺序 |
|--------|------|---------|
| `GeometryAssetRegistry` | `GeometryAsset` + 其 `Geometry*` | `PrimitiveAssetRegistry` 之后、模块之前清空 |
| `PrimitiveAssetRegistry` | `PrimitiveAsset` + 变体数组 | 最先清空（它观察前两者） |

⚠️ 句柄已拥有 `Geometry*`，**不要**再把同一个 `Geometry*` 交给 `GeometryManager::Add`。

## 5. 异步流送与渲染门禁

```
GeometryAsset 状态：MetaOnly → Loading → Resident / LoadFailed
                                    ↑
            GeometryStreamingSystem（RenderPreBeginFrame，每帧）
              Pump()          派发上一帧读盘完成 → 主线程解析 Minipack → 建 GPU buffer
              AcquireOneFrame 扫描句柄表，单帧最多提交 1 次读盘（几何比纹理重）
              Collect()       回收已归零的文件引用
```

- 内置几何体**不经过**该管线：`SetupFromSpec` 内同步建好，调用返回即 `Resident`。
- `LoadFailed` 不自动重试（坏文件每帧重读只会刷屏）；要重试请 `ClearAsyncRequest()`。
- `PrimitiveState::CanRender` 是双重门禁：`PrimitiveAsset::IsGeometryReady()` **且**
  材质必需贴图 `IsReady()`。没有几何资产句柄的手工 `PrimitiveAsset(Geometry*, recipe)`
  恒放行，既有示例行为不变。
- 诊断：`HasPendingGeometryResource(entity)` 与 `HasPendingMaterialResources(entity)`
  分别回答「卡在几何」「卡在贴图」。

## 6. 场景包异步加载

```cpp
SceneStreamingRequest req;              // 必须是 app 成员（比读盘请求活得久）
req.Begin(gc->GetLoadedFileManager(), OS_TEXT("res/X/X.Scene.scene"));

// 引擎每帧的 Pump() 会自动覆盖本请求；就绪后主线程解析：
if (req.IsReady() && req.Build(device, geo_mgr, gvf, recipe, base_dir, assets, nodes, roots))
    AttachEntities(assets, nodes);
```

`Build()` 与同步入口 `LoadStaticMeshSceneAsPrimitiveAssets` 走**同一段**解析代码
（`LoadStaticMeshSceneFromMemory`），只是数据来源从磁盘映射换成内存块。

## 7. 常见坑

1. **不要在头文件里引入 STL**：`toml11` 只在 `.cpp` 内使用，取出的 `std::string`
   立即转成 `AnsiString` / `OSString`。
2. **`PrimitiveAsset` 里的变体数组是裸指针**：注册器先把变体数组全部 `Add` 完，
   再取 `GetData()` 交给 asset；之后再扩容就是悬垂。
3. **门禁失效的典型表现**：几何未就绪却进了收集 → 空指针/非法 BDA。若新增来源
   （例如程序化 mesh），务必给它一个 `GeometryAsset` 句柄或明确置 `Resident`。
4. **配置诊断**：解析失败只打印一次并带 `debug_name`（文件名）+ 具体键，不要改成
   「静默用默认值」，否则 TOML 拼错会变成难查的画面异常。
