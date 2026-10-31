# BufferGen —— UBO/SSBO 派生代码生成

把 `inc/hgl/graph/ubo/*.h` 与 `inc/hgl/graph/ssbo/*.h` 里的**整结构注解**转成
GLSL、TOML 读写与 FlatBuffers schema。本工具**不**参与编译产物，只在需要时手动
（或 CI 里）运行。

## 1. 一条规则

> C++ 结构是唯一真源；派生物一律由工具生成，不允许手改。

结构体上打一个整结构注解即可，**不需要逐字段标注**：

```cpp
#include <hgl/graph/BufferAnnotation.h>

struct alignas(64) HGL_SSBO_BDA LightRow
{
    uint32_t type   = 0;
    uint32_t flags  = 0;
    float    _pad0[2] = { 0.0f, 0.0f };
    Vector4f direction_intensity = Vector4f(0.0f, 0.0f, 1.0f, 1.0f);
    // ...
};
```

| 注解 | 含义 | GLSL 侧 |
|---|---|---|
| `HGL_UBO` | UBO 参数块（std140） | `struct` |
| `HGL_UBO_BDA` | 同上，BDA 无绑定无集 | `struct` + `readonly buffer …Ref` |
| `HGL_SSBO` | SSBO 行/数组元素（std430 / scalar） | `struct` |
| `HGL_SSBO_BDA` | 同上，BDA，**只读** | `struct` + `readonly buffer …` |
| `HGL_SSBO_ROW` | 同上，BDA，**可写**（arena 行） | `struct` + `buffer …` |
| `HGL_GPU_STRUCT` | 嵌套 GPU 结构（被上面引用，自成块） | `struct` |
| `HGL_CPU_ONLY` | 在独占目录但不上 GPU | （不产出） |

非 Clang 编译器下全部退化为空宏，零警告、零 ABI 影响。

## 2. 产出

| 产物 | 位置 | 说明 |
|---|---|---|
| GLSL 结构本体 | `ShaderLibrary/module/descriptor/ubo/*.glsl`、`ShaderLibrary/module/descriptor/ssbo/*.glsl` | 每个结构（或一组相关结构）一个文件，带 include guard |
| TOML 读写 | `inc/hgl/graph/generated/BufferToml.gen.h` | `Read<Struct>(toml::value, T&)` / `Write<Struct>(const T&)` |
| FlatBuffers schema | `inc/hgl/graph/generated/buffer_schema.fbs` | `table`（块/行）/ `struct`（嵌套定长） |

**不生成**「地址从哪来」：`#define shadow_info ShadowInfoRef(world_addresses.addr_shadow)`
这类寻址胶水是设计决策，写在 `ShaderLibrary/module/descriptor/ubo/scene_ubo_bindings.glsl` 里手工维护。
拆开的好处是重新生成永远不会覆盖寻址逻辑。

## 3. 数据来源

| 事实 | 来源 |
|---|---|
| 结构是否有注解、是哪一类 | 头文件文本扫描（`HGL_*` 宏） |
| 字段名 / 声明类型 / 顺序 | `clang++ -Xclang -ast-dump=json -Xclang -ast-dump-filter=<Name>` |
| 字段偏移 / `sizeof` / `alignof` | `clang++ -Xclang -fdump-record-layouts-complete` |
| 字段默认值 | **不解析**：生成的 TOML 读入以 `T{}` 起始，键缺失即保留 C++ 类内初始值 |

最后一条是关键简化：默认值永远与 C++ 声明同步，不需要在工具里复刻初始化表达式。

## 4. 用法

```powershell
# 生成（写入）
python src/Tools/BufferGen/buffer_gen.py --repo-root . --clang "C:\Program Files\LLVM\bin\clang++.exe" --write

# 只比对不落盘（CI / 门禁用；有差异返回 1）
python src/Tools/BufferGen/buffer_gen.py --repo-root . --clang "C:\Program Files\LLVM\bin\clang++.exe" --check
```

依赖目录：
- 默认 `inc`、`CMCoreType/inc`、`CMMath/inc`；
- 第三方头（glm）用环境变量追加：
  `HGL_BUFFERGEN_INCLUDE_DIRS=D:\vcpkg\installed\x64-windows\include`
  （多个用 `;` 分隔），或重复传 `--include-dir`。

## 5. 配置 `buffer_gen.toml`

只登记**命名与产物位置**，绝不描述布局：

```toml
[[struct]]
name = "LightRow"                 # C++ 结构名
kind = "ssbo"                     # ubo | ssbo | struct | cpu_only
file = "module/descriptor/ssbo/light_row.glsl"       # 相对 ShaderLibrary/
glsl_struct = "LightRowData"      # GLSL 普通结构名
glsl_ref = "LightRowBufferRef"    # GLSL buffer_reference 块名
ref_member = "lights"             # 块体为数组时的成员名
ref_array = 1                     # 1 = 块体是 <glsl_struct> <ref_member>[]
ref_align = 16                    # buffer_reference_align（缺省 16）
note = "…"                        # 写进生成文件头部
```

`ref_align` 有意与 `alignof(T)` 解耦：BDA 取址在 C++ 侧只承诺 16B
（`GetBufferDeviceAddressAligned16`），照抄 `alignas(64)` 会声明一个兑现不了的取址承诺。

**新增结构**：加注解 → 在 `buffer_gen.toml` 登记 → 跑工具。未登记会直接报错
（拒绝猜命名），而不是悄悄生成一个名字不对的文件。

## 6. 校验

`--check` 已被 CI 门禁使用（见 `scripts/ci`）。它验证的是「仓库里的 GLSL / TOML /
FBS 与当前 C++ 结构完全一致」——任何一侧手改都会红。

字段级正确性另有运行期门禁兜底（`ShaderResourceSchemaRegressionGate` 的
`S.global-addresses-struct-parity` / `W.world-addresses-struct-parity` /
`W.light-row-struct-parity`），它们独立解析 C++ 与 GLSL 文本后逐字段对拍。
