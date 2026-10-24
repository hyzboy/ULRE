# 固定代码着色器架构与表面领域契约

> 状态：阶段一（表面领域契约层）已落地
> 相关代码：`inc/hgl/mtl/MaterialDomain.h`、`src/ShaderMaterialData/MaterialDomain.cpp`、
> `ShaderLibrary/common/pass_environment_interface.glsl`

---

## 1. 要解决的问题

现行 ShaderGen 的自由度是三维笛卡尔积：

```
(顶点格式) × (材质参数组合) × (Pass / 光照环境)
```

三者都可任意组合，于是：

- **无法穷举测试**。回归矩阵是乘法爆炸，任何一次改动都只能抽样验证。
- **合批率被拖垮**。哪怕只差一个"有没有法线贴图"的开关，`layout_hash` 就不同，
  材质行布局不同，合批直接断掉。
- **光照与材质互相污染**。`lit_ibl` 这类名字把"用不用 IBL"写进了材质身份里，
  上层美术/策划的资产被迫理解渲染管线细节。

目标：把无限组合收敛为**有限个不可协商的领域契约**，Shader 源码固定，
C++ 端只生成一个配置宏头。对使用者而言不存在 `Material` 这个概念，
只有"这个模型是什么（领域）" + "填值 + 塞贴图"。

---

## 2. 三层解耦

```
┌─────────────────────────────────────────────────────────────┐
│ 1. 资产层  Surface Description                                │
│    只声明 Domain + 数值 + 纹理槽；不含任何光照/管线概念         │
├─────────────────────────────────────────────────────────────┤
│ 2. 管线与环境层  Pass Environment                             │
│    Forward / GBuffer / Depth-Shadow / 无光 各自由 Pass 决定     │
│    注入哪个 output_policy 模块（见 pass_environment_interface） │
├─────────────────────────────────────────────────────────────┤
│ 3. 着色器实现层  固定源码 + 编译期宏头                          │
│    领域 shader 只构造 SurfaceOutput，最后一行交给                │
│    EvalPassEnvironment()；不认识阳光/阴影/IBL/GI 任何一个        │
└─────────────────────────────────────────────────────────────┘
```

关键约束：**领域 shader 的最后一行永远是环境评估调用**。

```glsl
// 领域 shader 的骨架（示意；实际从 .tmpl 生成，见阶段二）
#include "Generated_DomainConfig.h"        // 只有宏，没有代码拼接

SurfaceOutput surf = BuildStandardOpaqueSurface(input);
#if HGL_ALPHA_CUTOFF
if (surf.alpha < HGL_ALPHA_CUTOFF_REF) discard;
#endif

return EvalPassEnvironment(PassEnvironmentInput(input, surf, materialDataIndex));
```

换光照 = 换 `output_policy` 模块 = 换一个 `#include`，**领域源码一行不动**。

---

## 3. 领域契约表（实现状态）

契约的单一真源是 `src/ShaderMaterialData/MaterialDomain.cpp` 里的 `DomainContracts[]`。
下表与代码保持同步，**代码是权威，本表是导读**。

| 领域 | 顶点语义契约 | 纹理槽 | 参考模块 | 状态 |
| :--- | :--- | :--- | :--- | :--- |
| `StandardOpaque` | `Position` `UV0` `Normal` | `base_color` `roughness` `metallic` `occlusion` `opacity_mask` `normal` | `pbr_surface_source` + `ntb_tangent_vbo_normalmap` | ✅ 已实现 |
| `UI2D` | `Position` `UV0` | `sprite` | `texture_source` + `ntb_orthonormal` | ✅ 已实现 |
| `Foliage` | 待定 | 待定 | — | ⬜ 未实现 |
| `Terrain` | 待定 | 待定 | — | ⬜ 未实现 |
| `HumanSkin` | 待定 | 待定 | — | ⬜ 未实现 |
| `Custom` | 不校验 | 不校验 | — | ✅ 向后兼容逃生口 |

### 3.1 为什么 StandardOpaque 的契约里没有 `Tangent`

早期设计草案写的是 `vec3 Pos, vec3 Normal, vec4 Tangent, vec2 UV0`——**这是错的，别照抄**。

引擎的 NTB 提供者 `ntb_tangent_vbo_normalmap.glsl` 用的是 Lengyel cotangent frame：
从世界位置 + UV0 的屏幕空间导数（`dFdx`/`dFdy`）现算切线基。
`InterStageSemantic` 里虽然登记了 `WorldTangent` / `WorldBinormal`，但 mesh varying
的发射白名单**刻意不含它们**——片元阶段根本没有这两个 varying。

因此"要求材质声明 Tangent 顶点语义"等于要求一条不存在的顶点流，会让所有既有
Production 材质在解析期直接失败。契约以 `ntb_tangent_vbo_normalmap` 的实际输入为准。

> 未来若改为真正的切线顶点流，需要同时改三处：mesh varying 发射白名单、
> NTB 提供者模块、本契约表。三者不同步就会立刻炸门禁。

### 3.2 `must_declare` 与 `required` 不是一回事

| 字段 | 位置 | 含义 |
| :--- | :--- | :--- |
| `must_declare` | 契约表 | 材质的 `[resources].textures` **必须列出**这个槽 |
| `required` | 材质 TOML | 运行时**必须绑定**真实纹理 |

`must_declare = true` + `required = false` 是有意为之：保证同一领域的所有材质
**引用行布局完全一致**（可共享 `layout_hash`、可合批），但绑定仍可缺省——
未绑定时 `SampleOptional` 以中性值（1.0）保底。

### 3.3 槽顺序即等于 `layout_hash`

ShaderGen 按**声明顺序**排布材质引用行。调整契约表里既有领域的槽顺序 =
`layout_hash` 变化 = 所有材质缓存失效 = 首帧全量重编译。

**不要重排既有领域的槽，只在末尾追加。**

### 3.4 `Custom` 是逃生口，不是默认值

材质 TOML 不写 `domain` 键 → `MaterialDomain::Custom` → `ValidateMaterialDefinitionDomain`
**直接返回 OK**，不做任何校验。

这是刻意的向后兼容设计：schema-4 的历史材质（包括全部示例与既有 Production 资产）
一夜之间全部变成非法是不可接受的。但 `Custom` 不是"新材质可以不写 domain"的许可——
新增资产应当声明领域，`Custom` 只会随着阶段四的资产收敛逐步消失。

> ⚠ 实现细节：`ValidateMaterialDefinitionDomain` 对 `Custom` 返回
> `MaterialDomainValidationResult::OK` 而**不是** `NoContract`。调用方只把 `OK` 当通过，
> 返回 `NoContract` 会让每一个既有材质都变成解析失败（曾因此让整个门禁崩溃，
> 定位成本极高）。`NoContract` 的语义是"非 Custom 但契约表里查不到"，属内部错误。

---

## 4. 校验流程

```
ParseMaterialDefinitionFile(.material.toml)
  ├─ ParseDefinition                 ← 解析 domain 键（缺省 = Custom）
  ├─ ValidateMaterialDefinitionKeys  ← 顶层键白名单（含 "domain"）
  └─ ValidateMaterialDefinitionDomain
       ├─ Custom            → OK（豁免）
       ├─ 顶点语义 bitmask  → definition.vertex_requirements 必须覆盖
       ├─ 顶点提供策略      → provider_policy 必须允许派生
       ├─ 材质源模块        → material_source_module_name 必须存在（UI2D 豁免）
       └─ 纹理槽            → 白名单 + must_declare
```

任何一步失败都返回人类可读的 `out_detail`，由
`RecordParseFailure("domain", ...)` 记录为解析错误 → `InvalidValue`。
**Fail-Fast 在资产加载期，不漏到运行时。**

### 4.1 扩展方式：加领域，而不是放宽契约

新增 `Foliage` 支持的正确做法：

1. 在 `MaterialDomain` 枚举里加 `Foliage`（并更新 `ENUM_CLASS_RANGE` 与
   `MaterialDomainContractCount`，`static_assert` 会强制你对齐）。
2. 在 `DomainContracts[]` 里填该领域的顶点语义位、纹理槽表、参考模块名。
3. 实现 Foliage 的 `material_source_provider` 与 `ntb_provider` 模块。
4. 让门禁的 `D.material-domain-contract` 自检自然覆盖新行。

**绝对不要**为了让某个特例材质过校验而去改 `ValidateMaterialDefinitionDomain`
的判断逻辑——那会让契约退化成"什么都没有"。

---

## 5. 门禁

| 门禁 | 基线 | 说明 |
| :--- | :--- | :--- |
| `ShaderResourceSchemaRegressionGate all` | 53 PASS / 0 FAIL | 含 `D.material-domain-contract` |
| `TestCSMIncrementalPass` | 51/51 | CSM 契约 |
| 8 示例烟测 | 零 error / 零 VUID | TextureQuad / DrawTriangle / MaterialErrorFallback / LineRender / TextDraw / CascadeShadowMap / PBRSpheres / DualWorldShadows |
| 全量 `ctest` | 183/184 | `BidirectionalMapStressTest` Timeout 为既存问题 |

> `ShaderCodeModuleRegistry::LoadDirectory` 的文件模块数量被门禁**硬编码**校验
> （当前 72）。新增任何 `.glsl` 模块都必须同步
> `ShaderResourceSchemaRegressionGate.cpp` 的 `file_count` / `expected_count`，
> 否则 `I.glsl-code-module-file-parse` 会失败。

---

## 6. 阶段路线

| 阶段 | 分支 | 交付 |
| :--- | :--- | :--- |
| 一 · 契约层 | `feature/shader-domains-contract` | 领域枚举 + 契约表 + 校验 + 门禁 + 环境接缝接口 |
| 二 · 宏头生成器 | `feature/shadergen-config-generator` | ShaderGen 降维为 `Generated_DomainConfig.h` 生成器；剥离 AST 拼接 |
| 三 · 环境解耦 | `feature/pass-environment-decoupling` | Pass 决定注入哪个环境头；消除 `lit_ibl` 类材质概念 |
| 四 · 资产收敛 | `feature/surface-asset-unification` | 资产只填领域 + 值 + 图；合批率收敛；清理旧拼接代码 |
