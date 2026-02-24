# GPU 资源绑定全面重构方案

> 调研范围：descriptor/descriptor set/UBO/SSBO 绑定系统 + Vertex Input Attribute 处理 + 未来 SSBO 化顶点数据的迁移路径。
> ECS 层已完成重构，本文档重点在 ECS 之下的图形层。

---

## 一、当前设计概览（架构地图）

```
MaterialCreateConfig                         用户端 —— 描述"我要什么材质"
  └─ MaterialCreateInfo                      中间层 —— 持有 ShaderCreateInfo* + MaterialDescriptorInfo
       ├─ ShaderCreateInfo{V/G/F}             组装 GLSL 字符串 → 编译 SPV
       └─ MaterialDescriptorInfo             统计 set/binding 号

MaterialManager::CreateMaterial()            运行时 —— 从 MCI 创建 GPU 对象
  └─ Material                                持有 ShaderModuleMap + VertexInput + mp_array[]
       └─ MaterialParameters[N]              每个 DescriptorSetType 对应一个 DescriptorSet

DescriptorSet                                包装 vkUpdateDescriptorSets
  └─ BindUBO/BindSSBO/BindTexture(binding, buf)   按 binding 号写 VkWriteDescriptorSet

RenderCmdBuffer::BindDescriptorSets(mtl)     渲染时 —— 自动遍历 desc_binding[] + mp_array[]
  └─ vkCmdBindDescriptorSets(…, ds[], …)

VertexInput / VertexInputConfig / VIL        顶点格式描述 + VAB 绑定配置
VABList / GeometryDataBuffer                 渲染时每 draw call 绑定的 VAB
VertexDataManager                            管理 VAB/IBO 内存池（用于批量渲染）
```

---

## 二、发现的设计问题

### 2.1 名称字符串查找贯穿整个绑定链

**问题描述**

用户调用 `material->BindTextureSampler(DescriptorSetType::PerMaterial, "TextureBaseColor", tex, sampler)` 时，链路如下：

```
Material::BindTextureSampler(set_type, name, …)
  → mp_array[set_type]->BindTextureSampler(name, …)
    → desc_manager->GetTextureSampler(set_type, name)   // 字符串查找 → 返回 binding 号
      → descriptor_set->BindTextureSampler(binding, …)
```

每次都对 `AnsiString` 做哈希/比较查找。`Material::Update()` 被每帧调用一次，贯穿所有 `mp`。这在每帧绑定时是纯粹的字符串散列开销，而 `binding` 号在 material 创建后就不会变了。

**影响**：每帧渲染多个材质时浪费 CPU 时间。

---

### 2.2 `DescriptorSet` 绑定后无法更新（一次性语义 vs. 每帧语义混淆）

**问题描述**

`DescriptorSet::BindUBO()` 内有：

```cpp
if(binded_sets.Contains(binding)) return(false);  // 拒绝重复绑定
```

但 `DescriptorSet::Update()` 在 `vkUpdateDescriptorSets` 后立刻调用 `Clear()`，清空所有绑定。这意味着每帧必须重新 `BindXxx` + `Update()`。

然而上层 `Material::BindTextureSampler()` 并不是每帧调用的（示例代码只在初始化时调用一次），而 `Material::Update()` 会每帧触发 `mp->Update()`（又调 `descriptor_set->Update()`，又调 `Clear()`）。

这导致**逻辑矛盾**：
- 纹理绑定是初始化时一次性的 → 应写入并保持（persistent descriptor set）。
- UBO/SSBO 绑定可能每帧都在变（ring buffer 地址变化） → 需要每帧更新。

两类操作混用同一套 `Bind + Update + Clear` 机制，且没有明确的语义分隔。

---

### 2.3 `DescriptorSetType` 枚举过于宽泛，实际使用稀疏

**问题描述**

```cpp
enum class DescriptorSetType { Unknow, RenderTarget, Camera, World, Static, Global, PerFrame, PerMaterial, Instance }
// 共 9 个类型
```

`Material::mp_array[DESCRIPTOR_SET_TYPE_COUNT]` 预分配 9 个槽位，但大多数材质只用到 2-3 个（PerMaterial + Camera 或 PerMaterial + RenderTarget）。

`BindDescriptorSets()` 中：

```cpp
ENUM_CLASS_FOR(DescriptorSetType, int, i)
{
    mp = mtl->GetMP((DescriptorSetType)i);
    if(mp) { mp->Update(); ds[count]=mp->GetVkDescriptorSet(); ++count; }
}
vkCmdBindDescriptorSets(…, 0, count, ds, 0, 0);
```

这里把 set=0 传给 `vkCmdBindDescriptorSets`，但 `mp_array` 中跳过的槽位会导致 descriptor set 在 pipeline layout 期望的 set 号位置与实际提交的顺序错位（如果 Camera=set2 而中间 World=set1 为空，提交两个 ds 时 set 号就对不上）。

**这是一个潜在的渲染错误**。Vulkan 要求 `pDescriptorSets[i]` 对应 `firstSet + i` 号 set，不能跳过。

---

### 2.4 `DescriptorBinding`（自动绑定器）只处理了 UBO，遗漏 SSBO 和 Texture

```cpp
// VKDescriptorBindingManage.cpp
BindUBO(mp, bma[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER], false);
BindUBO(mp, bma[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC], true);
// SSBO 和 Texture 部分已注释掉，未实现
```

`DescriptorBinding::Bind()` 只能自动绑定 UBO，Camera 等 SSBO 需要手动调用 `material->BindSSBO()`，割裂了"自动绑定"的统一语义。

---

### 2.5 `ShaderDescriptorSet` 里的 `count` 字段依赖副作用递增

```cpp
// VKShaderDescriptorSet.cpp
ShaderDescriptor *ShaderDescriptorSet::AddDescriptor(uint32_t ssb, ShaderDescriptor *new_sd)
{
    if(descriptor_map.Get(new_sd->name, sd)) { … sd->stage_flag |= ssb; return sd; }
    else { …; count++; return new_sd; }  // count 作为 set 内的 item 数量，依赖这里的 side effect
}
```

`count` 的实际语义是"binding 数量"，但在 `MaterialDescriptorInfo::Resort()` 前它实际是"已注册的不重名描述符数"。两个阶段的含义不同，且没有断言保护。

---

### 2.6 Vertex Input 与材质描述符完全独立，无法联动迁移

**问题描述**

目前 TransformID 和 MaterialInstanceID 走 VAB（顶点输入流），`VertexInputGroup` 枚举中有专门的 `TransformID` 和 `MaterialInstanceID` 分组：

```cpp
enum class VertexInputGroup:uint8 { Basic, TransformID, MaterialInstanceID, JointID, JointWeight }
```

`VertexInputConfig::CreateVIL()` 对这两个分组有特殊处理（hardcode format/inputRate/stride）。

当迁移到 SSBO 模式（`HGL_L2W_USE_SSBO=1`，`HGL_MI_USE_SSBO=1`）时，TransformID/MI_ID 仍然通过 VAB 传入（`transform_vab`, `material_instance_vab`），只有"数据本身"（矩阵/颜色等）放入 SSBO。VAB 里的 ID 索引依然存在。

**未来潜在迁移目标**：彻底消除 TransformID/MI_ID 的 VAB，改由 gl_InstanceIndex 或其他方式索引 SSBO，届时需要同步移除 `VertexInputGroup::TransformID / MaterialInstanceID` 枚举项、删除 VIL 中的特殊处理、以及修改 shader 侧逻辑。

当前两个子系统（VIA/VIL 与 DescriptorSet）之间没有任何联动协议，迁移时需要改动多处且容易遗漏。

---

### 2.7 `VILConfig` 的覆盖机制语义不清晰

`VILConfig` 允许按名称覆盖格式（如把 Position 从 `VEC3` 改成 `VEC2I`），但：
- 覆盖后的 `VIL` 和默认 `VIL` 被一起保存在 `VertexInput::vil_sets` 中（无法删除），导致每种 VILConfig 组合都会创建并永久保存一个 VIL 对象。
- `VertexInputLayoutHash` 有 `Position/Normal/…` 字段，但实际上并不参与 VIL 的选择，只是一个辅助工具，容易引起误解。

---

### 2.8 `MaterialCreateInfo` 中 UBO/SSBO 模式用 `#ifdef` 切换，产生两套字段

```cpp
#if defined(HGL_MI_USE_SSBO) && HGL_MI_USE_SSBO
    SSBODescriptor *mi_ssbo;
#else
    UBODescriptor *mi_ubo;
#endif
```

这导致整个 MaterialCreateInfo 的二进制布局随编译宏变化，序列化/反序列化材质文件时如果编译选项不同会产生静默错误。

---

### 2.9 着色器生成中 GLSL 代码片段全部是 `constexpr const char *`，硬编码在 `.h` 文件中

`MFGetPosition.h`、`MFCommon.h` 等文件将 GLSL 代码片段直接写死在 C++ 头文件中。修改任何 shader 逻辑都需要重编译 C++ 代码。无法热重载，无法支持编辑器。

---

## 三、重构方案（分阶段，可独立执行）

### 阶段 0：修复渲染正确性 bug（优先级：紧急）

**描述符 set 号错位问题**

修改 `RenderCmdBuffer::BindDescriptorSets()` 中的 `vkCmdBindDescriptorSets` 调用：

```cpp
// 当前（错误）：全部从 firstSet=0 开始，跳过空 mp 导致 set 号错位
vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
    pipeline_layout, 0, count, ds, 0, 0);

// 修改后：每个非空 mp 单独提交，使用其真实 set 号
ENUM_CLASS_FOR(DescriptorSetType, int, i)
{
    mp = mtl->GetMP((DescriptorSetType)i);
    if(!mp || !mp->IsReady()) continue;
    VkDescriptorSet ds = mp->GetVkDescriptorSet();
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline_layout, i /*firstSet = actual set number*/, 1, &ds, 0, 0);
}
```

或者在 `MaterialDescriptorManager` 中记录每个已激活 set 的 `{set_index, VkDescriptorSetLayout}` 对，在 `PipelineLayoutData` 中使用连续无空洞布局（当前 Vulkan 驱动不接受中间有洞的 set 数组）。

---

### 阶段 1：消除名称字符串查找（性能优化）

**目标**：让 binding 号在 `Material` 创建后就直接缓存，绑定时 O(1) 查找。

**方案**：

为 `MaterialDescriptorManager` 增加预计算的 "slot" 表：

```cpp
// 新增
struct BoundDescriptor {
    int             binding;        // Vulkan binding 号
    VkDescriptorType type;
    DescriptorSetType set_type;
    // 可选：用于运行时验证
    AnsiString      name;           // debug-only
};

using BoundDescriptorTable = UnorderedMap<AnsiString, BoundDescriptor>;
```

`Material` 创建时（`CreateMaterial`）一次性解析所有描述符的 binding 号，缓存在 `BoundDescriptorTable`。后续的 `BindUBO/BindSSBO/BindTexture` 直接查表，不再走 `desc_manager->GetXxx(set_type, name)`。

---

### 阶段 2：分离"一次性绑定"与"每帧绑定"（语义修复）

**目标**：纹理等静态绑定只写一次描述符集，UBO/SSBO ring buffer 绑定每帧更新。

**方案**：

在 `MaterialParameters` 中增加两类绑定模式：

```cpp
class MaterialParameters {
    // persistent_wds: 初始化时写入，不清空
    ValueArray<VkWriteDescriptorSet> persistent_wds;
    // dynamic_wds: 每帧写入，Update() 后清空
    ValueArray<VkWriteDescriptorSet> dynamic_wds;

    bool persistent_committed = false;

public:
    bool BindTextureSampler(…);  // 写入 persistent_wds
    bool BindUBO(…);             // 写入 dynamic_wds (ring buffer)
    bool BindSSBO(…);            // 写入 dynamic_wds

    void Update() {
        if(!persistent_committed) {
            vkUpdateDescriptorSets(…, persistent_wds, …);
            persistent_committed = true;
        }
        if(!dynamic_wds.IsEmpty()) {
            vkUpdateDescriptorSets(…, dynamic_wds, …);
            dynamic_wds.Clear();
        }
    }
};
```

---

### 阶段 3：DescriptorBinding 补全 SSBO 和 Texture 自动绑定

补全 `DescriptorBinding::Bind()` 中已注释的 SSBO 和 Texture 分支：

```cpp
// BindSSBO
for (const auto &[name, binding] : bma[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER])
    if (const IGPUBuffer *gpu = GetSSBO(name)) mp->BindSSBO(binding, gpu);

for (const auto &[name, binding] : bma[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC])
    if (const IGPUBuffer *gpu = GetSSBO(name)) mp->BindSSBO(binding, gpu, true);

// BindTextureSampler (for global/camera textures that don't change per material)
for (const auto &[name, binding] : bma[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER])
    if (Texture *tex = GetTexture(name)) mp->BindTextureSampler(binding, tex, default_sampler);
```

---

### 阶段 4：消除 MaterialCreateInfo 中的 `#ifdef` 二态字段

**目标**：`MaterialCreateInfo` 的内存布局不随编译宏变化。

**方案**：把 `mi_ssbo/mi_ubo` 和 `l2w_ssbo/l2w_ubo` 统一为 `ShaderDescriptor*`，在运行时根据 `RenderOptions` 决定语义：

```cpp
// 替代 #ifdef 的运行时判断
enum class BufferBackend { UBO, SSBO };

struct AssignBuffer {
    ShaderDescriptor *descriptor = nullptr;  // UBODescriptor 或 SSBODescriptor
    BufferBackend     backend    = BufferBackend::UBO;
};

AssignBuffer mi_assign;
AssignBuffer l2w_assign;
```

`RenderOptions.h` 中保留宏，但只用于初始化上述结构体，不再影响成员布局。

---

### 阶段 5：Vertex Input 与描述符联动协议（为 SSBO 顶点迁移铺路）

**当前状态**：TransformID / MaterialInstanceID 通过 VAB 传递（顶点流 R16_UINT）。

**目标迁移路径**：未来可以切换到 `gl_InstanceIndex` 直接索引 SSBO，不再需要 VAB。

**方案**：引入 `VertexDataPolicy` 枚举，标记每类数据的传递方式：

```cpp
enum class VertexDataPolicy : uint8 {
    VAB,            // 通过顶点缓冲区传递（当前默认）
    InstanceSSBO,   // 通过 SSBO + gl_InstanceIndex 传递（目标状态）
    DrawID_SSBO,    // 通过 gl_DrawID + SSBO 传递（多 draw call 批量）
};

// MaterialCreateConfig 中新增
struct VertexChannelPolicy {
    VertexDataPolicy transform_id   = VertexDataPolicy::VAB;
    VertexDataPolicy material_inst  = VertexDataPolicy::VAB;
    VertexDataPolicy vertex_pos     = VertexDataPolicy::VAB;  // 未来扩展
};
```

`VertexInputConfig::CreateVIL()` 根据 `VertexChannelPolicy` 决定是否加入 `TransformID`/`MaterialInstanceID` 的 VIA。

`MaterialCreateInfo::SetLocalToWorld()` 根据 policy 决定是否还需要添加 `TransformID` VIA + 对应 VAB。

这样，`HGL_L2W_USE_SSBO` 和 `HGL_MI_USE_SSBO` 这两个宏的切换，最终可以替换为 `VertexChannelPolicy` 的运行时配置，而无需重编译。

---

### 阶段 6：Shader 代码片段外部化（可选，长期目标）

**当前**：所有 GLSL 片段都是 `constexpr const char *`，修改需重编译 C++。

**建议**：将 `MFGetPosition.h`、`MFCommon.h` 等中的 GLSL 片段迁移到 `.glsl` 文件，以 `incbin` 或 CMake `file(READ)` 嵌入，或通过资产系统加载：

```
src/ShaderGen/glsl/
  GetPosition2D_NDC.glsl
  GetPosition2D_Ortho.glsl
  GetPosition3DL2WCamera.glsl
  GetNormal.glsl
  …
```

`ShaderCreateInfo` 将片段注册为 key-string，shader 生成阶段通过 key 查找文本，而非直接 `#include` C++ 头文件。

---

## 四、迁移 TransformID/MI_ID 到纯 SSBO 的完整步骤（可独立执行）

以下是把 TransformID 从 VAB 完全迁移到 `gl_InstanceIndex + SSBO` 的完整步骤（MI_ID 类似）：

1. **Shader 侧**：移除 `layout(location=?) in uint TransformID;`，改用 `gl_InstanceIndex` 作为 SSBO 索引。
   - `MFCommon.h` 中的 `MF_GetLocalToWorld_ByAssign` 改为 `mat4 GetLocalToWorld(){return l2w.mats[gl_InstanceIndex];}`

2. **VIA 侧**：`VertexInputGroup::TransformID` 对应的 VIA 不再添加到 `ShaderCreateInfoVertex`（`ShaderCreateInfoVertex::AddAssignTransform()` 改为空操作或条件操作）。

3. **VIL 侧**：`VertexInputConfig::CreateVIL()` 中 `group==TransformID` 的特殊处理分支删除。

4. **Runtime 侧**：`TransformAssignmentBuffer` 不再创建 `transform_vab`，`GeometryDataBuffer` 不再需要 TransformID VAB 槽位。`VABList::Add(mdb)` 也不再需要为 assign 流留槽。

5. **Draw call 侧**：`vkCmdDraw(…, instanceCount=N, firstInstance=base_index)` 通过 `base_index` + `gl_InstanceIndex` 直接定位 SSBO 中的矩阵，不需要 VAB ID 流。

6. **清理**：`Assign::TransformID` 命名空间、`VertexInputGroup::TransformID`、`VKRenderAssign.h` 中对应的常量可以标记 deprecated 并最终删除。

---

## 五、各组件职责总结（重构后）

| 组件 | 职责（重构后） |
|------|--------------|
| `DescriptorSetType` | 保持不变；删除未用的 `World/Static/Global`（或合并） |
| `DescriptorBinding` | 自动绑定：UBO + SSBO + Texture 全部支持 |
| `MaterialParameters` | 分 persistent/dynamic 两类 WDS，Update 不清空 persistent |
| `Material` | 持有 binding 号缓存（BoundDescriptorTable），绑定时 O(1) |
| `VertexInputConfig` | 支持 `VertexChannelPolicy`，TransformID/MI_ID 槽位按策略加减 |
| `MaterialCreateInfo` | 消除 `#ifdef` 二态布局，使用运行时 `BufferBackend` 枚举 |
| `RenderCmdBuffer` | `BindDescriptorSets` 按真实 set 号逐个提交，修复 set 错位 |
| GLSL 片段 | 长期目标：外部化为 `.glsl` 文件，消除修改需重编译 C++ |
