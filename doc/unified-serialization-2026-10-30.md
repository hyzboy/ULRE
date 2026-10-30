# 统一序列化框架（Unified Serialize Archive）

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
<Group>Codec.gen.h            FlatBuffers 布局描述表 + 格式门面
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
| `inc/hgl/serialize/FlatCodec.h` | `Value ↔ FlatBuffers`（描述表驱动，两趟构建） |

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

枚举在值树里以**名字字符串**保存（`EnumTraits<T>::ToName`），因此 TOML 输出可读；
FlatBuffers 侧通过描述表挂的 `EnumFromNameFn` / `EnumToNameFn` 双向转换，
整数写法的历史数据仍可读。

## 6. 校验

通用表没有 flatc 生成的 `Verify()` 成员，`FlatCodec` 无法复现 schema 级校验。
需要强校验时调用方先用已有生成头的根类型跑一次：

```cpp
flatbuffers::Verifier verifier(data, size);
if (!verifier.VerifyBuffer<hgl::asset::schema::SceneSpec>(nullptr)) return false;
// 再走通用解码
hgl::serialize::fb::DecodeFromBuffer(data, size, SceneSpec_fbs, node);
```

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
// TOML
hgl::graph::SceneSpec spec;
asset_codec::ReadSceneSpec(toml_value, spec);          // 缺键保留缺省值
const toml::value out = asset_codec::WriteSceneSpec(spec);

// FlatBuffers
const auto root = asset_codec::EncodeSceneSpec(fbb, spec);
asset_codec::DecodeSceneSpec(root, spec);
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
