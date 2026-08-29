# ShaderGen 描述符 ABI 统一化 + Vertex 集独立 — 实施方案

> 状态：实施中（Phase 0–6 已完成，2026-08-29；仅剩 Phase 7 清理收尾）
> 日期：2026-08-29
> 分支基线：ShaderGenClean
> 关联文档：MATERIAL_SHADER_ARCHITECTURE.md、doc/mesh-shader-multi-object-per-draw-descriptor-set.md

---

## 一、背景与目标

### 1.1 问题

当前"语义 → set → binding"的映射在全引擎有 **四份平行真源**，靠人工纪律对齐：

| # | 真源 | 位置 | 形态 |
|---|---|---|---|
| 1 | C++ 常量 | `inc/hgl/common/DescriptorSetTypeDef.h:10-32` | 17 个 `constexpr int kSceneBinding*/kPerObjectBinding*` |
| 2 | GLSL 默认值 | `ShaderLibrary/common/descriptor_macros.glsl:50-95` | 14 个 `#ifndef #define`（手写） |
| 3 | 运行时名字表 | `src/Vulkan/VKShaderDescriptorSet.cpp:11-48` | `TryGetFixedBinding` 字符串→常量 |
| 4 | 生成期注入表 | `src/ShaderGen/MaterialShaderCompiler.cpp:740-754` | `kBindingDefineTable`（部分行查分配器） |

Vertex*、l2w、mesh_draw_params、CharQuad 三件套共 14 项 binding 号的**唯一赋值来源**是运行时的
`TryGetFixedBinding`（ShaderGen 侧 `AddSSBOVertex` 等均未传 preferred_binding）。

此外 Vertex* 系列 SSBO（几何 ABI，长期冻结）与 MeshDrawParams（每批/每 run 更新）同挤在
PerObject 集（Set 1），演化速度不同的两类 ABI 相互牵连，且 meshlet/nanite 落地后矛盾会加剧。

### 1.2 目标终态

```
Set 0  Scene     camera(0) sky(1) viewport(2) color_palette(3)     全局 layout，一帧一绑
Set 1  PerObject l2w(0) l2w_index(1) private_data_index(3)
                 mesh_draw_params(13) text_char*(14/15/16)         每批/每 run 更新
Set 2  Material  private_data(0=slot) texture_layer_rows(N)        per-material
Set 3  Bindless  texture2DArray[](0) sampler[](1)                  全局 layout
Set 4  Vertex    position(0) uv(1) ntb(2) index(3) color(4)
                 luminance(5) transform_id(6) size(7)              几何 ABI，长期冻结
```

Vertex 集放末尾（=4），**不破坏既有 set 编号**。三个真源、零份人工对齐：

| 产物 | 位置 | 消费者 |
|---|---|---|
| ABI 枚举 + 宏名表 | `inc/hgl/common/DescriptorSetTypeDef.h` | C++ 全体 + 生成工具 |
| `descriptor_macros.glsl` | `ShaderLibrary/common/`（**生成物**） | 全部 GLSL |
| 资源目录表 | `inc/hgl/mtl/DescriptorResourceCatalog.h`（新增） | ShaderGen 注册 + 运行时绑定 |

meshlet/nanite 展望：顶点集未来容纳 meshlet descriptor / vertex reuse table / cluster bounds
等新 binding；MeshDrawParams / task 参数留在 PerObject（易变集）。

### 1.3 实施总原则

1. **七个阶段（Phase 0–7），每阶段独立提交，提交后必须可以全量 REBUILD & TEST。**
2. 缓存失效只发生两次：Phase 3（GLSL 文本变、预处理后等价）、Phase 5（set/binding 编号变更）。
   其余阶段 GLSL 输出逐字节不变——**"cache hash 不变"本身是那些阶段的验收指标**。
3. 出问题 `git revert` 单提交即回到上一绿色状态；两个缓存失效点都伴随"清 cache 重编 +
   回归门 golden 更新"，不存在静默脏缓存。

---

## Phase 0 — 基线快照（无代码改动）

1. 全量构建 + 跑 `ShaderResourceSchemaRegressionGate`（已接入 ctest，
   `src/Tools/ShaderGen/CMakeLists.txt:51,66`），确认绿色基线。
2. 用现有样例工程跑一帧 3D mesh / 文本 / 线段三类场景，记录正常渲染截图。
3. `ShaderLibrary/common/descriptor_macros.glsl` 当前内容存档（Phase 2 要做逐字节比对）。
4. 记录 `cache/` 产物 hash 列表，后续各阶段用它判断"预期内缓存失效"。

**验收**：回归门绿色；三类场景截图存档。

---

## Phase 1 — ABI 枚举头（纯新增，零行为变化）

**改动文件：`inc/hgl/common/DescriptorSetTypeDef.h`**（就地扩展，不新建文件，避免 include 图变动）

新增三个枚举，**数值与现有常量完全一致**：

```cpp
enum class SceneBinding : int
{
    Camera=0, Sky=1, Viewport=2, ColorPalette=3
};

enum class PerObjectBinding : int
{
    L2W=0, L2WIndex=1, PrivateDataIndex=3,
    MeshDrawParams=13, TextCharInfo=14, TextCharStyle=15, TextCharInstance=16,
    // 过渡期临时条目（Phase 5 移出，届时删除）：
    VertexPosition=4, VertexUV=5, VertexNTB=6, VertexIndex=8,
    VertexColor=9, VertexLuminance=10, VertexTransformID=11, VertexSize=12
};

enum class VertexBinding : int;   // Phase 5 才定义（Position=0..Size=7）
```

原有 17 个 `constexpr int kSceneBinding*/kPerObjectBinding*` 改为枚举别名（调用点零改动）：

```cpp
constexpr const int kPerObjectBindingL2W = int(PerObjectBinding::L2W);
```

新增**宏名规范表**（Phase 2 生成器的唯一输入；放同一头文件使工具直接 include、无需解析）：

```cpp
struct DescriptorBindingMacroSpec
{
    DescriptorSetType set_type;
    const char *set_macro;      // "VERTEX_SET"
    const char *binding_macro;  // "VERTEX_POSITION_BINDING"
                                // 注意历史拼写不规则（TRANSFORMID/CHARINFO），表驱动、不推导
    int binding;
    const char *comment;        // 生成文件的行注释
};
constexpr const DescriptorBindingMacroSpec kDescriptorBindingMacros[] = { /* 每宏对一行 */ };
```

配 ABI 锚点断言：

```cpp
static_assert(int(PerObjectBinding::MeshDrawParams)==13);
static_assert(int(PerObjectBinding::TextCharInstance)==16);
```

**枚举值必须显式写死（不许编译器自动续号）+ static_assert 锚点**：中间插一项若导致全体静默
重编号即为 ABI 破坏（所有 SPV 重编、已发布缓存全失效），锚点断言让这种事故在编译期暴露。

**验证**：全量 rebuild → 回归门绿色 → **cache hash 不变**（GLSL 未动）。

---

## Phase 2 — 宏生成工具 + descriptor_macros.glsl 转为生成物（内容逐字节一致）

**新增 `src/Tools/ShaderGen/DescriptorMacroGen.cpp`**（单文件工具，仿
`VulkanPhysicalDeviceProfileCollector` 的 CMake 接法）：

- `--emit`：include `DescriptorSetTypeDef.h`，遍历 `kDescriptorBindingMacros`，向 stdout 输出
  完整 `.glsl`（**含开头 `@ulre` 元数据块**——`GLSLCodeModuleRegistry` 解析所必需，生成器原样输出）。
- `--verify <path>`：生成到临时串与文件比对，不一致 exit 1 并打印首个差异行。

**CMake 接线**（`src/Tools/ShaderGen/CMakeLists.txt`）：

```cmake
add_executable(DescriptorMacroGen DescriptorMacroGen.cpp)
add_test(NAME VerifyDescriptorMacros
         COMMAND DescriptorMacroGen --verify ${CMAKE_SOURCE_DIR}/ShaderLibrary/common/descriptor_macros.glsl)
```

**验收标准 = 生成器输出与现有手工文件逐字节一致**（不含 banner）。这一步本质是用现实校准
生成器——若不一致，修生成器而不是修文件。此时尚不替换仓库文件（或替换为内容完全相同的版本）。

**验证**：`DescriptorMacroGen --verify` 通过；全量 rebuild；回归门绿色；cache hash 不变。

---

## Phase 3 — ShaderGen 停止注入固定宏（GLSL 文本变化一次，预处理后等价）

**改动文件：`src/ShaderGen/MaterialShaderCompiler.cpp`**

- 删除：`BindingDefineSpec`、`kBindingDefineTable`（:731-754）、`AppendBindingDefine`
  （:756-779）、`BuildBindingPreamble`（:781-803）。
- `AssembleFinalGLSL`（:1021）签名去掉 `binding_preamble` 参数；ms/fs 注入串相应缩短。
- `descriptor_macros.glsl` 重新生成，这次**加 banner**
  （`// GENERATED by DescriptorMacroGen — DO NOT EDIT`）：今后手改该文件会被 Phase 2 的
  verify 测试当场拦下。
- 顺带删除 `MaterialShaderCompiler.cpp:722-726` "先定义者胜 / 同值不同体报错"注释描述的坑
  ——它随注入一起消失。

**前置确认（已核实）**：

- mesh 生成体无条件 include `descriptor_macros.glsl`（`src/ShaderGen/common/MeshShaderHeaderGen.h:40`）；
  FS 模板经 `surface_interface.glsl` 等引入。
- 宏默认值与被删注入值恒等（同源于 Phase 1 枚举）。

**验证（本阶段核心测试——预处理等价性）**：

1. Phase 3 前对回归门中每个材质 dump `FinalGLSL`（ms/fs 各一）。
2. 改动后同样 dump，两侧各跑 `glslang -E`（仅预处理），剥离 `# 行号` 标记后 diff——**必须全等**。
   这证明删掉的 `#ifndef` 块与文件内定义值相同。
3. 全量 rebuild、回归门绿色、清 cache 后全量重编一次（**预期内失效点 ①**）、三类样例场景渲染
   比对 Phase 0 截图。

---

## Phase 4 — binding 号真源回迁 ShaderGen（运行时名字表退役）

现状：`TryGetFixedBinding`（`src/Vulkan/VKShaderDescriptorSet.cpp:11-48`）是 l2w/l2w_index/
private_data_index/Vertex*/mesh_draw_params/char* 共 14 项 binding 的唯一赋值来源。本阶段把
数字经 `preferred_binding` 从生成侧下发，**数字不变**。

### 4a. 先梳理赋值时序（不改码，写进提交说明）

`ShaderDescriptor.set/binding` 在 `AddDescriptor`（生成期，`BuildVSIndexTableDecls` 读取它）与
`ReassignBindingsForSet`（运行期，`VKShaderDescriptorSet.cpp:62-144`）两处被写。确认 `AddDescriptor`
的 preferred→binding 赋值路径；若两处逻辑有分叉，本阶段顺手收敛为
"AddDescriptor 即赋值、Reassign 只做冲突校验"。

### 4b. `src/ShaderGen/ShaderBuildContext.{h,cpp}` — 语义化注册函数全部带 binding

```cpp
bool AddSSBOVertex(uint32_t flag_bits, const ShaderBufferSource &ss, int preferred_binding);
bool AddSSBOVertexIndex(uint32_t flag_bits);                    // 内部传 int(PerObjectBinding::VertexIndex)
bool AddSSBOMaterialPrivateDataIndex(uint32_t flag_bits);       // 内部传 int(PerObjectBinding::PrivateDataIndex)
bool AddSSBOStruct(uint32_t flag_bits, const ShaderBufferSource &ss, int preferred_binding); // 新重载
bool SetLocalToWorld(uint32_t flag_bits);                       // SBS_LocalToWorld → preferred=L2W
```

### 4c. `src/ShaderGen/MaterialShaderCompiler.cpp`

- `kDescriptorRegisterTable`（:516）每行增加 `int binding` 字段（值 = 枚举），
  `RegisterOp::AddSSBOStruct/AddSSBOVertex` 分支改传 binding；`RegisterOp::SetLocalToWorld` 走 4b。
- `kCharQuadSSBOTable`（:636）每行加 binding（TextCharInfo/Style/Instance），
  `RegisterCharQuadSSBOs` 的 `AddSSBO` 调用带 `preferred_binding`。

### 4d. `src/Vulkan/VKShaderDescriptorSet.cpp`

- 删除 `TryGetFixedBinding`（:11-48）与 `DynamicBindingStart`（:50-60）；
  `ReassignBindingsForSet` 删掉路径 2/3（名字表 + 动态递增），只留 preferred 优先 + 冲突报错；
  任何 SSBO 在 PerObject/Material 集拿到 `binding==-1` 都硬失败（现有 `index_driven` 报错分支
  推广到全部固定集）。
- 附带消灭一个潜在 bug：`DynamicBindingStart(PerObject)=8` 的注释称"固定成员占 0..7"，
  实际已占到 16——将来若有动态成员必撞 14/15/16，删掉即根治。
- `DynamicBindingStart` 在本文件之外零用户（已 grep 验证）。

### 4e. `src/ecs/support/TextRenderPipeline.cpp:254-258`

裸数字 `BindSSBO(14/15/16,…)` 改用 `int(PerObjectBinding::TextCharInfo)` 等枚举常量。

**验证**：全量 rebuild；回归门绿色；**cache hash 不变**（这是"数字未动"的最强证据）；样例场景比对。

---

## Phase 5 — Vertex 集落地（唯一编号变更点，预期内缓存全量重编 ②）

### 5.1 枚举与类型（`inc/hgl/common/DescriptorSetTypeDef.h`）

```cpp
enum class DescriptorSetType:int
{
    Unknow=-1,
    Scene=0, PerObject, Material, Bindless,
    Vertex,                        // ← 末尾追加，不破坏既有编号
    ENUM_CLASS_RANGE(Scene,Vertex)
};
```

- `DescriptSetTypeName[]` 追加 `"Vertex"`；`DESCRIPTOR_SET_TYPE_COUNT` 经 RANGE_SIZE 自动变 5。
- `PerObjectBinding` 删除 Vertex* 八个过渡条目及对应别名常量
  （4b/4c 已把调用点迁到表驱动，无直接引用残留——grep 验证）。
- 新增正式枚举：

```cpp
enum class VertexBinding : int
{
    Position=0, UV=1, NTB=2, Index=3,
    Color=4, Luminance=5, TransformID=6, Size=7
};
static_assert(int(VertexBinding::Size)==7);
```

- `kDescriptorBindingMacros`：Vertex 八行改为
  `{DescriptorSetType::Vertex,"VERTEX_SET","VERTEX_POSITION_BINDING",int(VertexBinding::Position),…}`。

### 5.2 SBS 表（`inc/hgl/graph/ShaderBufferSources.h`）

`SBS_VertexPosition`…`SBS_VertexIndex` 八项的 `set_type` 改 `DescriptorSetType::Vertex`。

### 5.3 重新生成 `descriptor_macros.glsl`

`VERTEX_SET 4`、`VERTEX_*_BINDING 0..7`；`PER_OBJECT_SET 1` 与其余宏不变。
**s1_* 模块零改动**（它们只认宏名，不感知编号）。

### 5.4 生成侧（`src/ShaderGen/`）

- `common/DescriptorBuilderCommon.h`：`PushVertex*` 七个函数的 set 参数从字面 `PerObject`
  改 `SBS_*.set_type`；`PushManifestSSBO` 的 `switch(ssbo_type)`（:352-382）中 Vertex* 四 case
  的 set 改 Vertex。
- `MaterialShaderCompiler.cpp`：`kDescriptorRegisterTable` Vertex 八行 binding 改
  `int(VertexBinding::*)`。
- `ValidateShaderResourceSchema` / `GetExpectedSetType`（`inc/hgl/mtl/ShaderResourceSchema.h:90-117`）：
  Vertex* 语义期望集改 Vertex。

### 5.5 运行时管线布局（`src/Vulkan/pipeline/VKPipelineLayoutData.cpp`）——重点坑

- :53 的循环 `for(i=Scene; i<Bindless; ++i)` 只覆盖 set 0..2，**Vertex=4 落在循环之外**。
  改为遍历 `DescriptorSetType::RANGE` 内所有非 Scene / 非 Bindless 成员
  （Scene :57 特判、Bindless :129 特判保持，`bindless_set_index` 仍 = 3）。
- `MaterialDescriptorManager` 的 per-set DSL CI 数组若按 `DESCRIPTOR_SET_TYPE_COUNT` 定容则
  自动支持；grep 审计所有按 4 定容的数组（`ShaderDescriptorSetArray` 等）。

### 5.6 MP 生命周期与绑定

- `src/SceneGraph/module/ShaderProgramManager.cpp` / `src/SceneGraph/module/ShaderProgramFinalizeFlowAdapter.cpp`：
  `BuildShaderProgramFinalizePlan` 按 `hasSet()` 生成 MP——Vertex 集自动获得 per-program MP
  （现有机制直接复用，Scene 跳过逻辑保留）。
- `src/ecs/support/PipelineMaterialRenderer.cpp::Draw`（:125-196）：Vertex*/VertexIndex 的
  `BindSSBO` 目标从 `per_object_mp` 改为 `vertex_mp`；新增 vertex MP 的 clone/bind 时序，
  **完全镜像 per_object_mp 的现有生命周期**。`BindMeshDrawParamsView` 保持在 per_object。
- `src/ecs/systems/render/RenderDescriptorBindingSystem.cpp::apply_requirement`（:716+）：
  Vertex* case 的 `bind_ssbo` 目标 MP 按 `req.set_type` 解析（bind_ssbo lambda 增加 vertex MP
  分支）；buffer 来源解析（`geometry->GetVAB`）不动。
- `PipelineMaterialRenderer.cpp:174-185` 的 `geometry_vab_bindings` 静态表同步。

### 5.7 设备能力门槛

`PhysicalDeviceLimitsLite` 校验处加一条 `max_bound_descriptor_sets >= 5` 断言
（Vulkan 规范 `maxPerStageDescriptorSets` 下限是 4；引擎已硬性要求 mesh shader 扩展，
实际设备都支持 8+，但契约层应显式声明）。

**验证**：全量 rebuild（含工具）→ `DescriptorMacroGen --verify` 绿 → 清 cache 全量重编 →
回归门（更新 golden：diff 中**只允许 set/binding 号变化**）→ 三类样例场景截图比对 →
如环境可用，开 Vulkan validation layer 跑一帧确认无 layout 不匹配。

---

## Phase 6 — 资源目录表收敛（6a 生成侧 / 6b 运行时，两个独立提交）

**新增 `inc/hgl/mtl/DescriptorResourceCatalog.h`**：

```cpp
enum class ResourceCatalogClass { SceneGlobal, VertexGeometry, PerDraw, MaterialData };

struct ResourceCatalogEntry
{
    DescriptorSemantic            semantic;        // VertexPosition / LocalToWorld / MeshDrawParams / ...
    ResourceCatalogClass          cls;
    const ShaderBufferSource     *sbs;             // SceneGlobal 为 nullptr（UBO 另有来源）
    DescriptorSetType             set_type;
    int                           binding;         // -1 = per-material 动态（MaterialData）
    bool                          engine_builtin;  // ValidateDefinitionCapabilitySubset 的 allowed 判定
};
constexpr const ResourceCatalogEntry kResourceCatalog[] = { /* ~24 行 */ };

inline const ResourceCatalogEntry *FindResourceCatalogEntry(DescriptorSemantic);
inline const ResourceCatalogEntry *FindResourceCatalogEntry(SSBOType);
```

binding 值直接引用 Phase 1/5 枚举；与 `kDescriptorBindingMacros` 分工——后者只管 GLSL 宏文本
（common 层），目录表管资源语义（mtl 层），互不依赖。

### 6a. 生成侧改写（`src/ShaderGen/`）

- `kDescriptorRegisterTable` + `RegisterOp` switch（`MaterialShaderCompiler.cpp:490-618`）→
  遍历目录表按 `cls` 三分支（Global 跳过 / FixedABI 批量注册 / MaterialData per-material 路径）；
  `EnsureIndexTableSSBOs`（:686）删除——"有 MaterialPrivateData 必有 Index 行表"改为目录表
  声明式保证（MaterialData 类附行表条目）。
- `DescriptorBuilderCommon.h`：`PushVertex*` 七函数 → 目录表 `VertexGeometry` 行循环生成；
  `PushManifestSSBO` 的 ssbo_type switch → `FindResourceCatalogEntry(ssbo_type)`。
- `ValidateDefinitionCapabilitySubset`（`MaterialShaderCompiler.cpp:196-302`）的 50 行 switch →
  读 `engine_builtin` + 三条类规则。
- `ShaderResourceSchema.h` 的 `GetExpectedSetType`/`GetDefaultDescriptorNameBySemantic`
  改为目录表派生（消掉又一份平行表）。

### 6b. 运行时改写

- `PipelineMaterialRenderer.cpp:143-185`：vab switch + `geometry_vab_bindings` 二合一为
  目录表 `VertexGeometry` 循环。
- `RenderDescriptorBindingSystem.cpp:944-958` 的 semantic→VertexSemantic lambda →
  目录表字段（catalog 行加 `VertexSemantic vab_semantic`，仅 VertexGeometry 行有效）。

**验证（每子阶段）**：全量 rebuild、回归门（**GLSL 输出应与 Phase 6 前完全一致**——本阶段是
纯结构重构，cache hash 不变是硬验收指标）、样例场景。

---

## Phase 7 — 清理收尾（独立小提交，可按需拆分）

- `ShaderBuildContext`：`AddSSBO` 八重载收敛（const char* 转发层删除）；构造函数补
  `local_to_world_max_count/local_to_world_stage_bits` 初始化（现存未初始化成员，
  `src/ShaderGen/ShaderBuildContext.cpp:106-117`）。
- `ReassignBindingsForSet` 非集合（按名排序）逻辑删除（binding 全由 preferred 决定后无意义）。
- `SerializedVertexEntry` 去留决策（运行时零消费，仅工具/回归门使用——保留则在头注释声明
  "仅 ShaderGen 内部中间表示"）。
- 命名残留：`GetVertexStageKey`→`GetMeshStageKey`（`src/ShaderGen/ShaderArtifactStore.cpp:15`）、
  `DescriptorSetType::Unknow` 拼写修正（改标识符不改值，序列化契约不受影响）。
- `inc/hgl/mtl/MaterialShaderCompiler.h:72-75` 过期注释（sampler 保底说法）修正。

---

## 风险矩阵与回滚

| 阶段 | GLSL/缓存影响 | 主要风险 | 回滚方式 |
|---|---|---|---|
| 0–2 | 无 | 无（生成器校准期） | — |
| 3 | 文本变、SPV 等价，失效① | 某模板未 include macros | 预处理 diff 当场暴露 |
| 4 | 无（cache hash 不变是验收项） | binding 赋值时序分叉 | 4a 已先梳理 |
| 5 | set/binding 变，失效② | VKPipelineLayoutData 循环外漏（5.5）；按 4 定容的数组 | 每子步 grep 审计清单 |
| 6 | 无（输出不变是验收项） | 纯重构 | git revert 单提交 |
| 7 | 无 | — | — |

每阶段一个 commit，出问题 `git revert` 即回到上一绿色状态；两个缓存失效点都伴随
"清 cache 重编 + 回归门 golden 更新"，不存在静默脏缓存。

---

## 动手顺序

严格递增依赖：**1→2 建真源，3→4 拆双写，5 才动编号，6 收结构，7 清尾巴。**
Phase 1–2 为纯增量（可独立验证）；Phase 3–4 消灭双写（数字不变）；Phase 5 单次破坏
（全量重编一次）；Phase 6 纯结构收敛（输出不变）。
