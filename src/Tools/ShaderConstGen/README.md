# ShaderConstGen —— C++ ↔ GLSL 数值契约生成器

把「同一批数字在 CPU 与 GLSL 两侧必须逐一相等」这件事，从**人工同步**改成
**单源 + 自动派生 + 编译期对拍**。

## 为什么需要它

`ShaderLibrary` 里与 C++ 对应的数字此前有三种维护方式，一种比一种容易错：

| 方式 | 例子 | 出错后果 |
|------|------|----------|
| 单功能 C++ 生成器 | `descriptor_macros.glsl`、`interstage_locations.glsl` | 相对安全，但每加一类常量就要再写一个可执行文件 |
| 文本正则门禁 | `ShaderResourceSchemaRegressionGate` 里的 `ExtractFirstIntAfter` | 只抓得到字面量，改个写法就静默漏检 |
| 纯手写裸数字 | `TrilinearClampSampler 7u`、`TEXT_SDF_SPREAD 8.0` | **完全不报错**：采样器序号错 ⇒ 静默降级为 Nearest |

## 结构

```
inc/hgl/shader/contract/*.h                  ← 唯一真源（HGL_SHADER_ENUM 注解枚举）
        │
        │  clang++ -Xclang -ast-dump=json    （值/项序/项名全部直接读 AST）
        ▼
src/Tools/ShaderConstGen/shader_const_gen.py
        │
        ├─→ ShaderLibrary/contract/sampler_presets.glsl     采样器预设宏
        ├─→ ShaderLibrary/contract/engine_constants.glsl    槽位数 / 遍数 / 拓扑 / 哨兵 / 阴影位 / SDF
        ├─→ ShaderLibrary/contract/lighting_constants.glsl  LightRow 的 type / flags / shadow_kind
        ├─→ ShaderLibrary/contract/descriptor_macros.glsl   描述符集序号（原 DescriptorMacroGen 的产出）
        ├─→ ShaderLibrary/contract/interstage_locations.glsl 阶段间 location（原 InterStageLocationMacroGen）
        └─→ inc/hgl/shader/generated/ShaderConstantsParity.gen.h
                                                            与遗留常量的 static_assert
```

原 `DescriptorMacroGen` / `InterStageLocationMacroGen` 两个单功能 C++ 生成器已删除：
它们的产物现在由本生成器统一负责（真源分别是 `PipelineContract.h` 的
`DescriptorSetId` 与 `ShaderStageLocationId`，后者也被运行期
`ShaderSemanticRegistry` 的 `stable_location` 引用）。

`engine_constants.glsl` 会被域绑定表与 ECS 描述块无条件 `#include`，所以只放
「全局安全」的常量；`lighting_constants.glsl` 目前没有 GLSL 消费者，单独成文件，
避免把 `LIGHT_*` 白送进每个 fragment 的宏命名空间。阴影 PCF 的两个档位宏
（`HGL_SHADOW_PCF_POISSON_TAPS` / `HGL_SHADOW_NORMAL_OFFSET`）**不生成**——
它们由 `EmitDomainOptions` 按材质配置覆盖、并由 `shadow_pcf.glsl` 的 `#ifndef`
兜底，生成一份全局默认值反而会破坏「0 = 走 3x3 盒式」的可调语义。

命名与格式化策略（宏名前后缀、数值后缀、十六进制/浮点形态）**不在头文件里**，
而在 `shader_const_gen.toml`：头文件只回答「值是多少」，TOML 只回答「怎么写出去」。

## 用法

```powershell
# 生成（内容未变时不写盘，保持 mtime —— 增量构建友好）
python src/Tools/ShaderConstGen/shader_const_gen.py --repo-root . --write

# 只比对不落盘（CI / 门禁）
python src/Tools/ShaderConstGen/shader_const_gen.py --repo-root . --check
```

CMake 会通过 `cmake/codegen.cmake` 在构建期自动调用（见根 `CMakeLists.txt`）；
缺少 Python 3.11+ 或 clang++ 时自动跳过，仓库里已提交的生成物照常可用。

## 三类校验

### 1. 与遗留 C++ 常量对拍（编译期，零成本）

`[[assert]]` 生成 `static_assert`，钉住契约枚举与既有常量：

```cpp
static_assert(static_cast<int64_t>(contract::BatchTopology::Lines)
              == static_cast<int64_t>(ecs::GPUCullSystem::kBatchTopologyLines), "...");
```

只要有人改动 `GPUCullSystem::kBatchTopologyLines` 而没同步契约枚举（或反过来），
**编译直接失败** —— 不再依赖运行期测试或文本抓取。

### 2. 与手写 GLSL 对拍（生成期）

`[[verify_glsl]]` 逐条检查手写 GLSL 里的 `#define` / `const` 声明与契约枚举同值。
目前只剩两类真正"手写"的常量需要这样盯：

- `shadow_pcf.glsl` 的两个档位兜底宏（`EmitDomainOptions` 会按材质配置覆盖，
  生成全局默认值反而会破坏「0 = 走 3x3 盒式」的可调语义）；
- `compute/compute_transform_hierarchy.glsl` 的 `const uint INVALID_HANDLE`
  （示例按路径直接加载，不走 `@ulre` 模块系统）。

`[[require_include]]` 则反过来钉住"必须 include 生成物"的文件，防止有人把数字内联回去。

### 3. 与 `sampler.toml` 的顺序对拍（生成期）

`[[sampler_registry]]` 检查 `ShaderLibrary/contract/sampler.toml` 的 `[[samplers]]`
出现顺序与 `SamplerPresetId` 枚举逐项同名同序。采样器索引整体错位是**最危险**的一类
漂移（`GetIndex` 查不到名字时保底返回 0，表现为悄悄全用 Nearest 采样）。

## 新增一个常量的步骤

1. 在 `inc/hgl/shader/contract/` 合适的头文件里加枚举项（**必须显式赋值**）；
2. 在 `shader_const_gen.toml` 里给它所属的 `[[file.enum]]` 声明命名策略
   （追加项一般不用改，规则已定）；
3. 跑一次 `--write`，把生成的 `.glsl` / `.gen.h` 一起提交。

如果新枚举整块是新领域的，记得 `[[file.enum]]` 之外还要补 `[[assert]]`，
把与新枚举重复的遗留常量钉住。

## 约束

* 契约枚举必须是 `enum class` + 显式底层类型 + 逐项显式赋值。
* `inc/hgl/shader/contract/*.h` 只能包含 `<cstdint>` / `<cstddef>` 与同目录头，
  因为生成器把它当作**自包含单元**喂给 clang。
* `HGL_SHADER_CONST`（`static constexpr` 常量通道）只接受字面量初始值——Clang AST
  JSON 不给 `VarDecl` 求值结果。能写成枚举的一律优先写枚举。
