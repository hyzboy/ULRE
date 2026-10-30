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
| 字段遍历（唯一一份） | `inc/hgl/asset/generated/<Group>Serialize.gen.h` | `EnumTraits<T>` + `StructSerializer<T>::Do(Archive&, Self&)`；与格式无关 |
| 布局描述 + 格式门面 | `inc/hgl/asset/generated/<Group>Codec.gen.h` | FlatBuffers 的 `TableDesc`/`FieldDesc` 表 + `Read*/Write*/Encode*/Decode*` |
| 属性元数据 | `inc/hgl/asset/generated/<Group>Meta.gen.h` | `PropertyMetaInfo[]` / `ClassMetaInfo` / `GetClassMeta(&obj)` |
| FlatBuffers schema | `inc/hgl/asset/generated/<group>_schema.fbs` | 发行版二进制格式的契约 |
| FlatBuffers C++ | `inc/hgl/asset/generated/<group>_schema_generated.h` | `flatc` 产物 |

「字段清单/类型/语义」只生成**一次**（`Serialize.gen.h`）；`Codec.gen.h` 里的描述表是
**纯数据**，格式门面只是把「值树 ↔ 格式」的两步接起来。新增格式后端（如 JSON）时
只需在 `inc/hgl/serialize/` 加一个 codec，**不需要重新生成本工具的任何产物**。
详见 `doc/unified-serialization-2026-10-30.md`。

## 3. 读入语义

**TOML —— 作者格式，增量语义**：

- **键缺失** = 保留 C++ 类内初始值（于是生成器不需要解析 C++ 初始化表达式，缺省值
  永远与声明同步）；
- **键存在但类型 / 长度不符** = 解析失败（返回 `false`，由调用方报出文件与字段）；
- **枚举**同时接受字符串名（`"normal_tangent"`）与整数值（`1`），写出时优先用名字。

**FlatBuffers —— 发行版格式，完整快照**：schema 里出现的每个标量字段都会被解码写回
（即使它等于 schema 缺省值、因而没有实际写进缓冲）；`Transient` 字段两边都不出现，
`EditOnly` 字段只进入作者侧 TOML、不进入发行版 FlatBuffers。`EditOnly` 不会改变 C++
结构布局；它是序列化策略标记，不是条件编译。
两种格式的分工是刻意的：TOML 给人改（缺键即缺省），FBS 给机器读（全量快照）。

## 4. 属性标签

| 标签 | 含义 | 谁消费 |
|---|---|---|
| `EditAnywhere` | 任何实例可编辑 | 检查器 |
| `EditDefaultsOnly` | 只能在原型/资产上编辑 | 检查器 |
| `VisibleAnywhere` | 只读展示 | 检查器 |
| `ReadOnly` | 编辑器只读 | 检查器 |
| `EditOnly` | 编辑器 TOML 保留；发行版二进制不包含 | AssetGen |
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

### 一键脚本（推荐，自动探测路径）

```cmd
# 重新生成并落盘（Windows CMD / 双击直接运行）
run_assetgen.cmd

# 比对校验模式（不修改文件）
run_assetgen.cmd -Check
```
更多详情与操作指引参见 [`doc/assetgen-rebuild-guide.md`](../../../doc/assetgen-rebuild-guide.md)。

### 手动执行命令

```powershell
# 生成（写入）—— 改完 C++ 结构 / 注解 / asset_gen.toml 之后跑这个
python src/Tools/AssetGen/asset_gen.py --repo-root . `
  --clang "C:\Program Files\LLVM\bin\clang++.exe" `
  --flatc "D:\vcpkg\installed\x64-windows\tools\flatbuffers\flatc.exe" `
  --include-dir "D:\vcpkg\installed\x64-windows\include" `
  --write

# 只比对不落盘（CI / 门禁用；有差异返回 1）
python src/Tools/AssetGen/asset_gen.py --repo-root . `
  --clang "C:\Program Files\LLVM\bin\clang++.exe" `
  --flatc "D:\vcpkg\installed\x64-windows\tools\flatbuffers\flatc.exe" `
  --include-dir "D:\vcpkg\installed\x64-windows\include" `
  --check
```

生成器需要 Clang、Python 3.11+（较旧 Python 需安装 `tomli`）与 FlatBuffers `flatc`。
`--check` 会把当前 `.fbs` 在临时目录运行 `flatc` 并对比提交的生成头；一致性校验也因此
能发现过期的 `_schema_generated.h`。生成产物已提交，普通 MSVC 构建不需要 Clang 或
`flatc`；编译 Codec 头与往返测试时仍需要 FlatBuffers 头文件与 CMake package。
可通过 `--clang`、`--flatc`、`--include-dir` 显式指定工具和头文件目录；也支持
`HGL_CLANG`、`HGL_FLATC` 与 `HGL_ASSETGEN_INCLUDE_DIRS` 环境变量。

其他开关：`--only <Group>` 只处理一个分组（调试用）；`--no-flatc` 跳过 `flatc`，
只更新 `.fbs` 与 `Serialize/Codec/Meta` 头（不重生成 `_schema_generated.h`）。

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

# 枚举的 TOML 拼写别名（按枚举值声明顺序）。解析时别名与 C++ 名都接受
# （大小写不敏感），写出时优先用别名 —— 既不改已有 TOML 拼写，也不用改 C++ 命名。
[[enum]]
name = "NTBType"
aliases = ["none", "normal", "normal_tangent", "normal_tangent_bitangent"]

[group.Geometry]
namespace     = "hgl::graph"                            # 结构所在命名空间
scan          = ["inc/hgl/graph/asset/GeometryAssetSpec.h"]  # 目录或单个头文件
serialize     = "inc/hgl/asset/generated/GeometrySerialize.gen.h"  # 唯一一份字段遍历
codec         = "inc/hgl/asset/generated/GeometryCodec.gen.h"      # 布局描述 + 格式门面
meta          = "inc/hgl/asset/generated/GeometryMeta.gen.h"
fbs_namespace = "hgl.asset.schema"
fbs           = "inc/hgl/asset/generated/geometry_schema.fbs"
fbs_generated = "inc/hgl/asset/generated/geometry_schema_generated.h"

[struct.BuiltinSphereParams]
asset_kind = "geo"                                       # 元数据里的资产类别
```

`scan` 的每一项可以是**目录**（递归扫描）也可以是**单个头文件**。按文件切分是为了让
同一个目录里的不同资产规约落到不同分组，生成文件不至于混在一起。

**新增资产结构**：加注解 → 把所在头文件加进某个 `[group.*]` 的 `scan` → 跑工具。
同一分组内重名会直接报错（生成函数名会冲突），不做静默改名。

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
