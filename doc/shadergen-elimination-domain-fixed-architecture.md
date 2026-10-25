# 彻底废除 ShaderGen 终极架构设计：基于 MaterialDomain 的固定着色器方案

> 状态：**终极架构规范（指导彻底删除 ShaderGen）**
> 核心命题：**彻底废除 C++ Shader 生成器与 AST 拼装，主着色器走向纯物理固定 GLSL，C++ 仅输出极简 options.glsl**
> 日期：2026-10-25

---

## 0. 核心定位与概念纠偏（必须牢记）

在 ULRE 的物理级极简架构中，核心分层与概念必须严格对齐，严禁混淆：

```
+-----------------------------------------------------------------------------------+
| 引擎核心: MaterialDomain (母材质 / 表面领域)                                         |
| - 例: StandardOpaque, UnlitSolid, VertexColor, Sky, Text2D, Foliage 等 16 个领域    |
| - 等价于 Unreal 中的 "Material" (母材质)                                            |
| - 它是真正持有 GLSL 算法与 Pipeline 契约的实体！                                     |
| - 全引擎仅有这十几个固定的 Domain，完全由引擎内部写死，上层不可新建、不可改！           |
+-----------------------------------------------------------------------------------+
                                         │ 实例化 (填数值、配贴图)
                                         ▼
+-----------------------------------------------------------------------------------+
| 用户资产: MaterialRecipe / .mi.toml (材质实例)                                      |
| - 例: lit.mi.toml, checkerboard_2d.mi.toml, builtin/pure_color                    |
| - 等价于 Unreal 中的 "MaterialInstance" (材质实例)                                  |
| - 资产绝不包含任何 Shader 代码！绝不编译为独立 SPV！                                 |
| - 它只是向 MaterialDomain 填参的一行数据，运行时直接作为 SSBO 行 + Bindless 纹理槽传递 |
+-----------------------------------------------------------------------------------+
```

### 关键纠偏结论
1. **编译 SPV 的单位从来都不是用户资产（`.mi.toml`）**！
2. **唯一需要对应着色器的实体是 `MaterialDomain`**（与其组合的渲染 Pass，如 ForwardLit / ShadowCaster）。
3. 现有的 `src/ShaderGen`（8000+ 行代码，包含 AST、Registry、DFS 依赖图、动态 Varying 拼接、OutputContract 拼接）之所以臃肿，是因为它**把原本属于固定 Domain 的东西，用 C++ 人肉充当了一套极度复杂的“动态预处理器”**。

---

## 1. 终极架构原则：彻底消灭 ShaderGen

### 1.1 物理级固定主着色器（Fixed Master Shaders）
每个 `MaterialDomain` 在结合对应渲染 Pass 时，其主着色器骨架是**物理级固定写死的 `.glsl` 文件**，不存在任何运行时代码拼接。
例如 `ShaderLibrary/domain/standard_opaque_forward_lit.frag.glsl`：
- 固定的顶点输入与 Varying 结构。
- 固定的表面解析逻辑（`EvalSurface(...)`）。
- 固定的光照与环境计算（`EvalPassEnvironment(...)`）。
- 固定的颜色附件输出（`out_Color = ...`）。

### 1.2 唯一允许的动态物：极简 `options.glsl`
全套着色器编译中，C++ 端**唯一允许动态生成**的产物是一个不到 20 行的配置头 `options.glsl`（甚至直接内存注入字符串）：
```glsl
// options.glsl (极简配置，无算法逻辑)
#pragma once

// 1. 仅限硬件必须特化的宏开关（如 Dither Mask / Discard / Blend）
#define HGL_ALPHA_TEST 1
const float HGL_ALPHA_CUTOFF = 0.5;

// 2. 引入该 Domain 对应的静态算法模块（纯多态 include，无 C++ 逻辑计算）
#include "surface/pbr_surface_source.glsl"
#include "ntb/ntb_tangent_vbo_normalmap.glsl"
```

### 1.3 绝大部分分支走“多态 include”与“空对象（Null Object）”，彻底消灭 `#if`
除硬件管线必须依赖的 Alpha 覆盖（`discard` / Dither）外，所有光照与环境算法**绝不使用 `#if / #else` 条件编译**：
- 关阴影：直接 `#include "shadow/shadow_none.glsl"`（内部直接 `return 1.0;`）。
- 关漫反射：直接 `#include "lighting/diffuse_none.glsl"`（内部直接 `return vec3(0.0);`）。
- 关天光：直接 `#include "ambient/ambient_none.glsl"`（内部直接 `return vec3(0.0);`）。
编译器通过内联与死代码消除（DCE）在微秒内生成最优代码。

### 1.4 材质属性全部通过运行时 BDA 传参（零编译开销）
贴图索引号、粗糙度乘数、基础颜色等业务参数，全部通过 `materialDataIndex` 直接从 BDA 内存（`MaterialDataSSBO`）中获取。换参数只需要更新 Buffer 数据，**完全不触发着色器重新编译**。

---

## 2. 待清理的旧 ShaderGen 模块清单

实现该终极架构后，`src/ShaderGen` 内部庞大的历史包袱将全部彻底删除：

| 模块 / 类 | 现有代码行 | 历史职责 | 终极形态处理 |
|---|---|---|---|
| `GenericMaterialBuilder.cpp` | 929 | 统筹材质依赖、插槽选择、AST 装配 | **彻底删除** |
| `MaterialShaderEmitter.cpp` | 675 | 动态拼接 GLSL 文本与宏定义 | **彻底删除** |
| `ShaderCodeModuleFile.cpp` | 573 | 解析 `// @ulre` 12 种注记表 | **彻底删除** |
| `FragmentTemplateComposer.cpp` | 499 | 动态拼接片元模板插槽与输出声明 | **彻底删除** |
| `ShaderCodeModuleRegistry.cpp` | 418 | 模块图维护、DFS 依赖遍历、剔除环 | **彻底删除** |
| `ShaderDocument.cpp` / AST | 173 | 维护代码块顺序、自述校验 | **彻底删除** |
| `RenderTemplateResolve.cpp` | 150 | 动态解析模板根与插槽角色 | **彻底删除** |
| **合计** | **3400+ 行**（核心逻辑占 ~38%） | 全部属于 C++ 人肉代码预处理器 | **替换为一个不到 200 行的极简 OptionsEmitter** |

---

## 3. 演进路线图

1. **第一阶段：固化各 MaterialDomain 的主着色器**
   - 梳理已闭环的重点领域（`StandardOpaque`、`UnlitSolid`、`VertexColor`、`Sky` 等）。
   - 为每个 Domain + Pass 准备物理固定的 `.glsl` 主模板文件。
2. **第二阶段：建立直通编译管道（Bypass ShaderGen）**
   - 编写轻量级 `DomainShaderPipeline`（< 200 行代码）：
     - 输入：`MaterialDomain` 枚举 + Pass 类型。
     - 输出：极简 `options.glsl` 内存字符串。
     - 动作：直接调用 `GLSLCompiler.dll` 编译固定主着色器，利用 SPIR-V 反射提取信息。
3. **第三阶段：整体切替与旧代码物理删除**
   - 将渲染系统底层对接至直通管道。
   - 一次性物理删除 `src/ShaderGen` 下冗余的生成器、注记解析与 AST 模块。
