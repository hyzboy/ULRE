# AssetGen —— 资产对象派生代码生成

把带 `HGL_ASSET_STRUCT` / `HGL_ASSET_CLASS` 注解的 C++ 结构，自动转成 **TOML 读写**、
**属性元数据表**（检查器 GUI / 文档导出）与 **FlatBuffers 二进制格式**。本工具不参与
编译产物，只在需要时手动（或 CI 里）运行。

## 1. 一条规则

> C++ 结构是唯一真源；派生物一律由工具生成，不允许手改。

注解打在**结构上**，字段清单、类型、顺序、偏移、类内缺省值全部由 Clang AST 与
`-fdump-record-layouts-complete` 自动提取 —— **不需要逐字段标注**。

```cpp
#include <hgl/asset/AssetAnnotation.h>

struct HGL_ASSET_STRUCT BuiltinSphereParams
{
    uint32_t slices = 32;                     // 自动序列化，缺省值取类内初始化

    HGL_PROPERTY(EditAnywhere, Category = "Shape", Range(1, 512),
                 Tooltip = "经线分段数")
    uint32_t rings = 16;

    HGL_PROPERTY(Transient)                   // 派生缓存，不进 TOML / FBS
    uint32_t cached_hash = 0;
};
```

## 2. 产出

| 产物 | 位置 | 说明 |
|---|---|---|
| TOML 读写 | `inc/hgl/asset/generated/<Group>Toml.gen.h` | `Read<Struct>(toml::value, T&)` / `Write<Struct>(const T&)` |
| 属性元数据 | `inc/hgl/asset/generated/<Group>Meta.gen.h` | `PropertyMetaInfo[]` / `ClassMetaInfo` / `GetClassMeta(&obj)` |
| FlatBuffers schema | `inc/hgl/asset/generated/<group>_schema.fbs` | 发行版二进制格式的契约 |
| FlatBuffers C++ | `inc/hgl/asset/generated/<group>_schema_generated.h` | `flatc` 产物 |
| FlatBuffers 桥 | `inc/hgl/asset/generated/<Group>Flat.gen.h` | `Encode<Struct>` / `Decode<Struct>` |

## 3. 读入语义

与 `BufferToml.gen.h` 完全一致：

- **键缺失** = 保留 C++ 类内初始值（于是生成器不需要解析 C++ 初始化表达式，缺省值
  永远与声明同步）；
- **键存在但类型 / 长度不符** = 解析失败（返回 `false`，由调用方报出文件与字段）；
- **枚举**同时接受字符串名（`"Normal"`）与整数值（`1`），写出时优先用名字。

## 4. 属性标签

| 标签 | 含义 | 谁消费 |
|---|---|---|
| `EditAnywhere` | 任何实例可编辑 | 检查器 |
| `EditDefaultsOnly` | 只能在原型/资产上编辑 | 检查器 |
| `VisibleAnywhere` | 只读展示 | 检查器 |
| `ReadOnly` | 编辑器只读 | 检查器 |
| `EditOnly` | 仅编辑器使用，发行版可剔除 | 发行版裁剪 |
| `Transient` | 不参与序列化 | 生成器 |
| `Category = "X"` | 检查器分组 | 检查器 |
| `DisplayName = "X"` | 检查器显示名 | 检查器 |
| `Tooltip = "X"` | 悬浮说明（缺省取 `///<` 尾注释） | 检查器 / 文档 |
| `Range(min, max)` | 软范围（滑块） | 检查器 |
| `Clamp(min, max)` | 硬范围（越界拒绝） | 检查器 |
| `Step = v` | 拖动步长 | 检查器 |
| `AssetRef = "geo"` | 该字符串是资产引用 | 检查器（资产选择器） |
| `Key = "k"` | TOML 键名覆盖 | 生成器 |

## 5. 用法

```powershell
# 生成（写入）
python src/Tools/AssetGen/asset_gen.py --repo-root . --clang "C:\Program Files\LLVM\bin\clang++.exe" --write

# 只比对不落盘（CI / 门禁用；有差异返回 1）
python src/Tools/AssetGen/asset_gen.py --repo-root . --clang "C:\Program Files\LLVM\bin\clang++.exe" --check
```

依赖目录：默认 `inc`、`CMCoreType/inc`、`CMCore/inc`、`CMMath/inc`；第三方头（glm、
flatbuffers）用环境变量追加：

```
HGL_ASSETGEN_INCLUDE_DIRS=D:\vcpkg\installed\x64-windows\include
```

（多个用 `;` 分隔），或重复传 `--include-dir`。

## 6. 配置 `asset_gen.toml`

只登记**分组与产物位置**，绝不描述字段或布局：

```toml
# 外部定义的枚举（来自第三方头，例如 Vulkan）：按整数序列化，不生成名字表
integer_enums = ["VkFormat"]

[group.Geometry]
namespace     = "hgl::graph"                            # 结构所在命名空间
scan          = ["inc/hgl/graph/asset"]                 # 扫描哪些目录找注解
toml          = "inc/hgl/asset/generated/GeometryToml.gen.h"
meta          = "inc/hgl/asset/generated/GeometryMeta.gen.h"
fbs           = "inc/hgl/asset/generated/geometry_schema.fbs"
fbs_generated = "inc/hgl/asset/generated/geometry_schema_generated.h"
flat          = "inc/hgl/asset/generated/GeometryFlat.gen.h"

[struct.BuiltinSphereParams]
asset_kind = "geo"                                       # 元数据里的资产类别
```

**新增资产结构**：加注解 → 在 `asset_gen.toml` 的某个 `[group.*]` 的 `scan` 目录里 →
跑工具。同一分组内重名会直接报错（生成函数名会冲突），不做静默改名。

## 7. 支持的类型

| C++ | TOML | FlatBuffers |
|---|---|---|
| `bool` / `uint8..64` / `int8..64` / `float` / `double` | 标量 | 同宽标量 |
| `AnsiString` / `U8String` / `OSString` | 字符串（UTF-8） | `string` |
| `Vector2f/3f/4f`、`Color3f/4f`、`Vector2u..`、`Vector2i..` | 数组 | 定长 `struct` |
| `enum class` | 字符串名或整数 | `int` |
| 另一个 `HGL_ASSET_STRUCT` | 子表 | `table` |
| `T[N]`（定长数组） | 数组 | 包装 `struct` |
| `hgl::ValueArray<T>` | 数组 | `[T]` |
| `hgl::ManagedArray<T>` | 表数组 | `[table]` |

不在表里的类型会**直接报错**（拒绝静默降级），而不是悄悄生成错误的代码。
