# ASTTooling —— Clang AST 驱动的通用枚举代码生成

这里的工具**不绑定任何业务模块**：输入是一个 C++ `enum class`，输出是一份
「成员数量 / 规范名称 / 稳定下标 / 数量断言」的注册表头。

当前的消费者是 `CMHash`：`enum class Algorithm` 是散列算法的唯一真源，
`CMHash/inc/hgl/util/hash/generated/AlgorithmRegistry.gen.h` 由本工具派生。

## 用法

```powershell
.\run_hashgen.cmd              # 生成并落盘
.\run_hashgen.cmd -Check       # 只校验（CI）
.\run_hashgen.cmd -Only HashAlgorithm
```

或直接调用：

```powershell
python src/Tools/ASTTooling/enum_gen.py --config src/Tools/ASTTooling/enum_gen.toml
```

需要 Python 3.11+（用到 `tomllib`）与 `clang++`（可用 `HGL_CLANG` 指定）。

## 为什么配置里没有成员列表

`enum_gen.toml` 只描述 **AST 无法推断** 的信息：

| 配置项 | 说明 |
|--------|------|
| `header` / `enum` / `namespace` | 去哪里找这个枚举 |
| `output` | 产物路径（提交进仓库） |
| `[[target.display]]` | 对外显示名，**以枚举项名称为键** |

`display` 的键必须真实存在于 AST：枚举项改名而配置没跟着改时，生成阶段
直接报错，不会留下一张「看着正常、其实对不齐」的表。

成员顺序、数量、名称全部来自 AST，因此不存在第二份手工清单。

## 产物形态

```cpp
namespace hgl::util::hash::generated
{
    constexpr size_t kAlgorithmCount = 18;

    constexpr const char *const kAlgorithmNames[kAlgorithmCount] = { "Adler32", ... };

    enum AlgorithmIndex : size_t { ALGORITHM_INDEX_Adler32 = 0, ... };
}

static_assert(size_t(Algorithm::RANGE_SIZE) == generated::kAlgorithmCount, "...");
```

`*_INDEX_*` 让「信息表」可以按枚举项键控初始化，而不是靠行序对齐；
`static_assert` 保证枚举增删后忘记重新生成会在编译期暴露。

## 测试

```powershell
python src/Tools/ASTTooling/test_enum_gen.py
```

不需要 clang：测试只验证「配置 ↔ AST 节点」的约束与渲染结果。
