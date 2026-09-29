# EnvironmentSystem / 环境综合信息管理

> **订正(2026-09-28)**:相机 / 环境 / 地址表断言已按权威文档 `doc/world-addresses-and-camera-model-plan.md`(§1、§3、§4、§7)就地修订,本文改动项:
> 1. **Scene 集整体退场**:`enum class SceneBinding`、`kSceneBinding*`、`VKGlobalSceneUBOSet`(源文件已删)都不存在,描述符集只剩 **Bindless(0)**;sky / shadow / viewport / camera **不再有 binding**,一律走 BDA 地址;
> 2. sky / shadow 的**地址发布改按世界**(世界表 `WorldAddresses`,批次 C2)。现状是 `GlobalAddresses`(全局表、无绑定无集、`HGL_FRAME_SLOT_TOTAL` 帧槽 × 128B)里的 `addr_sky` / `addr_shadow` 字段,多世界同帧会互踩;**viewport 保持全局**;
> 3. camera 不再是"场景 UBO 绑定",而是**相机行**——相机是**世界级数据**(世界私有 **16 槽 × 8 帧槽**,0 号槽恒为本世界默认相机),shader 经 `pc_root.camera_row` 读(`ShaderLibrary/ubo/scene_ubo.glsl:149`);
> 4. `EnvironmentManager` 仍是设备级容器,但 **profile 的所有权 / 生命周期随世界**(世界创建时选/建自己的 profile、销毁时归还,内置 default 可共享;C2);
> 5. §7 的「Scene 集 6 个 UBO 绑定」表、§9 的「Scene Set 加槽位」流程、§9 末的「Scene Set 仍是设备级单例」全部作废,已就地改写为世界表口径。
>
> 版本:2026-09(BDA 终态 / 单集(Bindless)描述符分支;本稿已按当前代码校对)
> 相关文件:`inc/hgl/graph/ubo/EnvironmentInfo.h`、`inc/hgl/graph/ubo/SkyInfo.h`、`inc/hgl/graph/ubo/ShadowInfo.h`、`inc/hgl/graph/ubo/UBOShaderSources.h`、`inc/hgl/graph/module/EnvironmentManager.h`、`src/SceneGraph/module/EnvironmentManager.cpp`、`inc/hgl/ecs/systems/render/EnvironmentSystem.h/.cpp`、`inc/hgl/ecs/systems/render/ViewUBOCommitSystem.h/.cpp`、`inc/hgl/ecs/systems/render/RenderSceneUBOSystem.h/.cpp`(原 RenderDescriptorBindingSystem)、`inc/hgl/vk/VKRenderTarget.h`、`inc/hgl/graph/ubo/GlobalAddresses.h`、`inc/hgl/common/DescriptorSetTypeDef.h`(`inc/hgl/vk/VKGlobalSceneUBOSet.h` 已随 Scene 集退场删除)、`src/SceneGraph/module/GlobalSSBOBufferRegistry.cpp`

## 1. 这套东西解决什么问题

统一管理"世界级环境信息"(目前有天空 `SkyInfo` 与阴影 `ShadowInfo` 两段,见 `inc/hgl/graph/ubo/EnvironmentInfo.h:20-26`;雾/环境光/IBL 仍是预留),并回答三个问题:

1. **数据放哪**——集中在一个管理器(`EnvironmentManager`,设备级容器;profile 生命周期随世界,见下 2),而不是每个 ECS world 各自建 UBO。旧 EnvironmentSystem 模式当年的症状是"多 RT 争抢共享描述符集的 sky binding",但**Scene 集已整体退场**,该形态不复存在;现在要防的是 sky / shadow / env 的**地址写全局字段** ⇒ 多世界同帧只有最后解析的那个 profile 生效(权威文档 §4);
2. **谁用哪份**——RT/WORLD 只持有一个 `EnvProfileID` 引用,不复制数据;未设置即用内置 default(订正 2026-09-28:profile 的所有权与生命周期**随世界**——世界创建时选/建自己的 profile、销毁时归还,内置 default 可共享;C2);
3. **什么时候写 GPU**——sky 在每个 RT/RenderPass 开始时固定全量写入。shadow 不是单份:按交换链 acquired image 分槽,只在交换链帧 acquire 之后写入当前槽。详见 `doc/shadow-ubo-inflight-overwrite.md`。

### 分层总览

```
┌─ 数据层  EnvironmentInfo(纯数据,一个 Profile 的全部环境状态)
│
├─ 管理层  EnvironmentManager(GraphicsContext 模块,设备级容器;profile 生命周期随世界)
│           Profile 注册 / GPU UBO 物化 / 统一写入
│
├─ 选择层  IRenderTarget::GetEnvironmentProfile()(未设置 = default)
│
├─ 编辑层  ecs::EnvironmentSystem(瘦转发,不拥有 GPU 资源;ExecutionPhase::RenderPreBeginFrame)
│
└─ 地址发布层  RenderSceneUBOSystem(每帧 RT→句柄→地址写表;Scene 集已退场;ExecutionPhase::RenderFrameSync)
            ViewUBOCommitSystem(pass 开始固定写入 camera/viewport/sky+shadow)
```

## 2. 数据层:`EnvironmentInfo` 与 `EnvProfileID`

`inc/hgl/graph/ubo/EnvironmentInfo.h`(`EnvProfileID` 见 :10-13,`EnvironmentInfo` 见 :20-26):

```cpp
using EnvProfileID = uint32_t;
constexpr EnvProfileID kEnvProfileInvalid = 0;
constexpr EnvProfileID kEnvProfileDefault = 1;   // 内置默认 Profile 的固定句柄

struct EnvironmentInfo
{
    SkyInfo     sky;
    ShadowInfo  shadow;

    // 预留:FogInfo fog; ...
};
```

- **纯数据、无 GPU 概念**,可直接序列化;CPU 侧唯一权威副本存在 `EnvironmentManager::Profile::cpu`。
- Profile 以 `EnvProfileID` 句柄引用(名字仅用于创建/查找,重复名字返回已有句柄)。
- `kEnvProfileDefault = 1` 是保留句柄,manager 初始化时自动创建。

## 3. 管理层:`EnvironmentManager`

`GraphModule` 体系(`GraphicsContext::GetManager<EnvironmentManager>()` / `GetEnvironmentManager()`),在 `GraphicsContext::Initialize` 中注册(`src/SceneGraph/render/GraphicsContext.cpp:84`),**必须在 BufferManager 之后**(default profile 物化需要它)。

### Profile 结构与 GPU 物化

```cpp
struct Profile
{
    EnvProfileID    id = kEnvProfileInvalid;
    AnsiString      name;
    EnvironmentInfo cpu;                                // CPU 权威数据
    StructView<SkyInfo>    *sky_ubo    = nullptr;       // sky 段 GPU 物化(懒创建,default 例外)
    StructView<ShadowInfo> *shadow_ring[kShadowUboRing] = {}; // 按 acquired image 分槽,不是单份
};
```

- sky 每个 profile 物化**一份** UBO;多个 RT 选同一 profile 时共享这一块。shadow 每个 profile 物化 `kShadowUboRing`(8) 份,按下标 = 交换链 `acquired_image`。不是按 RT 复制。
- UBO 命名 `"SkyUBO:<profile名>"` / `"ShadowUBO:<profile名>:<slot>"`,走 `BufferManager::CreateUBO` + `StructView`,并设 `SetUpdateClass(BufferUpdateClass::Deferred)`。`Deferred` 的「延迟到下一帧」语义**没有实现**,不能靠它避免跨帧覆写。shader source 分别是 `mtl::SBS_SkyInfo` / `mtl::SBS_ShadowInfo`;**Scene 集已整体退场 ⇒ 两段都没有 binding**,现经 `global_addresses.addr_sky` / `addr_shadow` 解引用(`src/SceneGraph/module/EnvironmentManager.cpp:47`、`:155`),本轮改为经**世界表** `world.addr_sky` / `world.addr_shadow`(见 §11.3)。

### 关键 API

| API | 说明 |
|---|---|
| `Create(name, init_info)` | 注册 profile(重名返回已有句柄) |
| `Find(name)` | 名字查句柄,失败 `kEnvProfileInvalid` |
| `Edit(id)` / `Get(id)` | 取 CPU 权威数据指针(可写/只读),无效句柄返回 nullptr |
| `MarkDirty(id)` | sky:`Update(cpu)+Commit()` 立即写入。shadow **不写 GPU**(调用点在 acquire 之前,写了会踩在途主帧) |
| `GetSkyUBO(id)` | 绑定层用,取 sky 段 GPU buffer(懒物化;无效句柄回退 default) |
| `GetShadowUBO(id, frame_index)` | 绑定层用。`frame_index` 必须是本帧 acquired image,与写入槽一致;离屏传 0 |
| `CommitMaterialized(frame, commit_shadow)` | ViewUBOCommitSystem 用。sky 每次都写;shadow 仅 `commit_shadow==true`(当前 RT 是交换链)时写 `shadow_ring[frame]` |

### default Profile 的特殊性

`OnGraphicsContextChanged`(即 GraphicsContext 初始化完成)时自动创建:

- 内容:`p->cpu.sky.SetTime(10, 0, 0)`(上午十点的太阳);
- **立即物化 sky 与 shadow 两段并标脏**(`EnsureDefault()`,`EnvironmentManager.cpp:110-135`),不等第一次访问——保证任何 world(包括离屏 `RenderOnce` 这种只渲一帧的路径)第一帧拿到的就是有效数据;上传统一走设备级上传扫描(`RenderBufferUploadSystem`)。

### ⚠️ UBO 写入的正确姿势(重要)

这类小 UBO 是 host-visible 持久映射,**不在设备级"待上传"扫描的队列里**——`RenderBufferUploadSystem` 只遍历设备 registry 里存活的 `StagedBuffer`(空 registry 是常态,`src/ecs/systems/render/RenderBufferUploadSystem.cpp:59-67`)。因此写入必须**两步都做**:

- `accessor->Update(data)`(拷贝进映射窗口 + 置脏,`inc/hgl/vk/buffer/StructView.h:200`)接着 `accessor->Commit()`(把窗口范围标脏交 L2,`StructView.h:215`)——只调其中一个都不完整;
- 直接改 `Data()` 返回的指针后必须自己 `Commit()`,否则数据停在映射窗口未提交(历史上 sky 丢数据就是这个原因);
- manager 内所有写入路径都遵守这一约定:`MaterializeSkyUBO`(`EnvironmentManager.cpp:65-66`)、`MaterializeShadowUBO`(`:105-106`)、`MarkDirty`(`:217-234`)、`CommitMaterialized`(`:236-256`);
- 注:`MarkSkyDirty()` 转发到 `MarkDirty`,sky 当帧写入 GPU。`MarkShadowDirty()` 同样转发,但 shadow 的 GPU 写入推迟到交换链 acquire 之后的 `CommitMaterialized`。不要把 shadow 加回 `MarkDirty` 的 GPU 写入。跨帧覆写的症状与修复见 `doc/shadow-ubo-inflight-overwrite.md`。

## 4. 选择层:RT 持有句柄

`IRenderTarget`(`inc/hgl/vk/VKRenderTarget.h`):

```cpp
void          SetEnvironmentProfile(EnvProfileID id);  // 可随时换,下一帧生效
EnvProfileID  GetEnvironmentProfile() const;           // 默认 kEnvProfileDefault
```

选择权在 RT 而不是 world,因为绑定槽位(原 Scene Set)按 RT 生效;world↔RT 本就一一对应(`world->Initialize(device, rt)`)。**订正(2026-09-28)**:Scene 集已整体退场 ⇒ 上述理由作废,权威口径是 **profile 的所有权 / 生命周期随世界**(世界创建时选/建自己的 profile、销毁时归还,内置 default 可共享;C2),RT 侧 `EnvProfileID` 句柄作为过渡保留。world 侧要改环境,通过编辑 profile 数据实现(见下节),不需要 world 再存一份选择。

## 5. 编辑层:`ecs::EnvironmentSystem`(瘦转发)

不再拥有任何 GPU 资源(旧版的 UBO 所有权/析构释放已移除),只做"本 world → 选中 profile → 转发编辑";注册阶段为 `ExecutionPhase::RenderPreBeginFrame`(`src/ecs/systems/render/EnvironmentSystem.cpp:14`),由 `EnsureCoreEcsSystems` 装配并向其注入 `RenderContext`(`src/ecs/core/DefaultSystems.cpp:176`、`:190-191`):

```cpp
SkyInfo *EditSkyInfo();                       // 编辑 RT 选中的 profile(未设置=default)
const SkyInfo *GetSkyInfo() const;
void SetSkyInfo(const SkyInfo &info, bool immediate = true);
void MarkSkyDirty();

// 同构的 shadow 一套(已落地,可作为"新增段落"的参照实现)
ShadowInfo *EditShadowInfo();
const ShadowInfo *GetShadowInfo() const;
void SetShadowInfo(const ShadowInfo &info, bool immediate = true);
void MarkShadowDirty();
```

profile 解析:`ResolveProfileID()` → `context->GetRenderTarget()->GetEnvironmentProfile()`(无效即 `kEnvProfileDefault`,`EnvironmentSystem.cpp:17-26`)。对外 API 与旧版兼容,因此既有调用方无需修改:示例 `example/Environment/AtmosphereSkyMinimal.cpp`、`AtmosphereSkyAmbient.cpp`、`AtmosphereSkySunGizmo.cpp`,以及 `inc/hgl/graph/gizmo/SunDirectionControlSystem.h`/`src/SceneGraph/gizmo/SunDirectionControlSystem.cpp`。

典型用法(示例内):

```cpp
if (auto *sky = environment_system->EditSkyInfo())
{
    sky->SetTime(8, 30, 0);
    environment_system->MarkSkyDirty();     // 立即写入 GPU
}
```

## 6. 绑定层:每帧解析 + pass 开始固定写入

### 6.1 RenderSceneUBOSystem(`CommitViewportUBO` / `SyncBindingsForCurrentCommand`)

系统本身注册在 `ExecutionPhase::RenderFrameSync`(`src/ecs/systems/render/RenderSceneUBOSystem.cpp:80`),`Update()` 与 `Render()` 都只做 `SyncBindingsForCurrentCommand()`（"绑定"已随 Scene 集整体退场 ⇒ 无描述符可推,现为空实现）。**C2 之后 sky / shadow 的地址发布已不在本系统**:

```
本系统自持的 viewport buffer                    → registry->SetViewportAddress(...) → global_addresses.addr_viewport(全帧槽同址)
world(本世界生效 env_profile) → ECSContext::SyncWorldAddresses → 世界表 WorldAddresses.addr_sky / addr_shadow(C2,按帧槽轮转)
camera:CameraSystem 解算（写组件自己的 CameraInfo）→ PublishCamera(Rows) 按 req.camera/主相机写**世界表相机行** → shader 经 pc_root.camera_row 读
(Scene 集 / binding 全部退场:以上都是"写表 / 读表",没有描述符可推)
```

依据:`EnsureViewportUBO`(`:88-148`)→ `SetViewportAddress`(调用点 `:145`);**sky / shadow 已按 C2 迁出全局表** —— 旧的 `ResolveSkyUBO` / `ResolveShadowUBO`(本系统)与 `SetSkyAddress` / `SetShadowAddress`(registry)已整删:每世界按自己的 profile(`ECSContext::GetEnvProfileID()`,默认取本世界 RT 的 `env_profile`)经 `EnvironmentManager::GetSkyAddress` / `GetShadowAddress` 取址,写**本世界**表槽(见 `doc/world-addresses-and-camera-model-plan.md` §4)。`GlobalAddresses` 是 **SSBO、无绑定无集**(`HGL_FRAME_SLOT_TOTAL` 帧槽 × 128B 步长),C2 后只剩 **6 字段 48B**(池基址等长期字段 + `addr_viewport`);`addr_sky` / `addr_shadow` / `addr_camera_info` / `addr_global_render_items` / `addr_draw_item_ids` 一律在世界表里(由归属契约门 `S./W.*-field-ownership` 锁死)。

`SyncGlobalAddressesTable` / `ResolveSkyUBO` / `ResolveShadowUBO` / `GetSkyUBO` / `GetShadowUBO` 均已整删,C2 之后 shader 侧 `sky` / `shadow` 宏直接读 `world_addresses`。旧文提到的 `IsSemanticResolvable` 语义解析器同理已不存在。

### 6.2 ViewUBOCommitSystem(视图三件套契约)

注册在 `ExecutionPhase::RenderBufferCommit`——即每个 RT 的 `PrepareRenderPassSetup` 内、`BeginRenderPass` **之前**:

```
RenderPreBeginFrame → RenderCollect → RenderBatch → [RenderBufferCommit ← 本系统] → RenderBufferUpload → RenderFrameSync
(阶段名真源:`inc/hgl/ecs/core/System.h:22-49` 的 `enum class ExecutionPhase`)
```

每个 RT/RenderPass 开始时**无条件全量写入**(不依赖脏标记):

- camera:`CameraSystem::PublishCamera(Rows)`（相机行；世界共享载体与 camera UBO 已于 C1-5 删除）
- viewport:`RenderSceneUBOSystem::CommitViewportUBO()`
- sky / shadow:**数据**在这里写（sky 每次 `CommitMaterialized` 都写；shadow 仅当前 RT 为交换链时写 `shadow_ring[acquired_image]`，离屏 pass 不写 shadow 槽）；**地址**按世界写世界表（C2：`ECSContext::SyncWorldAddresses` → `WorldAddresses.addr_sky` / `addr_shadow`，见 §6.1）

(实现:`src/ecs/systems/render/ViewUBOCommitSystem.cpp:14` 定阶段,`:17-36` 三个调用点)

约定分类:

| 信息 | 写入策略 |
|---|---|
| camera / viewport / sky | **pass 开始固定全量写** |
| shadow | 交换链帧 acquire 之后只写当前 image 槽;Tick 里的 `MarkShadowDirty` 不写 GPU。见 `doc/shadow-ubo-inflight-overwrite.md` |
| ColorPalette | 变化时写一次(内容基本静态) |
| 材质 SSBO(PBRSurface 等) | 作者侧 `Commit()`,不在此管 |

收益:任何一条"变化才写"路径漏调脏标记导致的静默丢数据,从机制上消除;多 world 的写点收敛为一处。代价:每 pass 多写几百字节 host-visible 映射内存,可忽略。

主 world 和离屏 world 都会装上该系统(Primitive 组件触发 `EnsureCoreEcsSystems`),离屏 `RenderOnce` 路径同样经过 `PrepareRenderPassSetup`,一次性渲染也有 pass 前固定写入。

## 7. Shader 侧消费

**Scene 集(描述符集 0)已整体退场**(订正 2026-09-28):`enum class SceneBinding`、`kSceneBinding*` 常量、`VKGlobalSceneUBOSet` 全部不存在(`inc/hgl/vk/VKGlobalSceneUBOSet.h` 源文件已删),描述符集只剩 **Bindless(0)**(`inc/hgl/common/DescriptorSetTypeDef.h:9-23`)。相机 / sky / shadow / viewport 改由 **BDA 地址**消费,没有绑定号可枚举:

| 数据 | GLSL 读点 | 地址来源 |
|---|---|---|
| camera | `CameraInfoBufferRef(...).cameras[pc_root.camera_row]`(`ShaderLibrary/ubo/scene_ubo.glsl:149`) | 相机行(世界级数据;`pc_root.camera_row` = 行号) |
| sky | `SkyInfoRef(global_addresses.addr_sky)`(`scene_ubo.glsl:74`) | `GlobalAddresses.addr_sky` → 本轮改 `world.addr_sky` |
| shadow | `ShadowInfoRef(global_addresses.addr_shadow)`(`scene_ubo.glsl:147`) | `GlobalAddresses.addr_shadow` → 本轮改 `world.addr_shadow` |
| viewport | `ViewportInfoRef(global_addresses.addr_viewport)`(`scene_ubo.glsl:86`) | `GlobalAddresses.addr_viewport`(**保持全局**) |

**没有 Fog 段**(`FogInfo` / `kSceneBindingFog` / `fog_info.glsl` 都不存在)。GLSL 侧声明在 `ShaderLibrary/ubo/scene_ubo.glsl`(`sky` 宏 :74、`viewport` 宏 :86、`global_addresses` 宏 :119、`shadow` 宏 :147、`camera` 宏 :149);`ShaderLibrary/common/descriptor_macros.glsl` 现只生成 `BINDLESS_SET 0`——`CAMERA_BINDING` / `SKY_BINDING` / `SHADOW_BINDING` / `GLOBAL_ADDRESSES_BINDING` 这些宏随 Scene 集退场一并消失。

- `sky/sky_atmosphere.glsl`:`GetSkyMainLightDir`(:16)、`GetSkyMainLightColor`(:21)、`EvalSkyAtmosphere(方向)`(:26)、`GetSkyAmbientColor`(:52);
- Lit 类材质的编译期开关是 `HGL_USE_SCENE_LIGHTING`(`#define` 由 `src/ShaderGen/template/FragmentTemplateComposer.cpp:376/388` 注入;旧文写的 `enable_scene_lighting` 这个名字全树 0 命中);旧文引用的 `ubo/sky_info.glsl` 已不存在,`sky.*` 统一由 `ubo/scene_ubo.glsl` 声明。直接光 = Cook-Torrance(`ShaderLibrary/lighting/direct_cook_torrance_pbr.glsl`),间接光 = `EvalSkyAtmosphere(N) × baseColor × (1-metallic) × ao`(`ShaderLibrary/lighting/indirect_sky_ambient.glsl:23-30`);主光方向/颜色/环境色的装配点在 `ShaderLibrary/compositor/forward_lighting.glsl:33-35`。

## 8. 现在能做什么(能力清单)

- 一个设备、N 个 profile、任意 RT 绑任意 profile、随时切换(下一帧生效);
- 不设置即有合理默认(default,10:00 太阳,初始化即就绪);
- 运行时动态编辑(太阳时间/方向/强度/天空色):`EditSkyInfo → MarkSkyDirty`,当帧生效;
- 离屏 RT 用不同天光(示例:`example/Basic/RenderToTexture.cpp:233-241`,`sun_intensity = 4.0f`、profile 名 `"RenderToTexture.OffscreenBright"`,补偿贴图二次着色的能量损耗,主屏保持 default);
- 多 world(主/离屏)各按自己选中的 profile 取 sky 数据(未设置即 default);**订正(2026-09-28)**:现状 sky / shadow 地址写在**全局表**字段里,两个 world 选不同 profile 时后解析者覆盖前者 ⇒ 地址改为**按世界**发布(C2)后互不干扰。

## 9. 未来加新信息怎么做(扩展指南)

以加 **Fog(雾)** 为例,完整步骤:

> 前提说明(校对注):Fog 目前**完全未实现**——`inc/hgl/graph/ubo/FogInfo.h`、`SBS_FogInfo`、`kSceneBindingFog`、`EditFogInfo`、`MaterializeFogUBO` 在代码树里都是 0 命中。本节是假想流程,下面每一步的引用都已校正到**当前真实符号**,并指出已落地的等价参照(第二个环境段落 `ShadowInfo` 已按本流程走完一遍)。

1. **定义数据**:`inc/hgl/graph/ubo/FogInfo.h` 写 `struct FogInfo {...};`,挂到 `EnvironmentInfo`:
   ```cpp
   struct EnvironmentInfo {
       SkyInfo  sky;
       FogInfo  fog;      // 新成员
   };
   ```

2. **定义 shader 源**:`inc/hgl/graph/ubo/UBOShaderSources.h` 加 `SBS_FogInfo`(现存量参照 `SBS_ShadowInfo`,`UBOShaderSources.h:44-48`);GLSL 块声明并入 `ShaderLibrary/ubo/scene_ubo.glsl`——**当前整个 `ShaderLibrary/ubo/` 只有这一个文件**,`fog_info.glsl` 不存在。注意布局:vec3/vec2 对齐、需要紧密排布时用 `layout(scalar)`,并用 `static_assert(sizeof(...)==N)` 锁死 CPU 镜像大小(参照 `inc/hgl/graph/ubo/GlobalAddresses.h:27`)。

3. **地址进表**(如果它是"每视图"信息):**不要再想 Scene 集槽位——Scene 集已整体退场**(`kSceneBindingFog` / `GlobalSceneUBOSet` / `SceneBinding` 全部不存在)。世界私有的观察者 / 状态(sky / shadow / env 这类)进**世界地址表 `WorldAddresses`**(与全局表同形:8 槽 × 128B;地址经 `pc_root.addr_world_addresses` 解引用,见权威文档 §1 / §4);GLSL 侧照 `sky` 宏写法加 `#define fog FogInfoRef(world.addr_fog)`,并同步「表归属」契约——世界私有地址**不得**再出现在全局表 `GlobalAddresses` 里;
   - 若更适合做材质级数据(每材质不同),则不走世界表,改走材质 SSBO/纹理槽(`TextureSlot`/`SSBOType`)路线,不经过本管理器。

4. **manager 物化**:`Profile` 加 `StructView<FogInfo> *fog_ubo`;照抄**已落地的第二段落** `MaterializeShadowUBO`(`EnvironmentManager.cpp:70-108`,写入是 `Update(data)+Commit()` 两连,见 `:105-106`);`MarkDirty`/`CommitMaterialized` 把 fog 段一并写入(`EnvironmentManager.cpp:217-234`、`:236-256`)。

5. **地址发布**:`EnvironmentManager::GetFogUBO(id)`(暂无);照抄 `EnsureViewportUBO` → `registry->SetViewportAddress(...)` 这条链(`RenderSceneUBOSystem.cpp:88-148`、调用点 `:145`)在 `RenderSceneUBOSystem` 里加一个 fog 地址写入(本轮之后写**世界表**,不写全局表),解析照抄 `ResolveSkyUBO` / `ResolveShadowUBO`(`:340-400`)。Scene 集已退场 ⇒ **不存在 `global_scene_set->UpdateUBO(kSceneBindingFog, ...)` 这种写法**。旧文建议的通用帮手 **`ResolveEnvUBO` 并未落地**,现实是每个段落一个 `Resolve<X>UBO()`。

6. **编辑转发**:`EnvironmentSystem` 加 `EditFogInfo()/MarkFogDirty()`,与 sky 同构——**已落地的参照是 shadow 那套**:`EditShadowInfo()/GetShadowInfo()/SetShadowInfo()/MarkShadowDirty()`(`inc/hgl/ecs/systems/render/EnvironmentSystem.h:49-54`,`src/ecs/systems/render/EnvironmentSystem.cpp:100-145`)。

7. **RT 选择无需改动**——这正是本架构的目的:选择层只传句柄,不感知内容;新信息自动对所有 profile 生效。

### 已知边界 / 后续方向

- **Scene Set 已整体退场**(订正 2026-09-28):`GlobalSceneUBOSet` / `SceneBinding` / `VKGlobalSceneUBOSet.h` 都不存在,"每 world 每帧改写共享 set"的槽位争抢随之消失。剩下的归属问题是「世界私有数据写在全局表里」——sky / shadow / env 地址本轮改按**世界**发布(权威文档 §4,批次 C2),相机行同理(§2,批次 C1),**viewport 保持全局**;
- profile 尚无序列化/热加载(数据是纯 struct,加即可);
- 尚无按时间驱动 sky 动画(可在 EnvironmentSystem::Update 里 SetTime + MarkSkyDirty 实现)。

## 10. 常见坑(历史教训)

| 坑 | 现象 | 规则 |
|---|---|---|
| 改了 `Data()` 不 `Commit()`(或只 `Update(data)` 不 `Commit()`) | UBO 数据停在映射窗口没上 GPU(sky 全黑/无天光) | 写入必须 `Update(data)+Commit()` 两连;或交给 ViewUBOCommitSystem 每 pass 固定写 |
| 懒创建 UBO 时机太晚 | 第一帧绑定的是未上传 buffer(离屏一次性渲染永久定格) | default 在 GraphicsContext 初始化即物化;其余 profile 首次 `GetSkyUBO` 物化 |
| 环境地址写全局字段 | 多 world 同帧只有最后解析的那个 profile 生效(互踩,主屏可能显示离屏世界的天空) | sky / shadow / env 地址按**世界**发布(世界表 `WorldAddresses`);数据仍只归 EnvironmentManager,world/RT 只持 profile 句柄 |
| `SetClearColor` 晚于 `BeginRendering` | 清屏值一帧滞后;一次性渲染清出未初始化黑色 | clear 值必须在 `vkCmdBeginRendering` 前写入 |

## 11. 本次校对记录与「未能核实」清单(2026-09,以当前代码为准)

### 11.1 已就地修正(旧描述 → 现状)

| 旧描述 | 现状 | 依据 |
|---|---|---|
| `RenderDescriptorBindingSystem`(RDBS) | 已改名 `RenderSceneUBOSystem`(2026-09-08),职责收敛为场景 UBO 数据流 + 材质化注册 | `inc/hgl/ecs/systems/render/RenderSceneUBOSystem.h:35-47` |
| `IsSemanticResolvable` 的 SkyInfo 分支 | 符号不存在;职责由 `ResolveSkyUBO` / `ResolveShadowUBO` 与 `SyncGlobalAddressesTable`(原 `ResolveGlobalAddressesUBO`,Scene 集退场后不再是 UBO 解析)承担 | `src/ecs/systems/render/RenderSceneUBOSystem.cpp:169-194`、`:340-400` |
| `bound_buffers_[4]` 硬编码 4 槽 | (2026-09-28 复核:`VKGlobalSceneUBOSet` 与 `SceneBinding` 已随 Scene 集整体退场,本行及其依据文件均已不存在) | 源文件已删除 |
| Scene 集只有 camera/sky/viewport/palette | (2026-09-28 复核:Scene 集已整体退场 ⇒ **0 个绑定**;数据改走 BDA 表地址,见 §7) | `inc/hgl/common/DescriptorSetTypeDef.h:9-23` |
| `MarkDirty` 只打标记不写数据 | `MarkDirty` = `Update(cpu) + Commit()`,是完整写入 | `src/SceneGraph/module/EnvironmentManager.cpp:217-234` |
| 正确写法 `Update(data)` + `Update()`(无参) | 正确写法 `Update(data)` + `Commit()` | `inc/hgl/vk/buffer/StructView.h:200`、`:215` |
| `enable_scene_lighting` | `HGL_USE_SCENE_LIGHTING` | `src/ShaderGen/template/FragmentTemplateComposer.cpp:376/388` |
| `ubo/sky_info.glsl` 读 `sky.*` | 该文件不存在;`sky` 块声明在 `ubo/scene_ubo.glsl:61-70` | 全树 grep 0 命中 |
| 帧序列 `RenderBeginFrame → Collect → … → FrameSync` | `RenderPreBeginFrame → RenderCollect → RenderBatch → RenderBufferCommit → RenderBufferUpload → RenderFrameSync` | `inc/hgl/ecs/core/System.h:22-49` |
| 环境信息只有 `SkyInfo` | `EnvironmentInfo{ sky, shadow }` | `inc/hgl/graph/ubo/EnvironmentInfo.h:20-26` |

### 11.2 未能核实 / 属设计态(未修改,或仅加标注)

- **Fog 相关全部符号**(`FogInfo.h`、`SBS_FogInfo`、`kSceneBindingFog`、`fog_info.glsl`、`MaterializeFogUBO`、`GetFogUBO`、`EditFogInfo`、`MarkFogDirty`、`ResolveEnvUBO`):全树 0 命中。§9 已就地标注为假想流程,并把每步引用改指到已落地的 shadow 等价物。
- `RenderBeginFrame`:0 命中(已换成 `RenderPreBeginFrame`)。
- `AtmosphereSky`:作为符号 0 命中;实际是示例文件名前缀(`example/Environment/AtmosphereSky{Minimal,Ambient,SunGizmo}.cpp`)。
- `bound_buffers_`:0 命中(见 11.1)。
- §8 「`RenderToTexture` 离屏 RT 用 OffscreenBright profile」:已核实为 `example/Basic/RenderToTexture.cpp:233-241`(profile 名 `"RenderToTexture.OffscreenBright"`,非 `"OffscreenBright"`);同目录 `RenderToTextureColorDepth.cpp` 用 `"RTTColorDepth.n"` 命名,未逐一核对。
- §10 `SetClearColor` 早于 `BeginRendering` 的时序坑:属历史经验条目,未逐行核实,保留原文。

### 11.3 2026-09-28 订正:相机 / 环境 / 地址表(权威文档对齐)

依据 `doc/world-addresses-and-camera-model-plan.md`(§1 三档地址分层、§2 相机世界级存储、§3 三级解析、§4 Env 随世界、§7 订正清单):

| 旧断言 | 现状 / 定稿 |
|---|---|
| sky / shadow 经 Scene Set binding 1 / binding 5 绑定 | **Scene 集整体退场**;sky / shadow 现经 `global_addresses.addr_sky` / `addr_shadow` 解引用,批次 **C2** 之后进**世界表** `WorldAddresses`(shadow 每槽一份 ring) |
| `GlobalAddressesInfo` 是 7×uint64 的 UBO(Set0/binding4) | `GlobalAddresses` 是 **SSBO**、**无绑定无集**、`HGL_FRAME_SLOT_TOTAL` 帧槽 × 128B 步长(`inc/hgl/graph/ubo/GlobalAddresses.h:11-57`) |
| camera 是"场景 UBO 绑定"/ UBO 池 | camera 是**相机行**;相机属**世界级数据**(世界私有 16 槽 × 8 帧槽,0 号槽恒为本世界默认相机),shader 经 `pc_root.camera_row` 读(`ShaderLibrary/ubo/scene_ubo.glsl:149`) |
| 环境数据"设备级单份"、RT 只持句柄即无争抢 | 数据仍集中管理,但 **profile 的生命周期随世界**(C2),sky / shadow / env **地址发布按世界**;**viewport 保持全局** |
| `VKGlobalSceneUBOSet` / `kSceneBinding*` / `SceneBinding` | 全部不存在(源文件已删),描述符集只剩 Bindless(0) |
