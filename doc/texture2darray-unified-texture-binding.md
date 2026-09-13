# Texture2DArray 统一纹理绑定机制与保留决策

> 结论先行：**保留 `Texture2DArray`，不消灭。** 它是"同规格纹理集"在引擎里的正确表达方式——一个描述符 + 层号 = N 张纹理；删除它会把描述符占用量变成层数的线性函数。本文记录该机制的完整实现（含 path:line 证据与实测数值）、保留理由、已落地的收口改动，以及因此修订的后续计划。
>
> 文档状态：2026-09-13 定稿（决策 + O1 落地）。基线分支 `RemoveTexture2DArray`，原"消灭"计划已按本文第 6 节修订，分支名保留（未回退）。

---

## 1. 一句话机制

**普通 `Texture2D` 在注册进 bindless 描述符数组时，被包装成"单层 `2D_ARRAY` view"（companion view），层号写 0**；真正的 `Texture2DArray` 则直接用自己的 `2D_ARRAY` 主 view。两者写进同一个 `texture2DArray[]`，GLSL 侧完全同形，差异只剩材质数据行里的层号字段 `.y`。

于是引擎里"用哪张纹理"变成两件事的组合：**描述符索引（哪张图 / 哪套纹理集）+ 层号（图内第几层）**。

---

## 2. 四个环节（每步只有一条路径）

### ① 注册口：同一个 `texture2DArray[]`

| 事实 | 位置 |
|---|---|
| 描述符池：`SAMPLED_IMAGE × 8192` + `SAMPLER × 64` | `src/Vulkan/VKBindlessTextureManager.cpp:17` |
| layout：binding=0 = `SAMPLED_IMAGE`（`PARTIALLY_BOUND｜UPDATE_AFTER_BIND`）、binding=1 = `SAMPLER`（仅 `PARTIALLY_BOUND`） | 同上 `:37-56` |
| 写入用 `tex->GetBindlessArrayView()`，`dstArrayElement = handle - 1`（句柄 1-based，0=无效） | 同上 `:127-162` |
| `Texture` 默认实现 = 主 view | `inc/hgl/vk/VKTexture.h:53` |
| `Texture2D` 覆写：**惰性创建 `ext.depth = 1` 的 `VK_IMAGE_VIEW_TYPE_2D_ARRAY`**，缓存在 `data->array_view`；主 view 保持 `TYPE_2D`（不影响 RTV/DSV/拷贝） | `src/Vulkan/VKTexture.cpp:37-56` |
| `Texture2DArray` **不覆写** → 直接用 2D_ARRAY 主 view | `inc/hgl/vk/VKTexture.h:96-108` |
| layerCount 来源：`subresourceRange.layerCount = ext.depth` | `src/Vulkan/VKImageView.cpp:28-34` |

### ② 数据口：`uvec2{descriptor_index, array_layer}`

- 行结构 `MaterialTextureReference`：8 字节，`static_assert(sizeof == 8)`（`inc/hgl/mtl/MaterialRecipe.h:181-187`）。
- 行布局：`BuildMaterialTextureReferenceLayout` 按声明顺序排布，`row_stride = 16B 对齐`（同上 `:410-456`）。
- 普通 Texture2D 的层号被**两处校验**强制为 0；`Texture2DArray` 允许 0..N-1：
  - 作者侧：`src/ecs/components/PrimitiveComponent.cpp:416-429`（`kind` 与声明采样器类型交叉校验 + 非数组不得带层号）
  - 收集侧：`src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp:183-193`（binding 校验）、`:896-906`（物化校验）
- 行内容在物化阶段写入：`handle = GetBindlessHandle(resource_id)` → `references[i] = {handle, handle==0 ? 0u : layer}`（同上 `:908-947`）。
- 句柄分配按**纹理对象**去重、不按类型（`tex_cache_`，`src/Vulkan/VKBindlessTextureManager.cpp:132-143`）；资源 id = `"texid:" + TextureID`（`src/ecs/support/RenderResource.cpp:6-12`）。

### ③ 采样口：只有 `texture2DArray` 一种采样形式

```glsl
layout(set=BINDLESS_SET, binding=0) uniform texture2DArray bindless_tex[];   // ShaderLibrary/common/bindless_textures.glsl:34
layout(set=BINDLESS_SET, binding=1) uniform sampler      bindless_samp[];   // :35

vec4 SampleOptional(uvec2 tex_ref, uint samp_idx, vec2 uv, vec4 fallback)    // :61-66
{
    return tex_ref.x == 0u
        ? fallback
        : Sample2DArray(tex_ref.x, samp_idx, uv, float(tex_ref.y));
}
```

`Sample2DArray`（`:48-55`）最终展开为 `texture(sampler2DArray(bindless_tex[nonuniformEXT(h-1u)], bindless_samp[nonuniformEXT(s)]), vec3(uv, layer))`。**没有任何按纹理类型分叉的代码。** 集合号 `BINDLESS_SET = 1`（`ShaderLibrary/common/descriptor_macros.glsl:47-49`，数值真源 `inc/hgl/common/DescriptorSetTypeDef.h:34-45`）。

### ④ 程序口：程序 key 里没有纹理身份/种类

```cpp
struct ShaderProgramKey { mesh_stage_digest; fragment_stage_digest; resource_layout_hash;
                          vertex_input_hash;  render_target_hash;  compiler_hash; };  // inc/hgl/mtl/ShaderProgramKey.h:13-21
```

→ 同一材质定义下，实体绑什么纹理、绑哪种纹理，**都不会产生新程序**；`resource_layout_hash` 描述的是材质定义的声明布局，不是某个纹理对象。

---

## 3. 链路全景（作者侧 → 行 → 描述符 → GLSL → GPU）

| 阶段 | 做什么 | 关键位置 |
|---|---|---|
| 作者侧 | `CreateTexture2DArray(name,w,h,layer,fmt,mips)` 建空 array，`LoadTexture2DArray(arr, layer, file)` 逐层从**单张 2D 文件**拷入 | `inc/hgl/graph/module/TextureManager.h:130-132`、`src/SceneGraph/texture/VKTexture2DArrayLoader.cpp:19-42` |
| 作者侧 | `SetMaterialTextureResource(name, tex, sampler, kind, resource_id, array_layer)`（层号落脚点） | `src/ecs/components/PrimitiveComponent.cpp:368-446` |
| 每帧 collect | 注册纹理句柄 → 组 `references[]` → 取行池行 → 写 `uvec2` 行 | `src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp:860-1089` |
| 每帧 batch | 写每实例地址表 `MaterialInstanceAddresses{payload_address, texture_reference_address}` | `src/ecs/support/PrimitiveBatchPipeline.cpp:828-861` |
| 描述符集 | 一帧绑一次 Scene(0)/Bindless(1) 两集，per-material 绑定零残留 | `inc/hgl/common/DescriptorSetTypeDef.h:34-45` |
| GLSL | `MTL_TEX(i)` → `MaterialTextureReferencesRef(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[i].texture_reference_address)` | 发射点 `src/ShaderGen/compile/MaterialShaderEmitter.cpp:205-219` |
| GPU | `Sample2DArray(handle, sampler, uv, layer)`，`nonuniformEXT` 索引 | `ShaderLibrary/common/bindless_textures.glsl:46-66` |

---

## 4. 实测对照（2026-09-13 实跑日志）

| 示例（材质定义） | 绑定纹理类型 | bindless 句柄 | 行数据（节选） | 程序 id |
|---|---|---|---|---|
| `example/Basic/PBRSpheres.cpp`（Lit） | 2 × `Texture2DArray`（各 **10 层**） | 1, 2 | base_color `desc=1 layer=0..9`；normal `desc=2 layer=0..9` | `program-4558440414288230968` |
| `example/Basic/SingleSphereMaterialSwitch.cpp` 近模式（Lit） | 2 × `Texture2DArray`（各 **1 层**） | 1, 2 | base_color `desc=1 layer=0` | `program-4558440414288230968`（同一个） |
| 同上 **远模式**（同一 Lit 定义，同一槽名） | 2 × **普通 `Texture2D`** | 未实测（见下注） | 未实测 | 预计同上（程序 key 不含纹理身份） |
| `example/Basic/SimpleSphere.cpp`（Lit，全程普通 2D） | 3 × `Texture2D` | 1, 2, 3 | base_color `desc=1 layer=0`、roughness `desc=2 layer=0`、normal `desc=3 layer=0` | `program-2855613769289964008`（与 array 示例不同；两者顶点格式实测不同，程序 key 含 `vertex_input_hash` → 推断由此导致） |
| `example/Texture/TextureRectArray.cpp`（UnlitTexture） | 1 × `Texture2DArray`（**4 层**） | 1 | base_color `desc=1 layer=0..3` | 另一份（另一材质定义） |

自洽校验（真实日志）：

```
[BindlessTextureManager] Initialized (max_tex=8192 max_sampler=64)
[BindlessTextureManager] Registered 7 samplers
[MaterialTextureReferencePool] created definition=Lit references=6 row_stride=48 capacity=1024 bytes=49200
[MaterialTextureReferences] owner=Sphere_M0_R0 definition=Lit row=1 references=6 gpu=0x305f80030
[MaterialTextureReferences] texture=base_color descriptor=1 layer=0
[MaterialTextureReferences] texture=roughness descriptor=0 layer=0      ← 未绑定槽 = 句柄 0
[MaterialTextureReferences] texture=normal    descriptor=2 layer=0
```

- 行地址步进 `0x305f80030 → 0x305f80060` = **0x30 = 48B**，与 `references=6 × uvec2` 对齐 16B 一致。
- TextureRectArray 行步进 `0x304400010 → 0x304400020` = **0x10 = 16B**（1 × uvec2 = 8B 向上对齐 16B）。
- SimpleSphere 的程序 id 与 array 示例不同**与纹理类型无关**：两者顶点格式实测不同（`VF_V2HF/VF_V2UN8` vs `VF_V2F/VF_V3F`，`example/Basic/SimpleSphere.cpp:46-54` vs `SingleSphereMaterialSwitch.cpp` 的 gvf），而 `ShaderProgramKey` 含 `vertex_input_hash` → 推断差异来自顶点输入；纹理种类不在 key 中（§2④ 直接可证）。

> **远模式未实测的说明**：远/近切换由相机距离驱动（`Tick:421-433`）。本次尝试用后台按键注入（PostMessage PageDown）触发，驱动返回 `delivery_failed / effect=unverifiable`，日志中 bindless 句柄数仍为 2 → **未能切到远模式**，故上表该行不填观测值。代码层判据：两个 recipe 只差 `recipe_name` 与材质数据行（`:177-187`），且 `ShaderProgramKey` 不含纹理身份（`inc/hgl/mtl/ShaderProgramKey.h:13-21`），故程序必然复用；句柄按纹理对象首次注册分配（`src/Vulkan/VKBindlessTextureManager.cpp:132-143`），远模式纹理注册后应取 3、4。自行滚轮 / PageDown 拉远一次即可在日志中复核。

`example/Basic/SingleSphereMaterialSwitch.cpp` 是"同一 Lit 下两种都绑"的专用验证例：远模式 `LoadTexture2D` 两张普通 2D（`:125-126`）+ 缺省 kind 绑定（`:238-244`）；近模式建 **1 层** array 后逐层装入并以 `kind = Texture2DArray` 绑定（`:130-148`、`:250-261`）；两个 recipe 只差 `recipe_name` 与材质数据行（`:177-187`）；切换由相机距离驱动（`Tick` `:421-433`，`PageUp/PageDown`、`Equals/Minus`、滚轮绑定见 `src/ecs/systems/tick/CameraInputMapping.cpp:25-29`）。

---

## 5. 为什么保留（收益与代价，如实分列）

### 5.1 收益

1. **描述符预算**：PBRSpheres 的 20 套纹理（10 列 × baseColor/normal）只占 **2 个** `SAMPLED_IMAGE` 槽位；改成"每张纹理一个描述符"要占 20 个。上限 8192（`VKBindlessTextureManager.cpp:17`），数组方案把"层数"从描述符预算里移出去。
2. **一个 `VkImage` 承载整套同规格纹理**：一次分配 / 一次内存绑定 / 逐层拷贝入同一 image，层间共享格式、mip 配置与 sampler 预设，不存在 N 张纹理各自配置错配的可能。
3. **材质行成本与张数解耦**：行仍是固定元数的 `uvec2[]`（Lit = 6 × 8B = 48B/行），写行/哈希/比较的代价不随"层数"增长；换成 per-texture 描述符则行内容要随张数膨胀、行池容量与缓存压力同步上升。
4. **采样一致性**：层与层之间的采样语义完全同构（同一 `sampler2DArray` + 不同 `.z`），LOD 与各向异性行为一致。

### 5.2 代价（保留即接受，需在文档层面固化）

1. **同规格强约束**：一套 array 内的所有纹理必须 **w/h/format/层数完全一致**——这正是"用得上它性能优势"的前提（`example/Texture/TextureRectArray.cpp:96-100` 写死 512×512 + `PF_BC7UN`；`PBRSpheres.cpp:199-210` 用第一张图探针决定尺寸/格式）。规格不齐的场景只能退化为多张独立纹理。
2. **层号是约定而不是类型**：`.y = 0`（普通 2D）由两处校验保证（见 §2②），漏一处就是**越界采样**（`vec3(uv, layer)` 打到不存在的层）而不是编译错。
3. **1 层 array 无性能收益**：`SingleSphereMaterialSwitch` 的近模式就是纯统一 trick，它的价值只在"验证两种绑定同形"。
4. **当前 mipmap 链实际是死的**：保留数组路径后这是必须修的缺陷，见 §6.1。

### 5.3 对照：per-fragment 成本不变（纠正一个常见误会）

同一 draw 内 `dataIndex` 是 **per-draw 常量**（来自 `gl_DrawID` / `gl_InstanceIndex`，经 varying 传给 fragment；`src/ShaderGen/template/FragmentTemplateComposer.cpp:201,616` 传 `materialDataIndex`），所以：

- 可选槽的 `if (句柄 == 0)` 是 **uniform 分支**，不产生 divergence；
- 它反而**省 fetch**：PBRSpheres 只绑 2/6 槽，其余 4 槽靠分支直接跳过；
- 数组方案与多描述符方案的 **texture fetch 次数完全相同**；差别只在描述符预算与 image 组织方式。

---

## 6. 保留决策带来的后续项（修订原"消灭 Texture2DArray"计划）

### 6.1 数组纹理的 mipmap 死链（**2026-09-13 已修复**）

| 事实 | 位置 |
|---|---|
| `CreateTexture2DArray(w,h,layer,fmt,mipmaps)` 的 `mipmaps` 形参**从未被使用** | `src/Vulkan/Texture/VKDeviceTexture2DArray.cpp:106-121` |
| `TextureCreateInfo(fmt)` 走 `mem_zero` 默认 → `origin_mipmaps = target_mipmaps = 0` | `inc/hgl/vk/VKTextureCreateInfo.h:14-15,34-37` |
| 创建时 `target_mipmaps = (origin_mipmaps > 1 ? origin_mipmaps : 1) = 1` | `VKDeviceTexture2DArray.cpp:40-41` |
| `TextureData::miplevel = tci->target_mipmaps` → `GetMipLevel() == 1` | `inc/hgl/vk/VKTextureCreateInfo.h:359` |
| `GenerateTexture2DArrayMipmaps` 首行 `GetMipLevel() <= 1 → return true`（恒早退） | `VKDeviceTexture2DArray.cpp:261-264` |
| `LoadTexture2DArray` 内部把 `auto_mipmaps` 硬编码为 `false` | `src/SceneGraph/module/TextureManager.cpp:195` |

上表为**修前**状态（存档用）。成因不止"形参没接线"：源资产的 mip 链本来就在文件里（实测
`res/image/pbr/*/baseColor.Tex2D` = BC7/1024²/**9 级**、`res/image/icon/freepik/*.Tex2D` = BC7/512²/**8 级**），
是逐层装载只拷了 0 级把链丢掉了。

**采用的方案（A：按源资产整链逐层拷入，不做压缩格式的自动生成）**

| 改动 | 位置 | 要点 |
|---|---|---|
| 共享块压缩 helper | `inc/hgl/vk/VKFormat.h`（`IsBlockCompressedFormat` / `GetBlockCompressedLevelBytes`） | 原实现是 `VKDeviceTexture2D.cpp` 里的匿名 namespace 副本，现上收为唯一真源 |
| API 语义修正 | `inc/hgl/graph/module/TextureManager.h:97,132` | `bool auto_mipmaps` → **`uint32 mip_levels = 1`**（零兼容：不保留无效 bool 形参）；级数真源 = 源资产（示例用探针的 `GetMipLevel()`） |
| 级数真正落进 tci + 多级图像可 blit | `src/Vulkan/Texture/VKDeviceTexture2DArray.cpp` | `origin_mipmaps = target_mipmaps = mipi_levels`；`mips > 1` 时补 `VK_IMAGE_USAGE_TRANSFER_SRC_BIT` |
| **整链逐层拷入**（新函数） | `TextureManager::ChangeTexture2DArrayMipmaps` 同上文件 | 按级构建 `VkBufferImageCopy`：`mipLevel=i`、`baseArrayLayer=layer`、`layerCount=1`、偏移按 BC 级别字节/非 BC `rolling_level_bytes` 推进 —— 与 2D 路径 `CommitTexture2DMipmaps` 同构 |
| 逐层装载分派 + 校验 | `src/SceneGraph/texture/VKTexture2DArrayLoader.cpp` | 尺寸/格式必须与数组一致（否则**静默错位**）→ fail-fast；级数策略见下 |
| 压缩格式拒绝生成 | 同上 + `GenerateTexture2DArrayMipmaps` | 块压缩格式无链 → **显式报错**，不静默降级；顺带修掉该函数入口屏障的 `oldLayout`（原写 `SHADER_READ_ONLY`，与本路径实际的 `TRANSFER_DST` 不符） |
| 删无效形参链 | `src/SceneGraph/module/TextureManager.cpp:196-202` | `LoadTexture2DLayerFromFile(..., false)` 的硬编码 `false` 一并删除 |

级数策略（数组创建时给定，逐层装载时校验）：

| 数组级数 | 源链 | 行为 |
|---|---|---|
| 1 | 任意 | 只拷 0 级（源链被截断，与旧版一致） |
| > 1 | ≥ 数组级数 | 整链拷入（块压缩格式的唯一合法路径） |
| > 1 | = 1（无链）且**非**块压缩 | 单级拷入 + 逐级 blit 生成 |
| > 1 | = 1（无链）且**块压缩** | **显式失败**：不支持自动生成，须由资产提供链 |
| > 1 | 1 < 源链 < 数组级数 | **显式失败**（不混合"拷贝 + 生成"两种来源） |

**实测验证（2026-09-13）**

| 项 | 结果 |
|---|---|
| 构建 | 0 个 C++ 错误（`error C*/LNK` = 0；仅 CMCore 示例的 vcpkg 后置步骤 MSB3075，既有） |
| PBRSpheres | `[Texture2DArray] create name=pbr_baseColor_array 1024x1024 layers=10 fmt=BC7UN **mip_levels=9**`（normal 同）；`[ERROR]`=0、`VUID`=0、675 帧 |
| SingleSphereMaterialSwitch | 两张 1 层数组 `mip_levels=9`；0 ERROR / 0 VUID / 697 帧 |
| TextureRectArray | `mip_levels=1`（行为与修前一致，改动不外溢）；0 ERROR / 0 VUID / 704 帧 |
| 字节布局自证 | 源文件 payload == 引擎逐级公式之和（`baseColor.Tex2D`：`1398096 == 1398096` MATCH；各级偏移 `0, 1048576, 1310720, …, 1398080`）；`001-online resume.Tex2D`：`349520 == 349520` MATCH |
| 门禁 | `ShaderResourceSchemaRegressionGate` = **38 PASS / 1 FAIL**（`V1.material-output-contract` 为既有基线，与本改动无关） |
| 视觉 | 前排贴图细节清晰、远排表面平滑无高频闪烁（截图存 `computer_use_5160b4017aeb4072930a7e1c6fb3c312.png`） |
| 内存代价 | PBRSpheres 两套数组 20 MB → ≈26.7 MB（BC7 1024² 整链 = 1.333 倍） |

### 6.2 修订：行数组化保留 `uvec2`，取消 ABI 塌缩

原计划（O2）含"`MaterialTextureReference` 8B → 4B"的 ABI 塌缩，**该子项取消**——层号必须留在行里。修订后仍可做的部分：

- 行字段数组化：`uvec2 tex_refs[N]`（槽序 = `material.toml` 声明序），配套 `#define SLOT_<name> <i>u`；
- helper 由 ShaderGen 与 `MTL_TEX` **一起发射**（不要放进 `bindless_textures.glsl`：无纹理引用的材质也 include 该模块，会撞"宏未定义"）；
- `required` 槽免守卫：模块元数据已带策略（`inc/hgl/mtl/ShaderCodeModule.h:203-209`），收集阶段已保证 required 槽句柄非 0（`RenderPrimitiveCollectSystem.cpp:932-942`）→ 该分支是编译期死代码；
- 收益：加一个纹理槽 = toml 一行 + GLSL 一行，且 GLSL 名字与 toml 名字不可能漂移。

### 6.3 仍可独立清理的死码（与本决策无关，纯零调用点）

| 项 | 位置 | 判据 |
|---|---|---|
| `Texture2DArrayLoader` / `Texture1DArrayLoader` / `TextureCubeArrayLoader` | `inc/hgl/graph/texture/TextureLoader.h:220-291` | 全仓零实例化，唯一"使用"在注释里（`src/SceneGraph/texture/VKTexture2DArrayLoader.cpp:7,14`） |
| `Sample2D()` | `ShaderLibrary/common/bindless_textures.glsl:39-46` | 全 ShaderLibrary 零调用（11 处采样全走 `Sample2DArray`/`SampleOptional`） |
| `SetMaterialTextureArrayLayer()` | `inc/hgl/ecs/components/PrimitiveComponent.h:197` | 零调用点（声明 + 实现各一处） |
| `VKSamplerType.h` / `VKTextureType.h` | `inc/hgl/vk/` | 零消费者，且 `SamplerImageViewType[]` 15 项对 19 个枚举项本身错位 |
| `TextureSamplerTypeDef.h` | `inc/hgl/common/` | 零消费者；且 `TextureTypeName[]` 因某一项后**缺逗号**被字面量拼接（实际 11 项对 12 个枚举项）→ `GetTextureTypeName(Texture2DArray)` 返回 `"textureCubeArray"`、`ParseTextureType("texture2DArray")` 返回 `Texture1DArray`、末项越界读 |

> 名字表类代码要么整体删掉，要么补 `RANGE_SIZE` 覆盖性断言（既定偏好：断言必须随表自动 scale，禁"加一个枚举补一条断言"）。

### 6.4 可选增强（不属于本次决策）

- `Texture2DCollection`（N 张同规格纹理但不希望占层号的场景；同时是"每张纹理独立 `TextureID` + 名字、RenderDoc 直读"的路径）；
- per-declaration sampler 预设（`material.toml` 的 `[resources].textures` 已解析 `filter/wrap/swizzle/anisotropy/...`，见 `src/ShaderGen/material_definition/MaterialDefinitionFile.cpp:911-917`，但尚未接线到运行期）。

---

## 7. 本次随文档一并落地的改动（O1：可选槽统一取样）

### 7.1 改动

| 文件 | 内容 |
|---|---|
| `ShaderLibrary/common/bindless_textures.glsl` | 新增 `SampleOptional(uvec2, uint, vec2, vec4)`（`:57-66`），把"句柄 0 判空 + 采样"收口到一处；用法注释同步改写 |
| `ShaderLibrary/material/pbr_surface_source.glsl` | 4 个 optional 槽 + `EvalMaterialAlpha` 从逐槽 `if` 块塌成**每槽一条语句**（103 → 65 行）；顺带删除 `EvalMaterialAlpha` 中未被使用的 `material_data` 局部 |

净变化 **2 files, +33 / −58**。零 ShaderGen 改动、零 ABI 改动。

```glsl
material_output.baseColor *=
    SampleOptional(MTL_TEX(material_data_index).tex_base_color, TrilinearSampler, material_uv, vec4(1.0)).rgb;
material_output.roughness =
    clamp(material_output.roughness * SampleOptional(MTL_TEX(material_data_index).tex_roughness, LinearSampler, material_uv, vec4(1.0)).r, 0.04, 1.0);
material_output.metallic =
    clamp(material_output.metallic * SampleOptional(MTL_TEX(material_data_index).tex_metallic, LinearSampler, material_uv, vec4(1.0)).r, 0.0, 1.0);
material_output.ao =
    SampleOptional(MTL_TEX(material_data_index).tex_occlusion, LinearSampler, material_uv, vec4(1.0)).r;
```

### 7.2 语义等价论证（逐槽代数）

| 槽 | 旧行为 | 新行为（fallback `vec4(1.0)`） | 是否等价 |
|---|---|---|---|
| base_color | 有句柄：`*=.rgb` | `*= 1.0.rgb` 或 `*=.rgb` | 等价 |
| roughness | 有句柄：`clamp(r * s, 0.04, 1.0)` | `clamp(r * 1.0, 0.04, 1.0)`；`r` 已在 `:31` 被 `clamp(…, 0.04, 1.0)` → clamp 幂等 | 等价 |
| metallic | 同构 | 同构（`r` 已在 `:30` clamp 到 [0,1]） | 等价 |
| occlusion | 有句柄才**替换** `ao` | fallback `1.0` = `ao` 初值（`:34`） | 等价 |
| opacity_mask | 无句柄 → `1.0` | fallback `.r = 1.0` | 等价 |

### 7.3 实际可改面（比"按调用点计数"窄，判据如下）

| 分类 | 站点 | 处置 |
|---|---|---|
| 守卫 + 采样 | `pbr_surface_source.glsl` 4 槽 + `EvalMaterialAlpha` | 收口为 `SampleOptional` |
| required 槽、本就无守卫 | `texture_source.glsl:21`、`text_source_gpu.glsl:91/125/162` | **不改**（toml `required = true`，套 helper 只是白加判断） |
| 守卫包着一整块 TBN 数学 | `ntb_derivative_normalmap.glsl:40`、`ntb_tangent_vbo_normalmap.glsl:29` | **保留 `if` + `Sample2DArray`**（已知绑定且需显式层号） |

### 7.4 验证证据（全部实跑）

| 项 | 结果 |
|---|---|
| 全量 cook | `ShaderCooker --store <dir>` → **cooked=28 / failed=0 / skipped=10**，17 个 `.frag` |
| 生成物残留 | 所有 `.frag` 中 `_texture.x != 0u` / `opacity_texture.x == 0u` **0 命中**；Lit forward 的 `EvalMaterialSource` 即上面那 8 行 |
| 示例冒烟（`ULRE_ARENA_DEBUG=1`，12s） | PBRSpheres 668 帧 / TextureRectArray 695 / SingleSphereMaterialSwitch 693，**`[ERROR]`=0、`VUID`=0**；`descriptor/layer` 与改前逐字一致 |
| 门禁 | `ShaderResourceSchemaRegressionGate` = **38 PASS / 1 FAIL**；`ShaderDocumentRegression` exit 0；`ShaderDocumentProductionRegression` → "smoke production fixtures passed" |
| 既有 FAIL | `V1.material-output-contract` → `depth coverage resource pruning mismatch` 为**基线状态**（路径限定 `git stash push -- <两文件>` 复跑同一条 FAIL），与本改动无关 |
| 视觉自证 | `computer_use list_windows` → `capture mode=vision`；PBRSpheres 可见贴图图案与金属度渐变（非全灰） |

建议提交信息：

```
统一可选纹理槽取样：新增 SampleOptional，pbr 源模块五槽各收为一行

- bindless_textures.glsl: 新增 SampleOptional(uvec2, uint, vec2, vec4)，把「句柄 0 判空 + 采样」
  收口到一处；零 ShaderGen 改动、零 ABI 改动
- pbr_surface_source.glsl: 4 个 optional 槽 + EvalMaterialAlpha 塌成每槽一条语句（103 → 65 行），
  fallback 统一 vec4(1.0)，与逐槽 if 语义逐位等价（乘 1.0 / ao 初值 1.0 / alpha 缺省 1.0）
- 顺带删除 EvalMaterialAlpha 中未被使用的 material_data 局部
- required 槽（texture_source / text_source_gpu）与 NTB 的整块守卫保持原样
```

---

## 8. 改这条链时的雷区

1. **两处重复校验**：作者侧（`PrimitiveComponent`）+ 收集侧（`RenderPrimitiveCollectSystem`）都要动，只改一处会留下一条拒绝路径。
2. **`.y = 0` 是约定不是类型**：非数组槽带上非零层号 = 越界采样（不是编译错）；新增纹理槽前先确认 toml 声明的采样器类型。
3. **required 语义**：`required = true` 的槽缺绑定 = 硬失败（收集阶段 `:932-942`）；改成 `false` 后 GLSL 的 fallback 会**静默兜底**，画面错误不会报错。
4. **companion view 生命周期**：`data->array_view` 在 `~Texture` 里释放（`src/Vulkan/VKTexture.cpp:22-23`），主 view 必须保持 `TYPE_2D`（RTV/DSV/拷贝路径依赖）。
5. **别以"性能"为由去掉可选槽的 `if`**：它是 per-draw uniform 分支且省 fetch（§5.3）；恒采样方案只在分支极敏感或全槽必绑时才划算。
6. **GLSL 文本进内容哈希**：改动 `ShaderLibrary/**` 会全量失效着色器缓存 → 用 `ShaderCooker --store <dir>` 重烤；`build/cache-hot` 下只有 `.spv`，不要当生成物读。
7. **结构大小头变更要删 obj / clean-first 重编所有 include 者**（本仓 MSBuild 增量对头文件依赖跟踪不可靠）。
8. **行尾/编码**：`ShaderLibrary/*.glsl` 与 `doc/*.md` 都是**混行尾**（例：`bindless_textures.glsl` = CRLF，`pbr_surface_source.glsl` = LF-only，`texture_source.glsl` = CRLF），改动前逐文件探测；`git stash push/--pop` 会把 LF 工作区文件重新物化成 CRLF，需归一回原位。

---

## 9. 验证命令速查

```bash
cd E:/ULRE

# 1) 全量重烤 + 生成物检查
STORE="$LOCALAPPDATA/Temp/ulre_cook" && rm -rf "$STORE"
./build/out/Windows_64_Debug/ShaderCooker.exe --store "$STORE"
grep -c "SampleOptional(" "$STORE"/shader-cache/stage/*.frag          # Lit forward 应为 7
grep -n "_texture.x != 0u\|opacity_texture.x == 0u" "$STORE"/shader-cache/stage/*.frag   # 应为空

# 2) 示例冒烟（cwd 必须是仓库根，res/ 在根）
ULRE_ARENA_DEBUG=1 timeout 12 ./build/out/Windows_64_Debug/PBRSpheres.exe > "$LOCALAPPDATA/Temp/pbr.log" 2>&1
grep -c "\[ERROR\]\|VUID" "$LOCALAPPDATA/Temp/pbr.log"                # 应为 0
grep "MaterialTextureReferences] texture=" "$LOCALAPPDATA/Temp/pbr.log" | head

# 3) 门禁三件套
./build/out/Windows_64_Debug/ShaderResourceSchemaRegressionGate.exe    # 基线：38 PASS / 1 FAIL（V1）
./build/out/Windows_64_Debug/ShaderDocumentRegression.exe
./build/out/Windows_64_Debug/ShaderDocumentProductionRegression.exe

# 4) 同一 Lit 下两种纹理共存的对照例（远/近模式切换：滚轮 / PageUp·PageDown）
ULRE_ARENA_DEBUG=1 ./build/out/Windows_64_Debug/SingleSphereMaterialSwitch.exe > "$LOCALAPPDATA/Temp/sw.log" 2>&1
grep "Register texture handle" "$LOCALAPPDATA/Temp/sw.log"             # 近=1,2（array）；远=再多 2 个（普通 2D）
```

---

## 附录：关键判据一句话索引

| 问题 | 判据 |
|---|---|
| 普通 2D 为什么能进 `texture2DArray[]`？ | `Texture2D::GetBindlessArrayView()` 建单层 2D_ARRAY companion view（`src/Vulkan/VKTexture.cpp:37-56`） |
| 层号从哪来、谁保证 2D 恒 0？ | `SetMaterialTextureResource(..., array_layer)` + 两处交叉校验（`PrimitiveComponent.cpp:416-429`、`RenderPrimitiveCollectSystem.cpp:183-193`） |
| 同一 Lit 会不会因纹理类型不同而多编译一份程序？ | 不会，`ShaderProgramKey` 不含纹理身份（`inc/hgl/mtl/ShaderProgramKey.h:13-21`） |
| 数组纹理有没有 mipmap？ | 没有（`auto_mipmaps` 形参未使用 + `GenerateTexture2DArrayMipmaps` 恒早退）——见 §6.1 |
| 可选槽的 `if` 影响性能吗？ | 不影响：per-draw uniform 分支且省 fetch（§5.3） |
