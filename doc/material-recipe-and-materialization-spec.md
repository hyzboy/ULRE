# Material Recipe 与 Materialization Spec 契约(6th 重构宪法)

> 本文是 `refactor/6th` 分支材质系统重写的**架构宪法**。
> 它**不是**实现文档,而是锁死数据契约与信息流向的最高约束——实现只要遵循本文,任何写法都行;实现违反本文,就是 bug。
>
> 旧文档 `doc/SimplifiedMaterialSystem_Design.md`(_5th 分支)是历史记录,与本文冲突时**以本文为准**。
>
> 状态:**草案 v4 —— 待 Phase 1 落地验证**
>
> **D1 收敛说明(当前分支已落地):** 本文描述的 `MaterializationSpec` / `MaterializationResolver` / `MaterializationPools` 旧物化体系已整体删除。下游 `MaterializeRecipeRowsForPrimitive` 现直接由单一绑定 IR `ResolvedBindingTable`(`inc/hgl/mtl/MaterialBindingContract.h`,由 `ShaderGen/BindingTableBuilder` 构建)驱动,唯一合法 SSBO 输出通道为 `resolved_ssbo_bindings`。本文保留为历史设计参考:**Recipe 语义(§4)与 shader 侧三个语义契约(`MaterialTextureLayerTable` / `MaterialDataIndexTable` / `MaterialDataSlotData`,§6.2)仍有效,其数据源已由旧的 index_tables 上传迁移到 resolved_ssbo_bindings 直接驱动**。

---

## 0. 一句话定锚

**把整条链路当编译器:`Recipe` 是源码,`Materialization Spec` 是 IR,`GLSL` 是目标代码。ECS 是编译器前端(策略唯一驻地),ShaderGen 是编译器后端(无脑执行器)。Spec 是前后端之间唯一且自包含的契约。**

---

## 1. 为什么存在这份文档

### 1.1 5th 走过的弯路

`refactor/_5th` 用 AI 辅助演进材质系统,积累了大量过渡期债务:
- `MaterialResolveSystem` + tiered cache(L1/L2/L3)+ triad 状态(program_binding/program/payload)+ staging/committed 双缓冲 + 6 套诊断统计
- 这些都是"新旧路并存、每改一处维护双兼容"的产物,根源是**材质这一层的契约没想清楚就开始演进**

### 1.2 6th 的态度

- **不合并 _5th**。735 个提交里的过渡期双兼容代码全是负债,合并即继承债务。
- **干净重写**。从 `refactor/6th` 这个砍干净的骨架出发,先把材质契约定死,再逐功能补齐。
- **彻底放弃 texture/sampler slot 模式**。day-1 走 bindless,不留双兼容。
- **不在 shader 里做材质选择**。所有策略集中在 ECS 编译期决策。

---

## 2. 三层模型

渲染一个 PBR lit 物体时,信息按三层流动,**每层只跟相邻层有契约**:

```
┌─────────────────────────────────────────────────────────┐
│ Layer 1: Surface(材质模型)                              │
│   固定的光照数学。调一个固定函数名 GetPBRInput(uv),     │
│   拿 struct,算 BRDF。不知道数据从哪来。                 │
├─────────────────────────────────────────────────────────┤
│ Layer 2: Materialization(GetPBRInput 的函数体)          │
│   由 Spec 决定。从池里取数据(经间接索引)、从 bindless   │
│   数组取纹素、做 channel 拆分、法线解码、降级填充。      │
│   函数体由 ShaderGen 生成。                              │
├─────────────────────────────────────────────────────────┤
│ Layer 3: Resource(vec4 数据源)                          │
│   一个 resource 产出恰好一个 vec4 采样。类型有四种:     │
│   Texture2D / Texture2DArray / PCG function / Constant  │
└─────────────────────────────────────────────────────────┘
```

**关键不变量:Surface 永远一个样,变的是 GetPBRInput 的函数体。** 打开生成的 `.glsl`,`GetPBRInput` 的实现就在那里,一目了然,可调试。

### 2.1 Material Model(Surface 契约)

每个材质模型定义 **一个 input struct + 一个 getter + 每个字段的默认值**。Day-1 只有一个模型:PBR lit。

```glsl
// PBR lit 材质模型的契约(surface 端的输入)
struct PBRInput {
    vec3  basecolor;     // default: vec3(0)
    vec3  normal_tan;    // default: vec3(0,0,1)   ← 切线空间
    float metallic;      // default: 0
    float roughness;     // default: 1
    float ao;            // default: 1
    vec3  emissive;      // default: vec3(0)
};

PBRInput GetPBRInput(uint instance_id, vec2 uv);   // surface 只调这个
```

- surface 模板(`surface/pbr_lit.glsl`)固定代码:声明 struct、调 getter、用字段算 BRDF、做 TBN 变换。
- 新增材质模型(unlit / clearcoat / skin)= 新 struct + 新 getter + 新模板。正交于其他模型。
- **法线空间契约**:getter 统一返回**切线空间**法线。flat/PCG 情况 getter 直接 `return vec3(0,0,1)`;TBN 变换永远在 surface 做。绝不让 getter 有时返回世界法线——那会重新引入分支。

> ⚠️ 注意:Material Model **只定义 surface 的输入 struct (PBRInput) 和 getter 名**,**不定义数据从哪个结构体来**。数据结构体(PBRSurface 等)是独立注册的,与材质模型正交(见 §6.3)。

### 2.2 Materialization(中间层)

Materialization 是**一个生成出来的函数体**。它从两类池取数据,经 per-instance 间接索引:

```glsl
// TextureLayer SSBO(per-recipe stride baked)
//   layout(set=DATA_SET, binding=TEXLAYER_BINDING) readonly buffer
//   TextureLayerData { uint tex_layers[]; } texlayer;
// DataIndex SSBO(per-recipe stride baked)
//   layout(set=DATA_SET, binding=DATAINDEX_BINDING) readonly buffer
//   DataIndexData { uint data_indices[]; } dataidx;
// PBRSurface pool(注册的标准结构体)
//   layout(set=DATA_SET, binding=PBRSURFACE_BINDING) readonly buffer
//   PBRSurfaceData { PBRSurface surfaces[]; } pbrsurface;

PBRInput GetPBRInput(uint instance_id, vec2 uv) {
    PBRInput m;

    // —— 纹理:经 TextureLayer 间接表 → bindless 池 ——
    uint tex_base = instance_id * 3;                       // stride baked
    uint bc_handle = texlayer.tex_layers[tex_base + 0];
    uint n_handle  = texlayer.tex_layers[tex_base + 1];
    uint mr_handle = texlayer.tex_layers[tex_base + 2];

    m.basecolor  = texture(globalTex2D[bc_handle], uv).rgb;
    vec2 n       = texture(globalTex2D[n_handle],  uv).rg;
    m.normal_tan = vec3(n, sqrt(max(0, 1 - dot(n,n))));   // ReconstructRG8
    vec4 mr      = texture(globalTex2D[mr_handle], uv);
    m.metallic   = mr.r;
    m.roughness  = mr.g;

    // —— 数据结构体:经 DataIndex 间接表 → PBRSurface 池 ——
    uint surf_idx = dataidx.data_indices[instance_id * 1 + 0];  // stride baked
    PBRSurface s = pbrsurface.surfaces[surf_idx];
    m.emissive   = s.emissive;

    m.ao = 1;   // 模型默认
    return m;
}
```

特点:**无分支、I/O 最优、池+索引对称、规整、inja 可模板化**。

### 2.3 Resource(vec4 数据源)

一个 resource 的抽象接口是**给定 uv 返回一个 vec4**。采样行是唯一类型相关的部分,之后全统一:

| 资源类型 | 采样行 | 运行时成本 |
|---|---|---|
| Texture2D | `vec4 s = texture(globalTex2D[idx], uvT(uv));` | 1 次纹理 IO |
| Texture2DArray | `vec4 s = texture(globalTex2DArray[idx], vec3(uvT(uv), layer));` | 1 次纹理 IO |
| PCG function | `vec4 s = pcg_checkerboard(uv, params);` | 计算量 |
| Constant | `vec4 s = vec4(0.8,0.2,0.1,1);` | **零** |

**资源分两类(命名区分):**
- **BakedResource**:Constant + 无参数 PCG。完全烘焙进 shader,**零 TextureLayer 槽、零采样**。
- **BoundResource**:Texture / TextureArray / 带参数 PCG。需要运行时数据,**占 TextureLayer 一个 uint32 槽**(Texture 类)或 DataIndex 槽(结构体引用)。

> 注意:BoundResource 的 Texture handle **不进任何固定 struct**,进 TextureLayer SSBO(见 §6)。结构体引用也走 DataIndex 间接(见 §6)。

---

## 3. 信息流与责任划分(核心)

### 3.1 完整链路

```
  Material Recipe                      ← 开发者提的源码(可任意复杂)
  (用户意图,可引用外部资源、留模糊、带作者标记)
       │
       │  ECS = 编译器前端(策略唯一驻地)
       │  · 语义分析:probe 资源元数据(format/channels/degenerate)
       │  · 条件编译:画质等级 / GPU 能力 / FBO 格式 / geometry 格式
       │  · 优化降级:flat → Constant、远距离 → 简化、资源丢失 → fallback
       │  · 资源去重:同路径纹理 → 同 handle
       │  · TextureLayer/DataIndex 组装:按 spec 的 stride 写 uint32 数组
       │  · 结构体池注册:PBRSurface 等标准结构体按名字注册
       ▼
  Materialization Spec                 ← IR(前后端唯一契约,自包含)
  (机器决断,无歧义,每个纹理已 resolved 为 handle,
   每个降级已做掉,每个资源已 reclassify 定型)
       │
       │  ShaderGen = 编译器后端(无脑执行器)
       │  · 只信 spec,不查 recipe、不看 GPU、不问画质
       │  · 可独立单元测试(喂手写 spec,查输出 GLSL)
       │  · spec 哈希 = shader 编译缓存键
       ▼
  GLSL                                 ← 目标代码
```

### 3.2 权威驻地

| 问题 | 谁说了算 | 备注 |
|---|---|---|
| 画质/GPU/资源状况导致的变体 | **ECS**(spec 生成那一刻) | spec 落地即决断,过了这条线不存在"为什么" |
| normal 变成 vertex normal 的原因 | **ECS**,但 ShaderGen **不关心** | 是 flat、还是太小太远、还是贴图丢了——ECS 在 spec 里都表达成同一个"无 normal 资源,用默认" |
| 纹理最终用哪种解码(RGB16F 直通 / RG8 重建) | **ECS** 写进 spec | 资源层永远只吐 raw vec4,不被格式污染 |
| getter 函数体长什么样 | **ShaderGen**(看 spec 生成) | 完全由 spec 决定 |
| shader 缓存命中 | spec 哈希比对 | 同 recipe 不同资源元数据 → 不同 spec → 不同 shader |
| 哪个 PBRSurface 数据给这个 instance 用 | **C++**(写 DataIndex 数组) | ECS/渲染批次决定索引值 |

### 3.3 三条硬约束

1. **单向信息流**。信息只从 Recipe → ECS → Spec → ShaderGen → GLSL。**绝不允许** ShaderGen 回头查 recipe、查 GPU 能力、查画质。ECS 是策略唯一驻地。
2. **Spec 必须自包含**。这是前后端防火墙。Spec 不能带指针、不能带"请去查 recipe 第 3 项"、不能引用外部状态。Spec 里出现的每个纹理必须已是 resolved handle;每个资源必须已 reclassify 定型;每个降级必须已做掉。
3. **Surface 不做材质选择**。Surface 是固定代码,只调固定函数名 `GetPBRInput(uv)`。所有选择推到 spec 生成那一刻(ECS 编译期),**不在 GLSL 里**。在 shader 里放 `if(有法线)...` 就是回到"一个 shader 服务所有"的反模式。

> **关于"编译期"的精确含义**:决策发生在 **ECS 系统 Tick 的 spec 构建阶段**(CPU 上的编译期),不是 GLSL 运行期、也不是 shader 编译的物理时刻。它是"渲染前、CPU 上、一次决断"。

---

## 4. Material Recipe(源码契约)

Recipe 是**开发者意图的表达**,可以任意复杂、引用外部资源、留模糊、带作者标记。

### 4.1 Day-1 字段(最小集)

```cpp
struct MaterialRecipe {
    MaterialModel       model;              // e.g. PBR_LIT

    // 资源声明:作者意图,引用路径/参数
    struct ResourceDecl {
        ResourceKind        kind;           // Texture2D / Texture2DArray / PCG / Constant
        std::string         semantic;       // "BaseColor"/"MR"/"Normal" —— 作者给的逻辑名
        std::string         path;           // 纹理路径(纹理类)
        std::vector<uint8_t> pcg_params;    // PCG 参数(PCG 类)
        ChannelMap          channels;       // {field: "basecolor", swizzle: ".rgb"} 等
        bool                treat_as_constant = false;  // 作者显式要求降级(f稀 等)
        UVTransform         uv;             // 可选:per-resource UV 变换
    };
    std::vector<ResourceDecl> resources;

    // 作者对画质/GPU 的偏好(可留空,ECS 按全局配置兜底)
    QualityHint         quality;

    // 声明的数据结构体引用(名字 + 类型),双材质过渡可写两份同类型
    struct StructRef {
        std::string     type;              // "PBRSurface"
        std::string     name;              // "from"/"to"/""(单份时可空)
    };
    std::vector<StructRef> struct_refs;
};
```

### 4.2 Recipe 允许的"模糊"

- `path` 可以指向一个**当时不存在的纹理**(加载后才知道是否存在/什么格式)
- `treat_as_constant` 可以不填(ECS 按资源元数据自动判定)
- `channels` 可以部分留空(ECS 用模型默认值兜底)
- 同一 semantic 可以有多份候选(ECS 按画质选其一)

**Recipe 可以复杂,因为复杂度由 ECS 消化;Spec 必须简单,因为简单度由下游依赖。**

---

## 5. Materialization Spec(IR 契约)

> **D1 已删除此 IR。** 其职责已由 `ResolvedBindingTable`(见 `inc/hgl/mtl/MaterialBindingContract.h`)承接。本节保留为历史契约描述,供理解 shader 侧消费语义。

Spec 是**机器决断的表达**,自包含、无歧义。它是前后端防火墙,也是 shader 缓存键。

### 5.1 字段

```cpp
struct MaterializationSpec {
    MaterialModel       model;              // 决定用哪个 surface 模板

    // 已 resolved 的资源:每个资源已是 BoundResource 或 BakedResource
    struct ResolvedResource {
        ResolvedKind        kind;           // Tex2D / Tex2DArray / PCG / Constant
        // —— BoundResource(Texture 类)用 ——
        uint32_t            handle;         // 全局 bindless 数组索引(已 resolve)
        TextureSlot         tex_slot;       // 在 TextureLayer 里的槽位(enum,见 §6.5)
        NormalDecode        normal_decode;  // Direct / ReconstructRG8 / ...
        // —— BakedResource 用 ——
        vec4                constant_value; // Constant 类直接填这个
        // —— 共用 ——
        std::vector<ChannelBinding> channels; // {field: "basecolor", swizzle: ".rgb"}
        UVTransform         uv;
    };
    std::vector<ResolvedResource> resources;

    // 已 resolved 的结构体引用:名字 + 类型 + DataIndex 槽位
    struct ResolvedStructRef {
        std::string         type;           // "PBRSurface"(结构体类型名)
        std::string         name;           // "from"/"to"/""
        DataSlot            data_slot;      // 在 DataIndex 里的槽位(enum,见 §6.5)
    };
    std::vector<ResolvedStructRef> struct_refs;

    // 该 spec 需要哪些 SSBO 类别(从 resources + struct_refs 派生,见 §6)
    SSBOCategoryMask    ssbo_needs;

    // 两个间接表的 stride(本 spec 每个 instance 占几个槽)
    uint32_t            tex_per_instance;   // TextureLayer stride
    uint32_t            structs_per_instance; // DataIndex stride

    // 决算出的 permutation 标记(影响 surface 门控,可选)
    QualityTier         resolved_tier;
};
```

### 5.2 数据结构体正交于材质模型

**关键澄清**:PBRSurface 这类数据结构体是**独立注册的**,与材质模型正交。

- `PBRSurface` 是一个**注册的标准结构体**,不是"PBR lit 的私有数据"。
- 多种 PBR 画质变体(超高/高/中/低)**共用同一个 PBRSurface 结构和同一份 PBRSurface 数据**——它们只是 shader 不同,数据源完全一样。
- clearcoat 材质可声明 `PBRSurface + ClearcoatData`(两个结构)。
- 双材质过渡声明 `PBRSurface("from") + PBRSurface("to")`——**同类型、不同名字**,DataIndex 里有两个槽分别索引。
- 材质声明时不但指定结构体**类型**,还要指定**名字**(为区分同类型多份)。

```glsl
// PBRSurface(独立注册的标准结构体,不隶属于任何材质模型)
struct PBRSurface {
    vec4  basecolor_const;   // baked basecolor(无纹理资源时用)
    float metallic_const;
    float roughness_const;
    vec3  emissive;
    // 注意:不含纹理 handle —— handle 在 TextureLayer SSBO
};
```

### 5.3 同一份 PBRSurface 数据多处共用

**这是独立注册的核心价值**:同一个 PBRSurface 数据(如"湿滑大理石"参数集)可以被多个 instance 引用(同一 DataIndex 索引值)。游戏中改变一个物体表面材质类型 = 改它的 DataIndex 索引值,**不是改一堆参数**。

```
PBRSurface pool:  [0]大理石  [1]金属  [2]木头  [3]湿大理石  ...
DataIndex:        instance_7 → 3      ← 改这个值,instance_7 从"大理石"变"湿大理石"
```

### 5.4 Spec 是 shader 缓存键

- `hash(spec)` = shader 编译缓存键
- 同 recipe 的玩家(RGB16F 法线)和 NPC(RG8 法线)→ 资源元数据不同 → ECS 产出**两个不同 spec** → **两条 shader**。这是对的,正是你要的。
- 注意:两个 spec 引用的 **PBRSurface 结构相同**(都是 PBR 标准结构),但 TextureLayer stride 可能不同、normal_decode 不同 → spec 哈希不同。

---

## 6. 数据供给模型(核心)

这是数据从 C++ 流到 shader 的**唯一通道设计**。核心思想:**池 + 间接索引**,且纹理和数据结构体**完全对称**。

### 6.1 双池模型

```
                    ┌─ 纹理池                                ← 特殊:GPU 采样器资源
资源池(pools) ──────┤  globalTex2D[] / globalTex2DArray[]...
                    │  (bindless,放在 set 4)
                    │
                    └─ 数据结构体池                          ← 通用:开放注册的标准结构
                       PBRSurface[] / ClearcoatData[] / ...
                       (具名 SSBO,放在 set 2)

                    ┌─ TextureLayer SSBO: uint32[]    ← 纹理 handle,per-instance
间接索引(per-instance)┤  offset = instance_id × tex_per_instance
                    │
                    └─ DataIndex SSBO:   uint32[]    ← 结构体索引,per-instance(新增)
                       offset = instance_id × structs_per_instance

实例(instance_id) → 查两个间接表 → 拿到 handle / 结构体索引 → 访问池
```

**对称性**:纹理和数据结构体**完全同构**——都是"池化数据 + per-instance 间接索引"。纹理的池是 bindless 采样器数组(set 4);结构体的池是具名 SSBO 数组(set 2)。两者都用 uint32[] 间接表(TextureLayer / DataIndex),都按 `instance_id × stride + slot` 寻址。

### 6.2 两个间接表 SSBO(独立)

| SSBO | 内容 | 指向 | stride |
|---|---|---|---|
| `TextureLayer` | uint32 纹理 handle 数组 | set 4 的 bindless 采样器数组 | `tex_per_instance`(spec baked) |
| `DataIndex` | uint32 结构体索引数组 | set 2 的具名结构体 SSBO 池 | `structs_per_instance`(spec baked) |

**为什么分开**(而非合并):TextureLayer 的 handle 指向 set 4 的 bindless 采样器(特殊 GPU 资源),DataIndex 的索引指向 set 2 的结构体 SSBO。**目标不同,分开更清晰**,且让 stride 独立(纹理数和结构体数各自变化互不影响)。

### 6.3 数据结构体:开放注册,正交于材质模型

数据结构体(如 PBRSurface)是**开放注册的标准结构**:
- C++ 侧定义 struct,按名字注册("PBRSurface" → 某个 SSBO 池)
- GLSL 侧按名字声明使用
- **依我们的结构设计会比较少**(不必每个材质都重写一套)
- 材质 recipe 引用结构体时声明 **(类型, 名字)**——名字区分同类型多份(过渡的 from/to)

### 6.4 Day-1 全部 SSBO 清单

| 类别 | 性质 | 结构定者 | 内容 | 特殊地位 |
|---|---|---|---|---|
| `TransformID` | 实例索引 | 固定 | `uint32`(instance index) | ⭐ 特殊 |
| `TransformData` | L2W 矩阵 | 固定 | `mat4[]` | ⭐ 特殊 |
| `TextureLayer` | **per-recipe 动态** | recipe 定 stride | `uint32[]` 纹理 handle 数组 | 间接表(纹理) |
| `DataIndex` | **per-recipe 动态** | recipe 定 stride | `uint32[]` 结构体索引数组 | 间接表(结构体,新增) |
| `PBRSurface` 等具名结构体池 | 材质模型级/通用 | 开放注册 | 结构体数组 | 池 |

- **TransformID / TransformData 地位特殊**:它们不是"材质"数据,而是场景/变换数据,可能需要特别处理(如 instance-rate VAB)。但它们遵循同一套"按名字注册/使用"的解耦模式。
- **TextureLayer / DataIndex 是间接表**:per-recipe 动态,C++ 按 spec 的 stride 组装 uint32 数组。
- **PBRSurface 等是结构体池**:具名 SSBO,按名字注册,可被多个 instance 共用索引。

### 6.5 C++ 枚举 + 材质声明(slot 显式定义)

**材质类型/SSBO/UBO 都是极少的,全部有 C++ 定义和枚举。** 加新的 SSBO/UBO 需要写 C++ 代码——这是有意为之的约束,换来的是 slot 不用"算",开发者直接用 enum 值指定。

**(a) SSBO 类别枚举**(已知所有 SSBO):

```cpp
enum class SSBOCategory {
    TransformID,
    TransformData,
    TextureLayer,
    DataIndex,
    // —— 以下为结构体池(需写 C++ 注册)——
    PBRSurface,
    ClearcoatData,
    // 加新的需写 C++ 代码
};
```

**(b) TextureSlot 枚举**(TextureLayer 间接表里的槽位,语义化):

```cpp
enum class TextureSlot : uint8_t {
    BaseColor = 0,
    Normal,
    Metallic,
    Roughness,
    Height,
    Opacity,
    Emissive,
    AO,
    Detail,
    // 加新的需写 C++ 代码

    ENUM_CLASS_RANGE(BaseColor, Detail)
};

// 配套名字表,顺序必须与 enum 对齐(static_assert 保证)
constexpr const char* TextureSlotNameList[] = {
    "BaseColor", "Normal", "Metallic", "Roughness",
    "Height", "Opacity", "Emissive", "AO", "Detail",
};
static_assert(/* count matches */);
```

**(c) DataSlot 枚举**(DataIndex 间接表里的槽位,指向结构体池):

```cpp
enum class DataSlot : uint8_t {
    PBRSurface   = 0,   // 单份 PBRSurface
    PBRSurfaceTo = 1,   // 双材质过渡的"目标"份
    Clearcoat    = 2,
    // 加新的需写 C++ 代码
};
```

**slot 不是 ECS 算出来的,是开发者用 enum 显式指定的。** 这消解了"slot 分配规则"这个待决项——根本不需要分配规则。

> 5th 分支已有同款模式 `SamplerSlot`(uint8_t enum + `SamplerSlotNameList` + static_assert),见 `inc/hgl/mtl/SamplerSlot.h`。6th 沿用并扩展为 TextureSlot + DataSlot 两套。

每个材质声明它要用哪几个 SSBO 类别 + 哪些 slot。Shader 声明侧(从 spec 派生)也声明它引用哪些——两边对齐即正确,不对齐就是 bug。

### 6.6 双池间接表的 C++ 组装

```cpp
// ECS 在 spec resolve 后,按 spec 组装两个间接表:
//
// TextureLayer:
//   for each instance:
//     for slot in 0..tex_per_instance:
//       tex_layer_ssbo[instance_id * tex_stride + slot] = resolved_handle;
//
// DataIndex:
//   for each instance:
//     for slot in 0..structs_per_instance:
//       data_index_ssbo[instance_id * struct_stride + slot] = struct_pool_index;
//
// 结构体池(PBRSurface):
//   [0]大理石参数  [1]金属参数  [2]木头参数  ...
//   DataIndex 引用这里的索引
```

stride 编译期 baked 进 shader(ShaderGen 从 spec 读)。同 spec 的所有 instance stride 一致。

---

## 7. ECS 资源元数据 Probe(决策前置)

你的两个例子(大理石 flat、MMORPG RGB16F vs RG8)共同说明:**决策依赖"加载后才知道的资源元数据"**。所以纹理加载必须拆成两步:

```
probe(轻量,读文件头/侧车) → 拿到 format/channels/degenerate 标记
   ↓
ECS resolve: recipe + 条件 + 元数据 → spec
   ↓
[shader 编译] 与 [纹素全量加载]   ← 两者可并行
```

### 7.1 probe 内容

- `format`(VK_FORMAT_R8G8B8A8 / RGB16F / RG8 / ...)
- `channel_count` / `channel_swizzle`
- `degenerate_flat`(法线贴图是否实际是平的——day-1 **靠作者显式 `treat_as_constant`,不做自动扫描**;自动扫描是后续优化,且会引入"扫完才能决策"的延迟)

### 7.2 Degenerate 判定:显式优先

day-1:**作者侧显式标记**(`is_flat` / `treat_as_constant`),不靠运行期扫纹素。
- 零成本、可预测、可调试
- ECS 收到标记后把该资源 **reclassify**:`Texture → Constant(vec3(0,0,1))`,于是生成的 spec 里该资源变 BakedResource,**不占 TextureLayer 槽、零采样**
- 这不是新机制,就是同一个资源抽象在 ECS 层的 reclassify

---

## 8. 全局 Layout

**纹理不占 per-material binding**。全局只有一个 bindless 大数组,所有纹理都在里面,靠 per-instance 的 handle 寻址。

### 8.1 set 划分

```
set 0: PerScene     camera UBO / 环境光 / 全局参数
set 1: PerView      视口/投影相关
set 2: PerMaterial  具名结构体池 SSBO(PBRSurface/ClearcoatData 等,按名字)
set 3: PerDraw      TransformData / TransformID + 间接表(TextureLayer/DataIndex)
set 4: Bindless     globalTex2D[] / globalTex2DArray[] / globalTexCube[] / globalTex3D[]
```

> set 2/3 内部用 SSBO 类别(§6.5)分配 binding 号,按具名类别固定(不是按材质变)。5th 时代那 4 套打架的 binding 逻辑(Resort 字母序 / 模板写死 / 枚举常量 / ResourceLayoutGenerator)直接**全部作废**——不是去统一,而是没东西需要统一了。

### 8.2 Bindless 数组按纹理类型划分

`sampler2D[]` / `sampler2DArray[]` / `samplerCube[]` / `sampler3D[]` 是不同 GLSL 类型,不能共享一个数组。所以"全局纹理数组"实际是四个,放在同一个 bindless set(set 4)里。

### 8.3 Handle 类型

建议**带类型的 handle**(不是带 tag 的裸 uint32):
```cpp
struct Tex2DHandle       { uint32_t index; };
struct Tex2DArrayHandle  { uint32_t index; };
struct TexCubeHandle     { uint32_t index; };
struct Tex3DHandle       { uint32_t index; };
```
让 C++ 类型系统替你挡住"把 CubeHandle 当 2D 用"的错误。

### 8.4 资源去重

两个 recipe 都用 `brick.png` → bindless 数组里**一条还是两条**?建议**按路径去重**:维护 `TexturePathRegistry: path → handle`。否则内存浪费且没法做"全局换贴图"。

---

## 9. Worked Example(端到端)

### 9.1 场景

一个 MMORPG 里,玩家衣服和 NPC 衣服用**同一个 MaterialRecipe**,但纹理资源不同:
- 玩家:`normal_player.png`(RGB16F,完整法线)
- NPC:`normal_npc.png`(RG8,双通道,需重建 z)

两者都引用同一份 PBRSurface 数据(共用)。

### 9.2 同一个 Recipe

```cpp
MaterialRecipe cloth_recipe {
    .model = PBR_LIT,
    .resources = {
        { Texture2D, "BaseColor", "res/cloth_basecolor.png",
            channels: {{basecolor, .rgb}} },
        { Texture2D, "Normal",    "res/cloth_normal.png",
            channels: {{normal_tan, .rgb}} },
        { Texture2D, "MR",        "res/cloth_mr.png",
            channels: {{metallic, .r}, {roughness, .g}} },
    },
    .struct_refs = {
        { "PBRSurface", "" },   // 单份,名字可空
    }
}
```

### 9.3 ECS resolve 出两个不同 Spec(共享 PBRSurface 结构)

**玩家(RGB16F 法线):**
```cpp
MaterializationSpec player_spec {
    .resources = {
        { Tex2D, handle=42, tex_slot=TextureSlot::BaseColor, channels:{{basecolor,.rgb}}, uv=default },
        { Tex2D, handle=43, tex_slot=TextureSlot::Normal,    channels:{{normal_tan,.rg}},
          normal_decode=Direct,                          // ← RGB16F 直通
          uv=default },
        { Tex2D, handle=44, tex_slot=TextureSlot::MR,       channels:{{metallic,.r},{roughness,.g}}, uv=default },
    },
    .struct_refs = {
        { "PBRSurface", "", data_slot=DataSlot::PBRSurface },
    },
    .ssbo_needs = { PBRSurface, TextureLayer, DataIndex },
    .tex_per_instance = 3,
    .structs_per_instance = 1,
}
```

**NPC(RG8 法线):**
```cpp
MaterializationSpec npc_spec {
    .resources = {
        { Tex2D, handle=45, tex_slot=TextureSlot::BaseColor, channels:{{basecolor,.rgb}}, uv=default },
        { Tex2D, handle=46, tex_slot=TextureSlot::Normal,    channels:{{normal_tan,.rg}},
          normal_decode=ReconstructRG8,                  // ← RG8 重建 z = sqrt(1-x²-y²)
          uv=default },
        { Tex2D, handle=47, tex_slot=TextureSlot::MR,       channels:{{metallic,.r},{roughness,.g}}, uv=default },
    },
    .struct_refs = {
        { "PBRSurface", "", data_slot=DataSlot::PBRSurface },
    },
    .ssbo_needs = { PBRSurface, TextureLayer, DataIndex },
    .tex_per_instance = 3,
    .structs_per_instance = 1,
}
```

注意 `normal_decode` 不同 → 两个 spec 哈希不同 → 两条 shader。**但 PBRSurface 结构相同**(都是标准 PBR 结构),且**两个 batch 共用同一份 PBRSurface 数据**(如果标量参数一样),只是 TextureLayer SSBO 的 handle 值不同。

### 9.4 两个间接表 SSBO 内容(C++ 动态写)

```
玩家 batch:
  TextureLayer SSBO: [42, 43, 44,  42, 43, 44,  ...]   (stride=3, 每 instance 3 个 handle)
  DataIndex SSBO:    [0,  0,  ...]                       (stride=1, 都引用 PBRSurface[0])

NPC batch:
  TextureLayer SSBO: [45, 46, 47,  45, 46, 47,  ...]   (stride=3)
  DataIndex SSBO:    [0,  0,  ...]                       (stride=1, 共用 PBRSurface[0])

PBRSurface pool:    [0] cloth_params  [1] metal_params  ...  (两 batch 都用索引 0)
```

### 9.5 双材质过渡例子(同类型两份)

```cpp
MaterialRecipe transition_recipe {
    .model = PBR_LIT,
    .struct_refs = {
        { "PBRSurface", "from" },   // 起始材质
        { "PBRSurface", "to" },     // 目标材质
    },
    // ... 资源略
}
// spec:
.struct_refs = {
    { "PBRSurface", "from", data_slot=DataSlot::PBRSurface },
    { "PBRSurface", "to",   data_slot=DataSlot::PBRSurfaceTo },
}
.structs_per_instance = 2;

// GLSL 里 materialization:
//   PBRSurface s_from = pbrsurface.surfaces[dataidx.data_indices[base + 0]];
//   PBRSurface s_to   = pbrsurface.surfaces[dataidx.data_indices[base + 1]];
//   float t = transition_factor;  // 来自哪? MI 或 vertex attr
//   ... mix ...
```

### 9.6 ShaderGen 生成两条不同 GLSL

**玩家 shader 的 GetPBRInput:**
```glsl
PBRInput GetPBRInput(uint instance_id, vec2 uv) {
    uint tex_base = instance_id * 3;                          // stride baked
    PBRInput m;
    m.basecolor  = texture(globalTex2D[texlayer.tex_layers[tex_base+0]], uv).rgb;
    m.normal_tan = texture(globalTex2D[texlayer.tex_layers[tex_base+1]], uv).rgb;   // Direct
    vec4 mr      = texture(globalTex2D[texlayer.tex_layers[tex_base+2]], uv);
    m.metallic   = mr.r;
    m.roughness  = mr.g;
    m.ao         = 1;   // 模型默认

    uint surf_idx = dataidx.data_indices[instance_id * 1 + 0];   // DataIndex 间接
    PBRSurface s  = pbrsurface.surfaces[surf_idx];
    m.emissive    = s.emissive;
    return m;
}
```

**NPC shader 的 GetPBRInput(注意 normal_decode 行不同):**
```glsl
PBRInput GetPBRInput(uint instance_id, vec2 uv) {
    uint tex_base = instance_id * 3;
    PBRInput m;
    m.basecolor  = texture(globalTex2D[texlayer.tex_layers[tex_base+0]], uv).rgb;
    vec2 n       = texture(globalTex2D[texlayer.tex_layers[tex_base+1]], uv).rg;
    m.normal_tan = vec3(n, sqrt(max(0, 1 - dot(n,n))));   // ReconstructRG8
    vec4 mr      = texture(globalTex2D[texlayer.tex_layers[tex_base+2]], uv);
    m.metallic   = mr.r;
    m.roughness  = mr.g;
    m.ao         = 1;

    uint surf_idx = dataidx.data_indices[instance_id * 1 + 0];
    PBRSurface s  = pbrsurface.surfaces[surf_idx];
    m.emissive    = s.emissive;
    return m;
}
```

Surface 模板(`surface/pbr_lit.glsl`)**一字不改**,只是 GetPBRInput 函数体不同。

### 9.7 大理石降级例子(同一机制)

作者给大理石地板的 recipe 带一张 normal map,但标记 `treat_as_constant=true`(或 ECS probe 后判定 flat):
- ECS 把 Normal 资源 **reclassify**:`Texture → Constant(vec3(0,0,1))`
- 生成的 spec 里 Normal 是 BakedResource,**不占 TextureLayer 槽**(`tex_per_instance` 从 3 降到 2)
- 生成的 GetPBRInput:`m.normal_tan = vec3(0,0,1);`(一行,零成本)
- "为什么降级"的原因(flat / 太远 / 贴图丢了)—— **全部不进 spec**,ECS 统一表达成"无 normal 资源,用默认"

---

## 10. 反模式(明确禁止)

| ❌ 反模式 | 为什么禁止 |
|---|---|
| ShaderGen 回头查 recipe | 违反单向信息流;Spec 不自包含 |
| ShaderGen 查 GPU 能力/画质 | 策略唯一驻地是 ECS;后端应可独立测试 |
| spec 里带指针 / "去查 recipe 第 3 项" | 违反 spec 自包含 |
| Surface 里写 `if(有法线) ... else ...` | 回到"一个 shader 服务所有";不可调试 |
| getter 内部运行期分支判断资源类型 | 必须 per-spec 单态化;inja 模板的价值就在此 |
| getter 返回有时切线、有时世界法线 | 重新引入分支;法线空间契约统一为切线空间 |
| **纹理 handle 放进结构体字段** | 纹理数量 per-recipe 变化;handle 必须进 TextureLayer SSBO |
| **每个材质重写一套结构体** | 结构体应开放注册、正交于材质模型;PBRSurface 等可多材质共用 |
| **结构体隶属材质模型**(PBR lit 私有 PBRSurface) | 结构体应独立注册;clearcoat/transition 也能引用 PBRSurface |
| **合并 TextureLayer 与 DataIndex** | 目标不同(纹理指向 bindless 采样器、结构体指向 SSBO 池);分开更清晰 |
| spec 保留 staging/committed 双缓冲 | 过渡期债务;spec 是决断,不存在"待定" |
| Texture 占 per-material binding | day-1 全 bindless;纹理只占全局数组一个 slot |
| 5th 的 tiered cache / triad / 双缓冲 | 全部不引入 |

---

## 11. 重构顺序(本文档的落地路径)

| Phase | 内容 | 产出 |
|---|---|---|
| **0** | 清场(删 6th 悬空引用)+ 固化黄金示例(带纹理球) | 干净可编译可运行的起点 |
| **1** | **定义资源契约**(本文 §2~§8)+ bindless 画一个贴图三角形的最小验证 | 契约文档 + 可跑的 bindless demo |
| **2** | Vulkan bindless 运行时(数组/pool/flags/handle 分配)+ 双池 SSBO 管理器 | 下半段 |
| **3** | ShaderGen getter 生成(inja 模板 per 资源源类型)+ 间接表 stride baked | 上半段 |
| **4** | ECS Task 流水线(recipe → probe → resolve spec → 编译+加载 → 组装间接表/结构体池) | 核心愿景 |
| **5** | GPU Scene(draw-item 已稳定) | 优化 |
| **6** | billboard/quad/terrain + PCG 资源扩展 | 功能补齐 |

> **Phase 1 必须包含可运行验证**。资源契约和 bindless layout 是同一件事的两面,光写文档验证不了——要跑通"bindless 画一个贴图三角形"才算 Phase 1 完成。

---

## 12. 待决问题(Phase 1 敲定时定)

- [x] ~~set 划分~~(已定 §8.1)
- [x] ~~DataIndex vs TextureLayer 分合~~(已定:分)
- [x] ~~PBRSurface 归属~~(已定:独立注册,正交于材质模型)
- [ ] PCG resource 的 day-1 范围(建议先 stub,只实现 Constant + Texture)
- [ ] handle 索引空间是否全局统一编号还是按类型分(§8.3 建议分)
- [ ] UV 变换 day-1 是否只支持 scale+offset,还是引入 UV-set 索引
- [ ] 跨变体的结构体池/PBRSurface 数据复用机制(day-1 建议先不优化)
- [ ] 资源去重的粒度(§8.4 建议按路径)
- [ ] TextureLayer slot 分配规则(spec 内按 resources 顺序?按 semantic 名?)
- [ ] DataIndex slot 分配规则(spec 内按 struct_refs 顺序?按 (type,name)?)
- [ ] 双材质过渡的过渡因子(t)从哪来——MI 字段?vertex attr?单独 SSBO?
- [ ] SSBO 类别枚举的扩展机制(未来加 ClearcoatData 等)

---

## 附:术语速查

| 术语 | 定义 | 层 |
|---|---|---|
| **Material Recipe** | 开发者意图,可任意复杂,源码 | 输入 |
| **Materialization Spec** | IR,自包含,前后端唯一契约,shader 缓存键 | 中间 |
| **Material Model** | surface 契约(input struct + getter + defaults),如 PBR lit | Layer 1 |
| **Materialization** | GetPBRInput 的生成函数体 | Layer 2 |
| **Resource** | vec4 数据源(Texture/Array/PCG/Constant) | Layer 3 |
| **BakedResource** | Constant + 无参 PCG,零 TextureLayer 槽 | Layer 3 子类 |
| **BoundResource** | Texture/Array/带参 PCG,占 TextureLayer 槽 | Layer 3 子类 |
| **双池模型** | 纹理池(bindless)+ 结构体池(具名 SSBO),两类资源各一个池 | 数据供给(§6) |
| **TextureLayer SSBO** | per-recipe,uint32 纹理 handle 数组,stride=tex_per_instance | 间接表(纹理) |
| **DataIndex SSBO** | per-recipe,uint32 结构体索引数组,stride=structs_per_instance(新增) | 间接表(结构体) |
| **PBRSurface / 结构体池** | 独立注册的标准结构体,正交于材质模型,可被多 instance 共用索引 | 池(结构体) |
| **StructRef** | recipe/spec 里对结构体的引用,(type, name, slot) | Spec 字段 |
| **SSBO Category** | TransformID/TransformData/TextureLayer/DataIndex/结构体池,开放注册 | §6 |
| **probe** | 轻量读资源元数据(format/channels/degenerate),决策前置 | ECS |
| **reclassify** | ECS 在 spec 生成时改变资源类型(如 Texture→Constant) | ECS |
| **NormalDecode** | 法线解码方式(Direct / ReconstructRG8 / ...),ECS 定、资源层不掺和 | Spec 字段 |
| **tex_per_instance** | spec 的 TextureLayer stride,编译期 baked | Spec 字段 |
| **structs_per_instance** | spec 的 DataIndex stride,编译期 baked | Spec 字段 |
