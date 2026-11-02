# 统一序列化框架（Unified Serialize Archive）

> **落地状态（2026-11）**：本文描述的框架已全部实现，且比当初的设计更进一步 ——
> 生成器现在**只产两份与格式无关的文件**（`<Group>Meta.gen.h` / `<Group>Serialize.gen.h`），
> 不再产出 `<Group>Toml.gen.h`、`<Group>Flat.gen.h`、`<Group>Codec.gen.h`、`.fbs`
> 与 `flatc` 头。下文凡出现这些名字，都是「改造前」的历史形态（保留作背景说明）；
> 现行口径以 `src/Tools/AssetGen/README.md` 与 `hgl/serialize/` 为准。
> 4 个资产文件（场景 / 几何 / 图元 / 材质配方）已全部改走
> `FromTomlText` → `Value` → `ReadFromValue` 的单一路径，源码里不再出现 `toml::value`。

## 1. 问题

改造前，AssetGen 为同一个资产结构生成 **两套** 编解码实现：

```
inc/hgl/asset/generated/<Group>Toml.gen.h   TOML 读写（约 24 KB / 组）
inc/hgl/asset/generated/<Group>Flat.gen.h   FlatBuffers 编解码（约 18 KB / 组）
```

两份代码各自重复「字段清单 + 类型映射 + 缺省/校验语义」，代价是：

- 代码量随「结构数 × 格式数」增长（$O(N \times M)$）；
- 任何语义调整都要改两处，且极易产生细微分歧；
- 再加 JSON / Protobuf 就是第三份、第四份。

更关键的是**物理约束不相容**：TOML/JSON 是「键 → 值」字典，可以边遍历边写；
FlatBuffers 的缓冲区**从后往前**生长，父表的子串 / 子表 / 向量必须在
`StartTable()` 之前全部建好。流式写入器不可能同时满足两者。

## 2. 方案

把「语义」与「布局」彻底分开，中间插入一层通用值树：

```
C++ 结构声明（唯一真源，HGL_ASSET_STRUCT + HGL_PROPERTY）
        │  Clang AST
        ▼
<Group>Serialize.gen.h        唯一一份字段遍历：StructSerializer<T>::Do(Archive&, Self&)
<Group>Meta.gen.h             属性元数据（键名表 / 枚举名表 / GUI 元数据）
        │
        ▼
hgl::serialize::Archive  ──►  hgl::serialize::Value（有序值树）
                                    │
                 ┌──────────────────┴──────────────────┐
                 ▼                                     ▼
        TomlCodec（Value ↔ toml::value）    FlatCodec（Value ↔ FlatBuffers）
```

- **遍历与校验只发生一次**，写在 `Serialize.gen.h` 里；
- 格式后端只做「值树 ↔ 自家表示」的机械搬运；
- 新增 JSON 后端时**不需要重新生成任何资产代码**，只加一个
  `hgl/serialize/JsonCodec.h`。

`Do` 的签名是模板 `template<typename Self> static void Do(Archive &, Self &)`，
`Self` 在写方向被推导成 `const T`，因此同一份字段列表既能写回也能只读输出，
不必再复制一遍「const 版本」。

## 3. 头文件布局

> **物理位置（2026-12 下沉完成）**：`hgl/serialize` 与 `hgl/asset` 已整体迁到公共库
> `CMAssetsManage`。`#include <hgl/serialize/...>` / `<hgl/asset/...>` 形态**未变**
> （`use_cm_module` 把 `CMAssetsManage/inc` 并入全局头搜索路径），消费者源码零改动。
>
> | 归属 | 物理目录 | 文件 |
> |---|---|---|
> | `CMAssetsManage`（公共库） | `CMAssetsManage/inc/hgl/serialize/` | `Value.h`、`Container.h`、`EnumMeta.h`、`Archive.h`、`Traits.h`、`MetaSchema.h`、`TomlCodec.h`、`FlatCodec.h`、`ArchiveFile.h` |
> | `CMAssetsManage`（公共库） | `CMAssetsManage/inc/hgl/asset/` | `AssetObject.h`、`AssetAnnotation.h`（`HGL_ASSET_STRUCT` / `HGL_PROPERTY` 宏） |
> | ULRE 主仓 | `inc/hgl/asset/generated/` | `*.gen.h`（AssetGen 产物，ULRE 领域结构专属） |
>
> 第三方依赖全部改由公共库自己声明：`TomlCodec.h` 用 vcpkg 的 **toml11 4.4.0**
> （`#include <toml.hpp>`，此前 vendored 在 `inc/toml/toml.hpp` 的那份已删除），
> `FlatCodec.h` 用 vcpkg 的 **flatbuffers**。`CMAssetsManage/src/CMakeLists.txt`
> 以 `find_package(... CONFIG REQUIRED)` + `target_link_libraries(... PUBLIC ...)`
> 把它们传播给消费方。依赖方向：
> `CMCoreType ← CMMath ← hgl/serialize ← hgl/asset ← ULRE 领域资产`。

| 文件 | 职责 |
|---|---|
| `hgl/serialize/Value.h` | 通用值树：Null / Bool / Int / UInt / Float / Double / String / Array / **有序** Table |
| `hgl/serialize/Traits.h` | 标量、HGL 字符串、枚举名、数学向量；`EnumTraits<T>` / `StructSerializer<T>` 扩展点 |
| `hgl/serialize/Container.h` | HGL 容器的四类抽象形态与降级适配 |
| `hgl/serialize/Archive.h` | 统一游标：`ar.Field(id, name, v)`，读写同源 |
| `hgl/serialize/EnumMeta.h` | `HGL_ENUM_BEGIN` / `HGL_ENUM_ITEM` 登记块与 `EnumMeta<T>` 名字↔值真源 |
| `hgl/serialize/MetaSchema.h` | `ClassMetaInfo` 自省入口（`AssetObject` 的属性表） |
| `hgl/asset/AssetObject.h` | `PropertyMetaInfo` / `ClassMetaInfo` / `PropertyValue` + `AssetState` |
| `hgl/asset/AssetAnnotation.h` | `HGL_ASSET_STRUCT` / `HGL_ASSET_CLASS` / `HGL_PROPERTY` 注解宏 |
| `hgl/serialize/TomlCodec.h` | `Value ↔ toml::value`（含文本解析/输出） |
| `hgl/serialize/FlatCodec.h` | `Value ↔ FlatBuffers`（`ClassMetaInfo` 驱动，两趟构建，自带 Verifier） |
| `hgl/serialize/ArchiveFile.h` | **文件门面**：按文件名后缀选后端，资产落盘/读盘唯一入口 |

## 4. 容器抽象形态与后端降级

| 抽象形态 | HGL 容器 | 文本后端 | 二进制后端 |
|---|---|---|---|
| Sequence | `ValueArray` / `PtrArray` / `ManagedArray` | 数组 | `vector` |
| Mapping | `OrderedMap` / `UnorderedMap` | 字符串键 → 原生表；其它键 → `{key,value}` 条目数组 | 表向量 |
| Set | `OrderedSet` / `FlatOrderedSet` | 数组 | `vector` |
| Tree | `FlatTreeContainer` | `[{ parent = -1, data = {…} }]` | 节点表向量 |

树形容器是双拓扑的，序列化时降级为「扁平节点数组 + parent 索引」，读回时按
数组顺序 `CreateNode(value, parent)` 重建层级拓扑，线性拓扑由容器惰性重算。

> 注：`hgl::ArrayList` 在 CMCore 中不存在，因此适配层覆盖的是上表所列的实际容器。
> 定长 C 数组（`T[N]`）暂不支持，请改用 `ValueArray` / `ManagedArray`。

## 5. 编解码语义

| 方向 | 规则 |
|---|---|
| 读（TOML） | 字段缺失 ⇒ 保留 C++ 类内初始值；类型/长度不符 ⇒ 失败并留下字段名 |
| 读（FlatBuffers） | **完整快照**：标量字段一律覆盖（即使等于 schema 缺省值、未写进缓冲）；指针类字段缺失则保留类内初始值 |
| 写 | 输出全部字段，保证 FlatBuffers 是完整快照 |

`Presence<T>` 字段是「完整快照」的例外：它记录的是「作者是否显式设置过」，快照化会丢掉
这一位。`Archive::Field(id, name, value)` 在写方向直接跳过 `present == false` 的字段，
读方向按「值树里有没有这个键」回填 `present` —— 后端与生成器都不需要自己判断三态。

枚举在值树里以**名字字符串**保存（`EnumTraits<T>::ToName`），因此 TOML 输出可读；
FlatBuffers 侧由 `FlatCodec` 按 `ClassMetaInfo` 里的枚举宽度转成整数，
整数写法的历史数据仍可读（`integer_enums` 登记的类型则只走整数）。

## 6. 校验

FlatBuffers 侧不需要 `.fbs`，也不需要 `flatc`：`FlatCodec` 自带与 `flatc` `Verify()`
等价的校验（偏移、向量长度、字符串范围全部落在缓冲内），入口是
`hgl::serialize::fb::VerifyBuffer<T>` / `VerifyAndDecodeFlatBuffers<T>`。
键名严格性由 `ClassMetaInfo` 驱动：`hgl::serialize::ValidateKeysByMeta` 递归下钻
一层表 / 表数组 / 子表，发现未登记键就报出分层路径，因此资产文件里不需要任何手写
键名白名单。

## 7. 使用示例

声明（唯一真源）：

```cpp
struct HGL_ASSET_STRUCT SceneTransformSpec
{
    hgl::math::Vector3f position{0.0f};
    hgl::math::Vector3f rotation_axis{0.0f, 0.0f, 1.0f};
    float rotation_angle_deg = 0.0f;
    hgl::math::Vector3f scale{1.0f};
};
```

生成（`SceneSerialize.gen.h`）：

```cpp
template<> struct StructSerializer<hgl::graph::SceneTransformSpec>
{
    static constexpr bool defined = true;

    template<typename Self>
    static void Do(Archive &ar, Self &v)
    {
        ar.Field(1, "position", v.position);
        ar.Field(2, "rotation_axis", v.rotation_axis);
        ar.Field(3, "rotation_angle_deg", v.rotation_angle_deg);
        ar.Field(4, "scale", v.scale);
    }
};
```

使用：

```cpp
// 作者格式（TOML）：先转成自有值树，再按生成的字段遍历读入
hgl::serialize::Value root;
hgl::serialize::FromTomlText(text, len, root);

hgl::graph::SceneSpec spec;                       // 缺键保留类内缺省值
hgl::serialize::Archive ar;
hgl::serialize::ReadFromValue(root, spec, &ar);   // 失败时 ar.FailedPath() 给出全路径

// 发行版格式（FlatBuffers）：同一个 spec，两行搞定
hgl::serialize::fb::EncodeToFlatBuffers<SceneSpec>(fbb, spec);
hgl::serialize::fb::DecodeFromFlatBuffers<SceneSpec>(data, size, spec);
```

键名严格性：

```cpp
AnsiString bad_path, bad_key;
if (!hgl::serialize::ValidateKeysByMeta(root, Class_SceneSpec, AnsiString(), bad_path, bad_key))
    Report(bad_path, bad_key);        // 未登记的键 ⇒ 分层路径 + 键名
```

### 7.1 落盘：`ArchiveFile.h`

上一节的编解码是**内存**动作。文件读写全部收敛到 `ArchiveFile.h`，调用方只给
文件名，**不**自己判断用哪个后端：

```cpp
// 资产对象（多态）：派生类只实现 Serialize(Archive&)，这里按后缀选后端
hgl::graph::EnvironmentAsset asset(spec);
hgl::serialize::SaveAssetObjectToFile(asset, OS_TEXT("sky34_cube_ibl.env"));        // FlatBuffers
hgl::serialize::SaveAssetObjectToFile(asset, OS_TEXT("sky34_cube_ibl.env.toml"));   // TOML

hgl::graph::EnvironmentAsset back;
hgl::serialize::LoadAssetObjectFromFile(back, OS_TEXT("sky34_cube_ibl.env"));

// 值语义结构：同一套后端，模板化
hgl::serialize::SaveStructToFile(spec, OS_TEXT("probe.env"));
hgl::serialize::LoadStructFromFile(OS_TEXT("probe.env"), spec);

// 不落盘：文本 / 字节缓冲
hgl::serialize::SaveAssetObjectToText(asset, out_text);
hgl::serialize::LoadAssetObjectFromBytes(asset, data, size);
```

| 入口 | 说明 |
|---|---|
| `ResolveArchiveFormatByExtension()` | `.toml` ⇒ TOML，其余**带**后缀 ⇒ FlatBuffers，**无**后缀 ⇒ 直接失败（不猜） |
| `WriteAssetObjectValue()` / `ReadAssetObjectValue()` | 多态核心：`obj.Serialize(ar)` 由虚函数分派，字段表来自 `GetClassMetaInfo()` |
| `SaveAssetObjectToText/Bytes/File`、`LoadAssetObjectFrom*` | 资产级门面 |
| `SaveStructToFile<T>` / `LoadStructFromFile<T>` | 值语义结构门面（`StructSerializer<T>` 必须已生成） |
| `SaveValueToBytes()` / `LoadValueFromBytes()` | 值树 ⇄ FlatBuffers 缓冲（读侧**先** `VerifyBuffer`） |
| `DescribeArchiveFailure()` | 把 `path=` / `field=` / `detail=` 拼成一行可读原因 |

红线：**不要在资产类里加 `SaveToToml` / `SaveToFlat` / `LoadFromToml` / `LoadFromFlat`
这类格式专用虚接口**。加一个后端要改一次资产派生代码，等于把「格式无关」这层
结构又破了。新增后端只需在 `ArchiveFile.h` 里多一个 `case`，资产代码零改动。

### 7.2 内置预设：产物在仓库，生成器在工具工程

内置预设（`ShaderLibrary/asset/environment/*.env`）是**作者侧产物**，引擎只读。
生成器因此**不在**运行时库里：

| 位置 | 角色 |
|---|---|
| `src/Tools/BuiltinAssets/`（`BuiltinAssetExport`） | 离线生成器：预设数据 + `PresetWriter`（拼路径 + 调 `ArchiveFile.h`） |
| `ShaderLibrary/asset/environment/*.{env,env.toml}` | 产物，运行时唯一入口 |
| `hgl::graph::EnvironmentAsset` | 只提供字段表与 `Serialize()`，**不含任何内置数据** |

生成器仍只走统一通路（`AssetObject::Serialize()` → `Archive` → Codec），所以同一份预设
「两种格式」不是两套代码：两次 `SaveAssetObjectToFile()`，差别只在文件名后缀。

```powershell
# 改完预设数据后重新生成（工作目录 = 仓库根）
BuiltinAssetExport --out ShaderLibrary/asset
```

`TestEnvironmentCodec` 第 9 节校验仓库里那 5 份预设「TOML 与 FlatBuffers 同源且字段
一致」—— 忘记重跑生成器会直接被这条门禁指出。未来其它内置元素（几何 / 材质预设…）
在 `BuiltinAssetExport` 的 `kGroups` 里加一组即可。

## 8. 门禁

| 目标 | 覆盖内容 |
|---|---|
| `TestSerializeArchive` | 值树、四类容器、TOML 往返与文本闭环、FB 往返、缺键/类型不符语义 |
| `TestAssetReflection` | 真实资产结构的 TOML + FlatBuffers 往返、元数据表与布局一致 |
| `TestEnvironmentCodec` | 环境规格双格式往返 + `ArchiveFile.h` 门面（文本/字节/文件、无后缀拒绝）+ 仓库内置 5 份预设两份格式一致 |
| `TestEnvironmentAssetURI` | `env:` URI、目录装载、二进制 `.env` 与缓存命中 |
| `TestSceneFile` / `TestGeometryAssetFile` / `TestPrimitiveAssetFile` | 文件级读写与语义校验 |
| `asset_gen.py --check` | 「改了 C++ 结构但忘记重新生成」 |
| `BuiltinAssetExport`（工具工程） | 「改了内置预设数据但忘记重新导出产物」 |

## 9. 后续

- JSON 后端（`JsonCodec.h`）：只需值树 ↔ JSON，无需动生成器；
- `Mapping` 的 FlatBuffers 表示（按键排序的表向量）：需要在描述表新增
  `VectorTable` + key 字段标记；
- 定长 C 数组（`T[N]`）：可在 `Container.h` 增加 `FixedArray` 形态。
