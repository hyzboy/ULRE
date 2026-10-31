# 材质实例（配方继承）与批量材质数据块

> 解决两件互相咬合的事：
>
> 1. **一份材质、多种参数**：同一套渲染状态与着色器，只改颜色 / 金属度 / 粗糙度。
>    语义对齐 UE 的 Material Instance —— 子配方继承父配方，**只写差异**。
> 2. **批量数据行不再硬编码**：`PBRSpheres` 那种 100 行 SSBO 数据行改成文件，
>    示例里只剩一条 URI 引用。
>
> 两者合起来才成立：批量示例往往是「**一个配方 + N 行数据**」（同一材质，
> 不同球不同金属度），而 100 行参数既不该写在 C++ 里，也不该写成 100 个配方文件。

---

## 1. 为什么不是在 C++ 里造参数

改造前的典型写法（`example/Material/PBRSpheres.cpp`）：

```cpp
// 100 次 GetAccessor<PBRSurfaceRow>() + 100 行结构体赋值
for (int row = 0; row < 10; ++row)
    for (int col = 0; col < 10; ++col)
    {
        auto acc = pool->GetAccessor<PBRSurfaceRow>();
        ssbo::PBRSurfaceRow r{};
        r.base_color = {0.72f, 0.72f, 0.72f, 1.0f};
        r.metallic   = float(col) / 9.0f;
        r.roughness  = 0.05f + float(row) / 9.0f * 0.95f;
        acc.Write(r);
        sphere_slot_accessors[row][col] = acc;
    }
```

问题不在「这段代码丑」，而在于：

- 数据是**内容**，却挂在**代码**上 —— 离线工具没法改，美术没法调；
- `sphere_slot_accessors[10][10]` 这种访问器数组是把「池行号」这种易变实现细节
  搬进了示例，池一改布局就得跟着改；
- 同一个双循环在多个示例里重复，且每份数值都不一样，无法复用。

---

## 2. 配方继承：`parent`

### 2.1 写法

`MaterialRecipeSpec` 增加一个字段（`AssetGen` 自动生成读写，不需要手写序列化）：

```toml
# ShaderLibrary/asset/recipe/RoughRed.recipe.toml
schema     = 1
name       = "RoughRed"
parent     = "rcp:@Lit"          # 类型化资产 URI，同类型 ⇒ 可以直接写 "@Lit"

[surface]
base_color   = [0.9, 0.15, 0.12, 1.0]
roughness    = 0.85
```

子配方**只写差异**：没出现在子文件里的键一律取父配方（传递到祖先链顶端）。

### 2.2 差量合并的口径：`Presence<T>`

合并是「按键是否出现过」，不是「值是否等于默认值」。`Presence<T>`
（`inc/hgl/serialize/Archive.h`）承担这个区分：

| 子文件里 | 结果 |
|---|---|
| 没写这个键 | `present == false` ⇒ **不覆盖**父值 |
| 写了（哪怕写的就是默认值） | `present == true` ⇒ **覆盖**父值 |

所以「显式写 `metallic = 0.0`」和「不写 `metallic`」是两件事。这也是能在文件里
把父配方的某个非零值**改回 0** 的唯一办法。

### 2.3 加载是两阶段的

`MaterialRecipeRegistry` 不能「读一个键就套一个父」——父可能后加载、可能不在同一
目录、甚至可能是被继承多次的中间层。所以：

1. **第一阶段**：把所有 recipe 文件的 `parent` 记下来，只建壳，不合并；
2. **第二阶段**：按 `parent` 拓扑序合并，父先就绪再合子。

`parent` 自身走 `AssetURI` 解析（`rcp:` 类型），因此父配方既可以是
`rcp:@Lit`（按名字），也可以是 `rcp:file:ShaderLibrary/asset/recipe/Lit.recipe.toml`。

循环引用（`A → B → A`）在第二阶段被检出并判该配方失败，不会无限递归。

### 2.4 与「一个配方 + N 行数据」的分工

| 变化的东西 | 放哪 |
|---|---|
| 渲染状态、着色器、纹理、**整体基调** | 配方（可继承，一个文件一份） |
| **逐实例**的数值（100 个球各自不同） | 数据块的一行 |

100 个球共用 `PBRSpheres` 配方；每球的 `PBRSurfaceRow` 取自数据块的第
`row * 10 + col` 行。配方管「怎么画」，数据块管「画成什么颜色」。

---

## 3. 材质数据块：`mtladata` 资产的三种形态

一个「数据块」= 一段**物理行数据**：`row_bytes` + `row_count` + 起址。
同一个块有三种落盘/内存形态，可互转（`TestMaterialDataBlockCodec` 守住往返一致性）：

| 形态 | 头/API | 用途 |
|---|---|---|
| **TOML** | `ParseMaterialDataBlockFile` / `WriteMaterialDataBlockToml` | 人可读、可手改、可进代码评审 |
| **FlatBuffers** | `EncodeMaterialDataBlockFlat` / `DecodeMaterialDataBlockFlat` / `BuildMaterialDataBlockFromFlat` | 工具链之间的结构化交换 |
| **`MDB1` 二进制** | `ViewMaterialDataBlockBinary` / `ReadMaterialDataBlockBinary` / `WriteMaterialDataBlockBinary` | 运行期零解析直通 GPU |

### 3.1 `MDB1` 布局

```
偏移  0   u8[4]  'M','D','B','1'
偏移  4   u16    version   (1)
偏移  6   u8     row_type  (GlobalSSBOType)
偏移  7   u8     reserved  (0)
偏移  8   u32    row_bytes
偏移 12   u32    row_count
偏移 16   ...    row_bytes * row_count 行数据
```

常量：`kMaterialDataBlockBinaryHeaderSize = 16`、`kMaterialDataBlockBinaryVersion = 1`、
`kMaterialDataBlockMaxRows = 1 << 16`。

`偏移 16` 之后是**纯 POD 行数据、无 padding、无指针对齐要求**，所以
`ViewMaterialDataBlockBinary()` 返回的视图可以 `memcpy` 到 mapped GPU 指针 —— 这就是
「文件 → GPU」的最短路径，中间不需要反序列化成对象再序列化回去。

### 3.2 FlatBuffers 能不能直接当 `VkBuffer` 内容？

**不能**，原因很具体：

- FlatBuffers 的 **table** 首部是根偏移 + vtable 偏移，每行前面还挂着偏移量。
  把整段字节按「行宽 = `sizeof(Row)`」铺进 SSBO，shader 读到的会是偏移量而不是字段；
- 但 FlatBuffers 的 **struct**（`FLATBUFFERS_STRUCT_END`，非 table）是纯 POD、
  字段顺序与内存布局固定、无内部偏移 ⇒ **连续排列就是行数组本身**。

所以正确做法是 `rows()` 返回 `[struct]` 向量，取 `Data()` 后 `memcpy` 到 mapped 指针；
表头（`name` / `target` / `row_bytes`）仍由 table 承载。即：**表头走 FlatBuffers，
行体走 POD**。这也是 `MDB1` 存在的理由 —— `MDB1` 相当于把「表头 + POD 行体」压成
一个无解析前缀，方便直接把整个文件映射进显存（一次性 `vkCmdUpdateBuffer` / staging 拷贝）。

### 3.3 TOML 形态的两个硬性键

`.material_data.toml` 的头必须有：

```toml
schema = 1
target = "PBRSurface"      # 决定行的字段集与行宽
```

- 缺 `schema` ⇒ `InvalidValue`；
- 缺 `target`：行字段无处投影 ⇒ 未知键报 `UnknownKey`；
- **写出**必须走 `WriteMaterialDataBlockToml()`：`SaveToTomlText(spec)` 会把并集
  （7 个字段）全写出来，再读回来就会撞 `UnknownKey`。`WriteMaterialDataBlockToml`
  会先按 `target` 过滤字段集合。

---

## 4. 资产层：`mtldata` 类型

`AssetType::MaterialData`（拼写 `mtldata` / 别名 `material_data`）已进
`AssetTypeTable[]`，于是 URI 门面直接可用：

```
mtldata:file:ShaderLibrary/asset/material_data/pbr_10x10_grid.material_data.toml
mtldata:@pbr_10x10_grid
```

| 组件 | 位置 | 职责 |
|---|---|---|
| `MaterialDataBlockAsset` | `inc/hgl/graph/asset/MaterialDataBlockAsset.h` | 一份块的句柄 + 状态机 |
| `MaterialDataBlockRegistry` | `inc/hgl/graph/asset/MaterialDataBlockRegistry.h` | 加载 / 按名字或路径查 / 整块上传 |
| `MaterialDataBlockObject` | `inc/hgl/graph/asset/MaterialDataBlockObject.h` | `AssetObject` 外壳 + `AssetFactory` 加载器 |

### 4.1 状态机

```
Parsed  ──Upload(池)──►  Ready
   │
   └─解析失败──► Failed（仍留在表里，工具可以读失败原因）
```

上传失败（无 `GraphicsContext` / 池满 / 行宽不符）**不算** `Failed` —— 停在
`Parsed`，只把原因写进 `failure_reason`。这样无图形设备的离线工具照样能读行数据，
`GetFailureReason()` 返回 `const AnsiString &`。

### 4.2 生命周期

`MaterialDataBlockRegistry` 是 `GraphicsContext` 的成员（`GetMaterialDataBlockRegistry()`），
比任何示例 app 都活得久。块的 `GlobalSSBORowRangeLease` 持有一段池行区间，
所以 `GraphicsContext::Shutdown()` 里 **`material_data_block_registry_.Clear()` 必须排在
`SAFE_CLEAR(module_manager)` 之前** —— 否则归还行区间时池已经没了。

---

## 5. 批量行：从池到实体

### 5.1 池侧：连续行区间

批量上传要的是**连续**行号（块内第 i 行 == 池行号 `first + i`），
而 `ActiveRowPool::Acquire()` 为了复用会从 Idle 里挑零散行号。所以另开一套：

```cpp
uint32_t AcquireRun(uint32_t count);                 // 从水位起整段占用，永不复用 Idle
bool     ReleaseRun(uint32_t first, uint32_t count);
bool     CommitRun (uint32_t first, uint32_t count); // 整段只 MarkDirty 一次
```

`AcquireRun` 用 512 行一批的 `CreateActive`，每批校验编号连续，任何一批失败整段回滚。

`GlobalSSBOBufferRegistry` 上的薄封装：

```cpp
uint32_t AcquireRowRange(GlobalSSBOType type, uint32_t count);
bool     ReleaseRowRange(GlobalSSBOType type, uint32_t first, uint32_t count);
bool     WriteRowRange  (GlobalSSBOType type, uint32_t first, const void *data, uint32_t count);
GlobalSSBORowRangeLease AcquireRowRangeLease(GlobalSSBOType type, uint32_t count); // RAII
bool     UploadMaterialDataBlock(const mtl::MaterialDataBlockView &view,
                                 GlobalSSBORowRangeLease &out_lease);  // 整块上传
```

`UploadMaterialDataBlock` 是「文件里的块」与「池里的行」之间**唯一**的口径检查点：
视图合法 → 池就绪 → `view.row_bytes == pool->GetRowBytes()`，任一项不符就拒（fail-fast），
绝不安静地按错误行距铺满整池。

> ⚠️ `MaterialDataBlockView::rows` 是 `const uint8 *`。传给 `WriteRowRange` 时必须
> 显式 `static_cast<const void *>`，否则会绑到按行模板重载、把 `T` 推成 `uint8_t`，
> 然后死在行距检查上（运行期才发现的蠢错）。头文件里的 `static_assert(sizeof(T) > 1)`
> 就是把这个错提前到编译期。

### 5.2 实体侧：`WithMaterialDataRow`

```cpp
builder->WithMaterialDataRow(ToOSString(PBR_GRID_DATA_PATH), row * GRID_SIZE + col);
```

它做四件事（`src/ecs/support/RenderableBuilder.cpp`）：

1. 从 `GraphicsContext` 取 `MaterialDataBlockRegistry` 与全局池；
2. `blocks->GetOrLoadURI(block_uri)` —— 不碰文件格式，只认 URI；
3. `block->Upload(pool)`（幂等）—— 第一次调用整块上传，之后是空操作；
4. `row_index >= block->GetRowCount()` 判越界，然后拼
   `GlobalSSBOBinding{ GetGlobalSSBOType(), GetSSBOId(), RowIDAt(row_index) }`，
   落到 `WithMaterialDataResource(binding)`（复用唯一授权入口，不另开绑定路径）。

**顺序是契约**：先上传再查行号。反过来的话 `RowIDAt()` 会返回
`ActiveRowPool::InvalidRowID`，看起来像「越界」其实只是「还没上传」。

### 5.3 配方级 vs 实体级

改造后 `PBRSpheres` 的配方仍带一个 `material_ssbo_binding`（块首行），
**但每帧真实行号由 per-entity `MaterialData` 组件的 4-ID 表承载**。
也就是说：配方管池/块身份，实体行管「这个实例取哪一行」——
「同一配方 100 个不同数据行」就是这么成立的。

---

## 6. 落点约定

| 资产 | 目录 | 说明 |
|---|---|---|
| 材质配方 | `ShaderLibrary/asset/recipe/**/*.recipe.toml` | 已有 62 份，由 `MaterialRecipeRegistry` 整目录加载 |
| 材质数据块 | `ShaderLibrary/asset/material_data/*.material_data.toml` | 本轮新增 |

- `ShaderLibrary/` 是主仓库普通目录，示例程序在仓库根 `E:\ULRE` 运行，所以路径就是
  `ShaderLibrary/asset/material_data/...`（相对工作根）。
- **`res/` 是 git 子模块**（`git@git.hyzgame.com:hyzboy/RuntimeData.git`），
  资产文件的改动不要提交到主仓库。
- 配方加载根是 `mtl::GetShaderLibraryPath() + "asset/recipe"`
  （`GraphicsContext::FindOrLoadMaterialRecipe`），说明 `ShaderLibrary/` 就是资产根。

---

## 7. 示例改造对照

```toml
# ShaderLibrary/asset/material_data/pbr_10x10_grid.material_data.toml（节选）
schema = 1
name   = "pbr_10x10_grid"
target = "PBRSurface"

[[rows]]                       # index 0 → row 0, col 0
base_color   = [0.72, 0.72, 0.72, 1.0]
metallic     = 0.0
roughness    = 0.05
normal_scale = 0.35

[[rows]]                       # index 1 → row 0, col 1
base_color   = [0.72, 0.72, 0.72, 1.0]
metallic     = 0.111111
roughness    = 0.05
normal_scale = 0.35
```

```cpp
// 改造前：双循环 + 两个二维数组
if (!CreateLitMaterialData()) return false;
if (!InitMaterialDataSSBO())  return false;
// ...
builder->WithMaterialRow(sphere_slot_accessors[row][col], sphere_material_data[row][col]);

// 改造后：一份文件 + 行下标
if (!InitMaterialDataBlock()) return false;     // GetOrLoadURI + Upload + 行数校验
// ...
builder->WithMaterialDataRow(ToOSString(PBR_GRID_DATA_PATH), row * GRID_SIZE + col);
```

改动量：`PBRSpheres.cpp` 少 56 行、多 933 行（其中 100 行是数据文件本身，
其余是新测试与注释）。示例里**不再出现任何逐行参数**。

---

## 8. 门禁与回归

```powershell
# 规约结构一致性（改了 HGL_ASSET_STRUCT 注释/字段必须重跑 -Write）
pwsh -NoProfile -File scripts\run-assetgen.ps1 -Check -Clang "C:\clang\bin\clang++.exe"

# 相关单元测试（均在仓库根 E:\ULRE 运行，依赖相对路径）
ctest --test-dir build -C Release -R "TestMaterialDataBlockCodec|TestGlobalSSBORowRange|TestMaterialDataBlockAsset|TestRenderableBuilderDataRow|TestPbrGridDataBlock|TestMaterialRecipeFile|TestMaterialRecipeInheritance|TestMaterialRecipeCodec" --output-on-failure
```

| 测试 | 守什么 |
|---|---|
| `TestMaterialDataBlockCodec` | 三形态往返一致 + 双后端（TOML / Flat）口径一致 |
| `TestGlobalSSBORowRange` | 连续区间语义 + 租约无效态 fail-closed + 源码契约（**无需图形设备**） |
| `TestMaterialDataBlockAsset` | `mtldata` 门面：解析 / 查表 / 状态机 / 失败原因 |
| `TestRenderableBuilderDataRow` | 块内行 → `GlobalSSBOBinding` 组合、越界一律 `InvalidRowID` |
| `TestPbrGridDataBlock` | 示例用数据文件的形状 + **100 行数值与原生成公式一致** + 演示源码不再逐行硬编码 |
| `TestMaterialRecipeInheritance` | `parent` 差量合并口径 + 两阶段加载 + 循环检测 |
| `TestMaterialRecipeCodec` | `parent` 字段的编解码往返 |

示例冒烟（`<目标>.exe` 必须在仓库根运行）：

```powershell
$env:PBR_FRAMES='12'; .\build\out\Windows_64_Release\PBRSpheres.exe
```

正常输出应包含：

```
[MaterialDataBlockRegistry] 数据块已加载: .../pbr_10x10_grid.material_data.toml name=pbr_10x10_grid type=PBRSurface rows=100 row_bytes=32
[MaterialDataBlockAsset] 数据块已上传: pbr_10x10_grid type=PBRSurface rows=100 first=1 ssbo=2
[T5] GPU cull instanced draw ENGAGED: ... buckets=10 items=100
```

---

## 9. 像素验收：A/B 对拍（证明「逐行生效」）

单测只能证明文件数值对、能上传成功，**证不到「第 i 个球真的读第 i 行」**。
这一步用 `PBR_DUMP` 做 A/B 对拍，10 分钟能跑完：

```powershell
# A：真数据（metallic 按列 0→1，roughness 按行 0.05→1）
$env:PBR_DUMP='rowdata'; .\build\out\Windows_64_Release\PBRSpheres.exe

# B：把 100 行全改成 metallic=0.5 / roughness=0.5（临时改，跑完 git checkout 还原）
#    正则替换 [[rows]] 里的两个字段即可

$env:PBR_DUMP='uniform'; .\build\out\Windows_64_Release\PBRSpheres.exe
```

两次 dump 都是 `pbrdump_<tag>_1280x720_A2BGR10UN.raw`（`A2BGR10` packed：
`R=b0-9` / `G=b10-19` / `B=b20-29`）。逐像素比对：
差异像素占比与最大通道差就是判据。

**本轮实测**：差异 `107204 / 921600`（11.63%），最大通道差 `0.660`，
差异包围盒 `x=[280,999] y=[263,527]` —— 恰好是球阵在屏幕上的投影范围。
⇒ 逐行数据确实进入了每个球自己的着色。

反之若换上统一数据后画面几乎不变，说明行号没进 `MaterialData` 组件、
或全部读到了同一行（先查 §10 表格「实体建出来但颜色全一样」一条）。

> 注意：`pbrdump_*.raw` 落在仓库根目录，验收后删除，别提交。

---

## 10. 已知坑（本轮踩过的）

| 现象 | 原因 | 处理 |
|---|---|---|
| 上传静默失败，日志 `row-size mismatch: expected=1 actual=32` | `view.rows` 是 `const uint8 *`，绑到了按行模板重载（`T = uint8_t`） | 显式 `static_cast<const void *>`；模板里加 `static_assert(sizeof(T) > 1)` |
| 读 TOML 报 `InvalidValue` | 头部缺 `schema = 1` | 数据块文件必须带 `schema` |
| 读 TOML 报 `UnknownKey` | 头部缺 `target`，或写出时用了 `SaveToTomlText`（写出并集 7 字段） | 补 `target`；写出改用 `WriteMaterialDataBlockToml` |
| `OS_TEXT(变量)` 编译不过（`Lxxx` 未定义） | `OS_TEXT` 是 `L##str`，只接受字面量 | 变量一律 `ToOSString(var)` |
| 实体建出来但颜色全一样 | 行号恒为 0（配方级绑定没被实体行覆盖） | 确认 `WithMaterialDataRow` 落在 `WithMaterialDataResource` 之后调用 |