# 统一序列化框架（Unified Serialize Archive）

> **落地状态（2026-11）**：本文描述的框架已全部实现，且比当初的设计更进一步 ——
> 生成器现在**只产两份与格式无关的文件**（`<Group>Meta.gen.h` / `<Group>Serialize.gen.h`），
> 不再产出 `<Group>Toml.gen.h`、`<Group>Flat.gen.h`、`<Group>Codec.gen.h`、`.fbs`
> 与 `flatc` 头。下文凡出现这些名字，都是「改造前」的历史形态（保留作背景说明）；
> 现行口径以 `src/Tools/AssetGen/README.md` 与 `inc/hgl/serialize/` 为准。
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

| 文件 | 职责 |
|---|---|
| `inc/hgl/serialize/Value.h` | 通用值树：Null / Bool / Int / UInt / Float / Double / String / Array / **有序** Table |
| `inc/hgl/serialize/Traits.h` | 标量、HGL 字符串、枚举名、数学向量；`EnumTraits<T>` / `StructSerializer<T>` 扩展点 |
| `inc/hgl/serialize/Container.h` | HGL 容器的四类抽象形态与降级适配 |
| `inc/hgl/serialize/Archive.h` | 统一游标：`ar.Field(id, name, v)`，读写同源 |
| `inc/hgl/serialize/TomlCodec.h` | `Value ↔ toml::value`（含文本解析/输出） |
| `inc/hgl/serialize/FlatCodec.h` | `Value ↔ FlatBuffers`（`ClassMetaInfo` 驱动，两趟构建，自带 Verifier） |

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

## 8. 门禁

| 目标 | 覆盖内容 |
|---|---|
| `TestSerializeArchive` | 值树、四类容器、TOML 往返与文本闭环、FB 往返、缺键/类型不符语义 |
| `TestAssetReflection` | 真实资产结构的 TOML + FlatBuffers 往返、元数据表与布局一致 |
| `TestSceneFile` / `TestGeometryAssetFile` / `TestPrimitiveAssetFile` | 文件级读写与语义校验 |
| `asset_gen.py --check` | 「改了 C++ 结构但忘记重新生成」 |

## 9. 后续

- JSON 后端（`JsonCodec.h`）：只需值树 ↔ JSON，无需动生成器；
- `Mapping` 的 FlatBuffers 表示（按键排序的表向量）：需要在描述表新增
  `VectorTable` + key 字段标记；
- 定长 C 数组（`T[N]`）：可在 `Container.h` 增加 `FixedArray` 形态。
