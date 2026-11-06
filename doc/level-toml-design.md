# 关卡级 TOML 编排设计（Level TOML）

> 目标：把一个演示/关卡里「一堆模型 + 各自的变换 + 各自的材质行」收进**一个** `*.level.toml`，
> 让上层从「手动加载 N 个 TOML、手写 N 段实体构造」变成**一次装载**：
>
> ```cpp
> auto *level = gc->GetLevelAssetRegistry()->GetOrLoad(OS_TEXT("example/Material/BasicLitMeshes.level.toml"));
> level->Instantiate(world, creater, instance);   // 模型子树内部走 ModelAsset 异步流送
> ```
>
> 本文定义数据格式与分阶段目标；下方“当前落地范围”记录实现与设计的差异。
>
> 命名沿革：本档原为 `scene-toml-design`。引擎的 3D 世界里 “Scene” 一词与
> glTF/`.model` 场景语义冲突，故规格化关卡统一正名为 **Level**（`LevelSpec` /
> `LevelAsset` / `LevelAssetRegistry` / `LevelLoader`，扩展名 `.level.toml` /
> `.level.fb`）；二进制模型包统一叫 **Model**（`.model` MDL2 MiniPack）。

### 当前落地范围

- 已实现 `LevelSpec` TOML/FlatBuffers（`.level.fb`）编解码、严格解析校验，以及支持
  `presets:`、异步 `geo:`/`primitive:` 几何引用与 `model` 节点（`.model` 包子树）的 `LevelLoader`。
- `BasicLitMeshes` 的几何、变换、材质行、纹理覆盖、相机和清屏色已迁入
  `example/Material/BasicLitMeshes.level.toml`；环境 profile 仍由世界/渲染目标配置。
- `GraphicsContext` 已持有 `LevelAssetRegistry`，按规范化路径缓存关卡规格；实例
  （`LevelInstance`）仍由调用方持有，须在世界与图形资源销毁前释放。
- `nodes[].model` 已接通：装载 `.model` 包 → `StaticMeshModelInstance` 实例化为子树实体，
  外层节点 TRS 由 wrapper 实体承载；`model` 与 `geometry` 互斥，`recipe` 必填（材质行绑定）。
- 环境 profile TOML、LevelWriter/Capture 尚未实现；关卡文件本身的非阻塞读盘在 backlog。

---

## 1. 现状与问题

`example/Material/BasicLitMeshes.cpp` 是当前形态的典型：

| 环节 | 现在要写什么 | 问题 |
|------|-------------|------|
| 几何 | 12 次 `CreateGeometryPreset(pc, ...)` | **已**数据化（预设名 → TOML），但名字散在代码里 |
| 材质配方 | `GetRecipe("06b.BasicLit.Lit")` | **已**数据化 |
| 材质行（SSBO） | `GetAccessor<EmissiveSurfaceRow>()` + `Write()` | 仍是代码，且**每个示例自己分配行** |
| 纹理覆盖 | `mesh_recipe.AddTexture(...)` | 代码 |
| 实体/变换 | 12 段 `CreateEntity` + `SetLocalPosition/Rotation/Scale` | 纯代码，且排列逻辑（绕圈分布）在 C++ 里 |
| 相机 | `InitCamera()` 手填 yaw/pitch/distance | 纯代码 |
| 环境 | `EnvironmentManager::Create/Edit` | 纯代码 |

也就是说：**形状、配方、纹理都已数据化，缺席的是「编排层」**——谁放在哪、用什么材质行、相机在哪。
关卡 TOML 就是补这一层，它不引入新的资源类型，只是把已有的几何/配方/纹理/模型包**引用起来**并加上变换与实例参数。

---

## 2. 分层与职责

```
*.level.toml            纯数据编排（本文定义）
      │  toml11 只在 .cpp 内
      ▼
LevelSpec               纯数据中间层（HGL 容器，无 GPU 概念、可回写 TOML）
      │
      ▼
LevelAssetRegistry      按路径去重 + 持有 LevelAsset（挂 GraphicsContext，设备级）
      │
      ├─▶ LevelLoader   规格 → ECS：几何(GeometryAssetRegistry) / 配方(MaterialRecipeRegistry)
      │                 / 材质行(GlobalSSBOBufferRegistry) / 模型子树(ModelAsset)
      │                 / 变换 / 相机 / 环境
      └─▶ LevelCapture  反向：活体世界 → LevelSpec → TOML（作者工具，非运行期必需）
```

三条硬边界：

1. **关卡不拥有资源**。几何、配方、纹理、模型包仍是各自的注册器拥有；关卡只持引用（裸观察指针 / 路径串）。
2. **关卡拥有实例参数**。材质行（SSBO）分配、实体、变换、可见性归关卡 —— 这解决了“每示例自己建行”
   的重复代码与行泄漏风险。
3. **TOML 只描述，不执行**。不写脚本、不写条件分支，避免配置里长出编程语言。

---

## 3. `*.level.toml` schema（schema = 1）

### 3.1 顶层骨架

```toml
schema = 1
name = "BasicLitMeshes"            # 必填：关卡名（诊断/查找）
description = "13 个内置形状绕圈摆放"  # 可选

[environment]                      # 可选：省略 = 沿用世界当前 profile
profile   = "basic_lit"            # 具名 profile；同名复用
ibl_texture = "res/image/.../sky.hdr"   # 可选：给出了就建/改该 profile 的 env_tex
ambient_provider = "ambient_light_sky"  # 可选：算法轴（GLSL 模块名）

[camera]                           # 可选：省略 = 用 ViewModel 默认参数
mode     = "view_model"            # view_model | free | orbit
position = [0.0, 0.0, 6.0]         # free 模式用
target   = [0.0, 0.0, 0.0]
distance = 16.0
yaw      = 45.0
pitch    = -20.0
fov_deg  = 45.0
main     = true

[clear_color]                      # 可选
value = [0.2, 0.2, 0.2, 1.0]
```

> `[clear_color]` 单列一张表是为了将来能扩（渐变/天空色），而不是写一个裸数组键。

### 3.2 关卡级材质实例（关键）

把「材质行」提升为**具名实例**，节点只引用名字。这是“一个 TOML 搞定”的核心：

```toml
[[materials]]
name = "lit_default"
type = "EmissiveSurface"           # GlobalSSBOType 的名字（GetGlobalSSBOTypeName 的真源）
color = [0.82, 0.82, 0.82, 1.0]

[[materials]]
name = "pbr_gold"
type = "PBRSurface"
base_color  = [1.0, 0.78, 0.28, 1.0]
metallic    = 1.0
roughness   = 0.25
normal_scale = 1.0
fresnel      = 0.04

[[materials]]
name = "glass"
type = "TransmissionSurface"
trans_color = [0.9, 0.95, 1.0, 0.6]

# 纹理覆盖（可选）：配方里已声明的槽位，按名字重新指定资源
[materials.textures]
base_color = "res/image/pbr/Concrete_Plain/base.png"
normal     = "res/image/pbr/Concrete_Plain/normal.png"
```

规则：

- `type` 必须是 `GlobalSSBOType` 里**有行结构**的类型；名字大小写不敏感匹配。
- 键白名单**按 type 分派**：`EmissiveSurface` 允许 `color`；`PBRSurface` 允许
  `base_color/metallic/roughness/normal_scale/fresnel`。写别的键 ⇒ 显式失败（与 `*.recipe.toml`
  的未知键策略一致），不静默忽略。
- 未给的字段用行结构的**默认值**（`PBRSurfaceRow{}` 的默认成员），不是 0。
- 同名材质只写一次；N 个节点引用同一行 ⇒ 天然共享（这正是 SSBO 的价值）。

### 3.3 节点树

```toml
[[nodes]]
name     = "Floor"
geometry = "presets:plane_square"       # 见 §4 引用语法
recipe   = "06b.BasicLit.Lit"
material = "lit_default"                # 引用 [[materials]] 的名字
mobility = "static"                     # static | movable
visible  = true

[nodes.transform]
position = [0.0, 0.0, 0.0]
rotation = { axis = [0.0, 0.0, 1.0], angle_deg = 45.0 }
scale    = [1.0, 1.0, 1.0]

[[nodes]]
name     = "Mesh_0"
parent   = ""                           # 空/省略 = 根
geometry = "presets:sphere_64"
recipe   = "06b.BasicLit.Lit"
material = "lit_default"
transform = { position = [6.5, 0.0, 0.0] }   # 只给需要的轴，其余取默认

[[nodes]]
name     = "ABeautifulGame"             # 复合模型子树（与 geometry 互斥）
model    = "res/ABeautifulGame.StaticMesh/ABeautifulGame.model"
recipe   = "06b.BasicLit.Lit"           # model 节点必填：包内图元的材质行绑定
transform = { scale = [0.01, 0.01, 0.01] }
```

要点：

- **平铺 + `parent` 引用**，不用数组嵌套：TOML 的 `[[nodes]]` 已是数组，再嵌 `[[nodes.children]]`
  会把层级关系写成缩进依赖，重排成本极高。装载器按「父先于子」的拓扑序建实体；出现环 ⇒ 显式失败。
- `transform` 三选一即可：`{ position }` / `{ rotation = {axis, angle_deg} }` / `{ scale }`，
  未给的分量取单位值。需要四元数直写时用 `rotation = { quat_wxyz = [w,x,y,z] }`（与引擎 `Quatf` 同序）。
- `mobility = "static"` 时装载器用 `Mobility::Static`（批量静态优化）；默认 `movable`。
- `visible = false` 用于保留节点但开场不画（排查时可以只改一个布尔）。
- `model` 与 `geometry` **互斥**：给了 `model` 就必须给 `recipe`；模型包自身的根变换保留，
  外层 `transform` 由 wrapper 实体承载；`visible` 沿祖先链级联到整个子树。

**实例差异**（同一几何/配方，个别节点要不同颜色时）：

```toml
[[nodes]]
name     = "Highlight"
geometry = "presets:cube"
recipe   = "06b.BasicLit.Lit"

[nodes.material]                # 就地覆盖：不给 name，直接写行字段
type        = "EmissiveSurface"
color       = [1.0, 0.0, 0.0, 1.0]

[nodes.textures]                # 就地纹理覆盖（比配方级覆盖优先级高）
base_color = "res/image/checker.png"
```

`[nodes.material]` 与 `material = "名字"` 互斥（同时给 ⇒ 失败）。

### 3.4 可选：资产清单

```toml
[assets]
recipes  = ["ShaderLibrary/asset/recipe/Examples/Material/06b.BasicLit.Lit.recipe.toml"]
primitive_dirs = ["res/primitive"]
```

用途：在装载前**一次性**把配方目录/文件加载好，避免「节点引用了配方但注册器里还没有」的时序问题。
省略时装载器按引用惰性加载（配方走 `FindOrLoadMaterialRecipe` 的默认目录扫描）。

---

## 4. 引用语法（统一 URI）

一个关卡里会出现五类引用，全部用**带前缀的字符串**，不猜：

| 前缀 | 例子 | 解析 | 备注 |
|------|------|------|------|
| `presets:` | `presets:sphere_64` | `GeometryPresetLibrary`（共享 VDM 的形状参数） | 资源仍归调用方/关卡的 creater |
| `geo:` | `geo:res/geo/unit_sphere.geo.toml` | `GeometryAssetRegistry::GetOrLoad` | 独立几何资产，文件来源自动异步 |
| `primitive:` | `primitive:res/primitive/pbr_sphere.primitive.toml` | `PrimitiveAssetRegistry::GetOrLoad` | 与 `geometry`+`recipe` **互斥** |
| `recipe:` | `recipe:06b.BasicLit.Lit` | `MaterialRecipeRegistry::FindOrLoad` | 也可直接写裸名字（向后友好） |
| `tex:` | `tex:res/image/x.png` | `TextureManager::CreateTextureAsset` | 等价于现在的裸路径 |

`nodes[].model` 不走前缀语法：直接写 `.model` 包路径（MDL2 MiniPack），由
`ModelAsset`/`ModelStreamingSystem` 流送装载（见 §8）。

设计取舍：**默认不加前缀的字符串按“配方名”解释**（`recipe` 字段最常见），其余字段必须带前缀。
好处是 `recipe = "06b.BasicLit.Lit"` 这种既有写法继续有效，而 `geometry = "sphere_64"` 这种
歧义写法会被拒绝并提示应写 `presets:sphere_64`。

---

## 5. 运行时装载

### 5.1 入口

```cpp
// 一步到位（推荐给示例）
LevelAsset *level = gc->GetLevelAssetRegistry()->GetOrLoad(path);
bool ok = level->Instantiate(world, creater, instance);
```

`GetOrLoad` 同步解析 TOML/`.level.fb`（纯文本/小二进制，读盘即解析）；重型内容
（几何 `geo:`/`primitive:`、模型包 `model`）由各自的注册器/流送系统异步补齐，
未就绪的资源由渲染门禁自然挡在收集之外。

### 5.2 就绪语义

`LevelAsset` 是“解析完的规格书”句柄，不做状态机：解析成功即可 `Instantiate`。
真正需要等待的是产物级就绪：

| 层 | 就绪判据 | 由谁推进 |
|------|---------|---------|
| 规格 | TOML/FB 解析 + 校验通过 | `LevelAssetRegistry::GetOrLoad`（同步） |
| 几何/图元 | `PrimitiveState::CanRender`（几何 + 必需贴图 Resident） | `GeometryAssetRegistry` / 流送 |
| 模型子树 | `ModelAsset` 状态机（`Assembling → Ready`） | `ModelStreamingSystem`（异步，主线程建 GPU 资源） |

**关键**：`presets:` 引用的几何是同步创建的（纯内存计算），所以只有 `geo:`/`primitive:`/`model`
引用会让内容有短暂无就绪期。BasicLitMeshes 这种纯内置形状场景**一次调用即出画**。

### 5.3 装载步骤（伪代码）

```
1. 解析 TOML → LevelSpec（未知键/schema/URI 校验，全部失败即返回，不做半装载）
2. 解析 [[materials]]：按 type 分配 SSBO 行、写值、记 name → accessor
3. 拓扑排序 nodes（父先子）
4. 对每个 node：
     model 分支：装载 .model 包 → StaticMeshModelInstance 实例化子树
                 → wrapper 实体承载外层 TRS → 根节点 SetParent → 可见性级联
     geometry 分支：解析 geometry → Geometry*（presets: 走关卡自己的共享 creater；geo: 走注册器）
     解析 recipe   → const MaterialRecipe*
     解析 material → accessor（或就地覆盖新建行）
     CreateEntity + CreateTransform + GeometryData.SetPrimitiveAsset + MaterialData.SetDataResource
5. [environment] / [camera] / [clear_color]
6. 全部成功才把实体标记为「关卡拥有」；中途失败 ⇒ 回滚已建实体（避免半个关卡）
```

第 6 步很重要：现在的示例都是 `Init()` 返回 false 就整体退出，而装载器必须能**回滚**，
否则“改错一个键”会留下半截世界且难以定位。

### 5.4 与渲染门禁的衔接

不需要新增门禁：`PrimitiveState::CanRender` 已经在查「几何就绪 + 必需贴图就绪」。
关卡只是把节点建出来，未就绪的节点自然不进收集 —— **关卡装载完成 ≠ 全都画出来**，
这正好符合异步语义。

---

## 6. 保存 / 捕获

「保存成 TOML」有两个层次，别混：

### 6.1 `LevelSpec → TOML`（序列化，必做）

`LevelWriter::Write(const LevelSpec &, const OSString &path)`。规则：

- 键顺序固定（便于 diff）：`schema` → `name` → `environment` → `camera` → `[[materials]]` → `[[nodes]]`。
- 浮点用最短往返表示（`%.9g`），保证 `写→读→写` 逐字节稳定。
- 默认值**不写**（`scale = [1,1,1]` 省略），让文件只承载“非默认”的信息。

### 6.2 `World → LevelSpec`（捕获，作者工具）

`LevelCapture::Capture(ECSContext *world)` 遍历有 `GeometryData` 的实体：

| 抓什么 | 从哪抓 | 难点 |
|--------|--------|------|
| 名字 / 层级 | `Entity::GetName()`、`TransformAccessor` 的 parent | 现有实体树有 parent 关系，直接映射 |
| TRS | `TransformAccessor::GetLocal*` | 无损 |
| 几何 | `PrimitiveAsset::GetGeometryAsset()->GetSpec().name` | **只有走预设/资产建出来的几何才有名字**；程序化直接 `new` 的几何抓不到 ⇒ 记为 `presets:<shape>_<params摘要>` 或拒绝并报告 |
| 配方 | `PrimitiveAsset::GetMaterialRecipe()->name` | 行内临时配方（`MaterialRecipe{}` 栈对象）没有名字 ⇒ 需先注册 |
| 材质行 | `MaterialData` 的 `GlobalSSBOBinding` → `GlobalSSBOBufferRegistry` 反查行内容 | **需要每种 row type 的“读”函数**（见 §7） |

因此**捕获的前提是 §7 的行编解码表双向可用**。第一版可以只做「写」，捕获延后（见 §9）。

捕获的现实价值：把已迁移示例**自动导出**成关卡 TOML，人再手工修两处（名字、绕圈分布
的算法本来在 C++ 里，捕获得到的是展开后的**结果**，正好是想要的“一堆模型”）。

---

## 7. 材质行编解码表（设计的硬骨头）

行是**类型化 POD**（`PBRSurfaceRow` / `EmissiveSurfaceRow` / `TransmissionSurfaceRow`），
TOML 是通用键值。二者之间必须有一张显式表，否则只能存原始字节（不可读、跨版本即碎）。

```cpp
// inc/hgl/graph/asset/LevelMaterialRowCodec.h
struct LevelMaterialRowCodec
{
    const char *type_name;                                  // = GetGlobalSSBOTypeName(...)
    GlobalSSBOType type;
    bool (*parse)(const toml::value &, void *row);           // 只在 .cpp 内见到 toml::value
    void (*write)(const void *row, TomlWriter &);            // 供 LevelWriter
    uint32_t row_size;
};
```

- 表按 `GlobalSSBOType` 覆盖，`GlobalSSBOTypeNameTableComplete()` 已有的 `static_assert`
  能挡住「加了枚举没加 case」。
- 新增行类型 = 加一条 codec + 一行单测，**不需要改关卡装载器**。
- 这是**唯一**允许出现 `toml::` 的地方之一（另一个是解析器本身），且不进入公共头文件
  （头文件里用不透明 `const void*`，或者干脆把 codec 的注册函数放在 `.cpp`）。

---

## 8. 与 `.model`（MDL2 二进制模型包）的关系

两者不是竞争关系，而是**互相引用**：

| | `.model`（二进制包） | `*.level.toml`（本设计） |
|---|---|---|
| 内容 | 导出的网格 + 节点矩阵 + 材质索引 | 编排：引用 + 变换 + 实例参数 + 相机/环境 |
| 生产者 | DCC/导出工具（GLTFConvert） | 人写 / 捕获 |
| 典型用法 | 一个模型资产的内部结构 | 关卡/演示的装配 |
| 装载 | `ModelAsset` + `ModelStreamingSystem`（异步流送） | `LevelAssetRegistry`（同步解析规格） |

（命名沿革：`.model` 包原名 `.scene`/SCN2，与关卡语义冲突，阶段一已整体正名为
`.model` / MDL2，见 `inc/hgl/graph/mesh/ModelPackFormat.h`。）

已落地：`[[nodes]]` 里的 `model` 字段把一个二进制模型包作为子树挂进关卡：

```toml
[[nodes]]
name = "ABeautifulGame"
model = "res/ABeautifulGame.StaticMesh/ABeautifulGame.model"
recipe = "06b.BasicLit.Lit"
transform = { position = [0.0, 0.0, 0.0], scale = [0.01, 0.01, 0.01] }
```

装载器（`LevelLoader`）展开为子树：包内节点/图元实体由
`StaticMeshModelInstance` 创建，外层 TRS/可见性由 wrapper 实体承载。
这样“一个大模型 + 周围一堆方块”才是一个 TOML。

---

## 9. 分阶段落地

| 阶段 | 内容 | 交付判据 |
|------|------|---------|
| **V1** | `LevelSpec` + `LevelAssetFile` 解析（白名单/URI/拓扑校验）+ `LevelLoader` + `LevelAssetRegistry` 挂 `GraphicsContext` | 解析器、加载器、设备级注册器和 `BasicLitMeshes` 的 12 个实体场景已落地 ✅ |
| **V2** | `[[materials]]` 的 codec 表（PBR/Emissive/Transmission 三种）+ `[environment]` 落地；`[camera]`、`[clear_color]` 与 `geo:`/`primitive:` 异步路径 | 示例里**没有**一段“自己分配行”的代码 ✅ |
| **V2.5** | `model` 节点（`.model` 包子树 + 材质行绑定 + 互斥校验 + 判据测试） | 已落地 ✅ |
| **V3** | `LevelWriter`（Spec→TOML）+ `LevelCapture`（World→Spec）+ `TestLevelRoundTrip`（写→读→写逐字节一致） | 示例的几何/变换/材质由捕获生成，人只增删节点 |
| **V4** | 示例全面切换为「一个 TOML 一条命令」 | `BasicLitMeshes.cpp` 缩到 ~40 行 ✅（已达成） |

当前形态对照（`BasicLitMeshes`）：

```cpp
bool Init() override
{
    SetClearColor(Color4f(0.2f, 0.2f, 0.2f, 1.0f));

    auto *level_asset = GetGraphicsContext()->GetLevelAssetRegistry()->GetOrLoad(
        OS_TEXT("example/Material/BasicLitMeshes.level.toml"));

    return level_asset && level_asset->Instantiate(GetECSContext(), &creater, level_instance);
}
```

---

## 10. 设计决策与坑

1. **为什么材质行必须关卡级**：行是 GPU 池里的**租约**。现在每个示例自己 `GetAccessor` + `Write`，
   示例一多就会出现“谁负责归还”的模糊地带。收进关卡后，`LevelAsset` 销毁即归还，
   与 `GeometryAsset` 拥有 `Geometry*` 是同一条纪律。
2. **为什么 `[[materials]]` 用具名而不是节点内联**：多个节点共享一行是常态，内联会让文件里
   重复多段相同的颜色；具名后是“声明 1 段 + 多处引用”。
3. **为什么节点用平铺 + parent**：见 §3.3；TOML 的嵌套数组会把层级写进缩进，重排即大改。
4. **不要**引入“节点级脚本/条件”：配置里一旦有分支，就等于把 C++ 搬到 TOML 里还失去了调试器。
   需要动态行为时用 ECS 系统的 C++ 代码，关卡只描述**初始状态**。
5. **默认值不写**：文件越小越可读，diff 也只显示真正的意图变化。
6. **失败要整体回滚**：见 §5.3 第 6 步；否则调错一个键会留下半个世界，比直接失败更难查。
7. **路径相对关卡文件**：`geo:` / `primitive:` / `tex:` / `model` 的相对路径按 **`.level.toml` 所在目录**
   解析（与 `*.recipe.toml` 的纹理路径约定一致），资产整体搬迁不必改内容。
8. **不要**让 `LevelSpec` 持有 `Geometry*`/`MaterialRecipe*`：那会让“解析”和“装载”耦合，
   也就失去 §5.1 的“先预取再实例化”两段式。
9. **Scene 与 Level 一个词不能两头用**：3D 世界的关卡编排叫 Level；glTF/模型包内部
   的 scene 语义在导出正名后统一落为 `.model` 包（`nodes[].model`）。“SceneGraph”
   仅保留为引擎子系统名（`src/SceneGraph/`），不指任何资产格式。
