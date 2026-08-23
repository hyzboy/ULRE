# EnvironmentSystem / 环境综合信息管理

> 版本:2026-08(IndirectMeshDraw 分支)
> 相关文件:`inc/hgl/graph/ubo/EnvironmentInfo.h`、`inc/hgl/graph/module/EnvironmentManager.h`、`src/SceneGraph/module/EnvironmentManager.cpp`、`inc/hgl/ecs/systems/render/EnvironmentSystem.h/.cpp`、`inc/hgl/ecs/systems/render/ViewUBOCommitSystem.h/.cpp`、`inc/hgl/vk/VKRenderTarget.h`、`src/ecs/systems/render/RenderDescriptorBindingSystem.cpp`

## 1. 这套东西解决什么问题

统一管理"世界级环境信息"(目前只有天空 SkyInfo),并回答三个问题:

1. **数据放哪**——集中在一个设备级管理器,而不是每个 ECS world 各自建 UBO(旧 EnvironmentSystem 模式,多 RT 下会互相争抢共享描述符集的 sky binding);
2. **谁用哪份**——RT/WORLD 只持有一个 `EnvProfileID` 引用,不复制数据;未设置即用内置 default;
3. **什么时候写 GPU**——视图三件套(camera/viewport/sky)在每个 RT/RenderPass 开始时**固定全量写入**,不依赖脏标记。

### 分层总览

```
┌─ 数据层  EnvironmentInfo(纯数据,一个 Profile 的全部环境状态)
│
├─ 管理层  EnvironmentManager(GraphicsContext 模块,设备级唯一)
│           Profile 注册 / GPU UBO 物化 / 统一写入
│
├─ 选择层  IRenderTarget::GetEnvironmentProfile()(未设置 = default)
│
├─ 编辑层  ecs::EnvironmentSystem(瘦转发,不拥有 GPU 资源)
│
└─ 绑定层  RenderDescriptorBindingSystem(每帧 RT→句柄→UBO→Scene Set)
            ViewUBOCommitSystem(pass 开始固定写入 camera/viewport/sky)
```

## 2. 数据层:`EnvironmentInfo` 与 `EnvProfileID`

`inc/hgl/graph/ubo/EnvironmentInfo.h`:

```cpp
using EnvProfileID = uint32_t;
constexpr EnvProfileID kEnvProfileInvalid = 0;
constexpr EnvProfileID kEnvProfileDefault = 1;   // 内置默认 Profile 的固定句柄

struct EnvironmentInfo
{
    SkyInfo sky;      // 目前唯一成员;雾/环境光/IBL 未来加在这里
};
```

- **纯数据、无 GPU 概念**,可直接序列化;CPU 侧唯一权威副本存在 `EnvironmentManager::Profile::cpu`。
- Profile 以 `EnvProfileID` 句柄引用(名字仅用于创建/查找,重复名字返回已有句柄)。
- `kEnvProfileDefault = 1` 是保留句柄,manager 初始化时自动创建。

## 3. 管理层:`EnvironmentManager`

`GraphModule` 体系(`GraphicsContext::GetManager<EnvironmentManager>()` / `GetEnvironmentManager()`),在 `GraphicsContext::Initialize` 中注册,**必须在 BufferManager 之后**(default profile 物化需要它)。

### Profile 结构与 GPU 物化

```cpp
struct Profile
{
    EnvProfileID    id;
    AnsiString      name;
    EnvironmentInfo cpu;                              // CPU 权威数据
    StructuredBufferAccessor<SkyInfo> *sky_ubo;       // sky 段 GPU 物化(懒创建)
};
```

- 每个 profile 的每类信息物化为**一份自己的 UBO**;多个 RT 选同一 profile 时**共享同一块 buffer**(只读,数据不变零上传),不按 RT 复制。
- UBO 命名 `"SkyUBO:<profile名>"`,走 `BufferManager::CreateUBO` + `StructuredBufferAccessor`(shader source 用 `mtl::SBS_SkyInfo`,对应 Scene Set binding 1)。

### 关键 API

| API | 说明 |
|---|---|
| `Create(name, init_info)` | 注册 profile(重名返回已有句柄) |
| `Find(name)` | 名字查句柄,失败 `kEnvProfileInvalid` |
| `Edit(id)` / `Get(id)` | 取 CPU 权威数据指针(可写/只读),无效句柄返回 nullptr |
| `MarkDirty(id)` | 数据改完调用:立即写入 GPU |
| `GetSkyUBO(id)` | 绑定层用,取 sky 段 GPU buffer(懒物化;无效句柄回退 default) |
| `CommitMaterialized()` | ViewUBOCommitSystem 用:所有已物化 profile 全量写入 |

### default Profile 的特殊性

`OnGraphicsContextChanged`(即 GraphicsContext 初始化完成)时自动创建:

- 内容:`sky.SetTime(10, 0, 0)`(上午十点的太阳);
- **立即物化并写入 GPU**,不等第一次访问——保证任何 world(包括离屏 `RenderOnce` 这种只渲一帧的路径)第一帧拿到的就是有效数据。

### ⚠️ UBO 写入的正确姿势(重要)

这类小 UBO 是 host-visible 持久映射,**不在设备级 dirty 扫描 registry 里**(那里只有 `StagedBuffer` 家族,即 VAB/IBO/顶点数据)。因此:

- `accessor->MarkDirty()` **只打标记,不写数据**——标记没人消费,数据就永远没上 GPU(历史上 sky 丢数据就是这个原因);
- 正确写法是 `accessor->Update(data)`(拷贝+置脏)接着 `accessor->Update()`(无参,内部 `CommitInternal → DeviceBuffer::Write` 直写/路由 staged);
- manager 内所有写入(MarkDirty / 物化 / CommitMaterialized)都遵守这一约定。

## 4. 选择层:RT 持有句柄

`IRenderTarget`(`inc/hgl/vk/VKRenderTarget.h`):

```cpp
void          SetEnvironmentProfile(EnvProfileID id);  // 可随时换,下一帧生效
EnvProfileID  GetEnvironmentProfile() const;           // 默认 kEnvProfileDefault
```

选择权在 RT 而不是 world,因为绑定槽位(Scene Set)按 RT 生效;world↔RT 本就一一对应(`world->Initialize(device, rt)`)。world 侧要改环境,通过编辑 profile 数据实现(见下节),不需要 world 再存一份选择。

## 5. 编辑层:`ecs::EnvironmentSystem`(瘦转发)

不再拥有任何 GPU 资源(旧版的 UBO 所有权/析构释放已移除),只做"本 world → 选中 profile → 转发编辑":

```cpp
SkyInfo *EditSkyInfo();                       // 编辑 RT 选中的 profile(未设置=default)
const SkyInfo *GetSkyInfo() const;
void SetSkyInfo(const SkyInfo &info, bool immediate = true);
void MarkSkyDirty();
```

profile 解析:`context->GetRenderTarget()->GetEnvironmentProfile()`。对外 API 与旧版兼容,因此 AtmosphereSky 系列、SunDirectionControlSystem 等既有调用方无需修改。

典型用法(示例内):

```cpp
if (auto *sky = environment_system->EditSkyInfo())
{
    sky->SetTime(8, 30, 0);
    environment_system->MarkSkyDirty();     // 立即写入 GPU
}
```

## 6. 绑定层:每帧解析 + pass 开始固定写入

### 6.1 RenderDescriptorBindingSystem(`ResolveSkyUBO`)

每帧 RenderFrameSync 阶段:

```
world 的 RT → GetEnvironmentProfile() → manager->GetSkyUBO(id)
→ GlobalSceneUBOSet set0/binding1 (kSceneBindingSky)
```

不再"自动补注册 EnvironmentSystem";`IsSemanticResolvable` 的 SkyInfo 分支同样直接查 manager。

### 6.2 ViewUBOCommitSystem(视图三件套契约)

注册在 `ExecutionPhase::RenderBufferCommit`——即每个 RT 的 `PrepareRenderPassSetup` 内、`BeginRenderPass` **之前**:

```
RenderBeginFrame → Collect → Batch → [RenderBufferCommit ← 本系统] → Upload → FrameSync
```

每个 RT/RenderPass 开始时**无条件全量写入**(不依赖脏标记):

- camera:`CameraSystem::CommitCameraUBO()`
- viewport:`RenderDescriptorBindingSystem::CommitViewportUBO()`
- sky:`EnvironmentManager::CommitMaterialized()`

约定分类:

| 信息 | 写入策略 |
|---|---|
| camera / viewport / sky | **pass 开始固定全量写**(视图状态,时序契约) |
| ColorPalette | 变化时写一次(内容基本静态) |
| 材质 SSBO(PBRSurface 等) | 作者侧 `Commit()`,不在此管 |

收益:任何一条"变化才写"路径漏调脏标记导致的静默丢数据,从机制上消除;多 world 的写点收敛为一处。代价:每 pass 多写几百字节 host-visible 映射内存,可忽略。

主 world 和离屏 world 都会装上该系统(Primitive 组件触发 `EnsureCoreEcsSystems`),离屏 `RenderOnce` 路径同样经过 `PrepareRenderPassSetup`,一次性渲染也有 pass 前固定写入。

## 7. Shader 侧消费

- Scene Set(描述符集 0)binding 1 = sky UBO(`kSceneBindingSky`,`inc/hgl/common/DescriptorSetTypeDef.h`);
- `sky/sky_atmosphere.glsl`:`GetSkyMainLightDir/GetSkyMainLightColor/EvalSkyAtmosphere(方向)/GetSkyAmbientColor`;
- Lit 类材质(`enable_scene_lighting`)经 `ubo/sky_info.glsl` 读 `sky.*`,直接光 = Cook-Torrance,间接光 = `EvalSkyAtmosphere(N) × baseColor × (1-metallic) × ao`(方向相关环境光)。

## 8. 现在能做什么(能力清单)

- 一个设备、N 个 profile、任意 RT 绑任意 profile、随时切换(下一帧生效);
- 不设置即有合理默认(default,10:00 太阳,初始化即就绪);
- 运行时动态编辑(太阳时间/方向/强度/天空色):`EditSkyInfo → MarkSkyDirty`,当帧生效;
- 离屏 RT 用不同天光(示例:`RenderToTexture` 的离屏 RT 用 `sun_intensity 4.0` 的 "OffscreenBright" profile 补偿贴图二次着色的能量损耗,主屏保持 default);
- 多 world(主/离屏)共享同一份 sky UBO 数据,无争抢。

## 9. 未来加新信息怎么做(扩展指南)

以加 **Fog(雾)** 为例,完整步骤:

1. **定义数据**:`inc/hgl/graph/ubo/FogInfo.h` 写 `struct FogInfo {...};`,挂到 `EnvironmentInfo`:
   ```cpp
   struct EnvironmentInfo {
       SkyInfo  sky;
       FogInfo  fog;      // 新成员
   };
   ```

2. **定义 shader 源**:`inc/hgl/graph/ubo/UBOShaderSources.h` 加 `SBS_FogInfo`(指向 GLSL UBO 声明);`ShaderLibrary/ubo/fog_info.glsl` 写 std140 块声明(注意 vec3 16 字节对齐,CPU 结构用 `alignas(16)` 镜像)。

3. **Scene Set 加槽位**(如果它是"每视图"信息):
   - `kSceneBindingFog` 新常量;`GlobalSceneUBOSet` 当前 `bound_buffers_[4]` 是硬编码 4 槽(camera/sky/viewport/palette),**扩槽需要同步改布局数组、Init 的 pool/layout 创建、UpdateUBO 的边界**;DSL 对未使用 binding 已带 PARTIALLY_BOUND 位,布局变更会使管线 shader 缓存失效,需要重编验证;
   - 若更适合做材质级数据(每材质不同),则不走 Scene Set,改走材质 SSBO/纹理槽(`TextureSlot`/`SSBOType`)路线,不经过本管理器。

4. **manager 物化**:`Profile` 加 `StructuredBufferAccessor<FogInfo> *fog_ubo`;仿照 `MaterializeSkyUBO` 写 `MaterializeFogUBO`;`MarkDirty`/`CommitMaterialized` 把 fog 段一并写入(依旧 `Update(data)+Update()` 两连)。

5. **绑定**:`EnvironmentManager::GetFogUBO(id)`;RDBS `ApplyResourceLayoutBindings` 里 `global_scene_set->UpdateUBO(kSceneBindingFog, ...)`;`ResolveSkyUBO` 同款 RT→句柄解析,建议抽成通用 `ResolveEnvUBO` 帮手。

6. **编辑转发**:`EnvironmentSystem` 加 `EditFogInfo()/MarkFogDirty()`,与 sky 同构。

7. **RT 选择无需改动**——这正是本架构的目的:选择层只传句柄,不感知内容;新信息自动对所有 profile 生效。

### 已知边界 / 后续方向

- **Scene Set 仍是设备级单例**:多 world 同帧交错录命令时,sky/camera/viewport binding 是"每 world 每帧改写共享 set"(当前串行渲染安全;sky 因 profile 化已消除数据争抢,但槽位争抢仍在)。Phase 2 计划:Scene Set 实例下放到 RT,layout 留设备级;
- profile 尚无序列化/热加载(数据是纯 struct,加即可);
- 尚无按时间驱动 sky 动画(可在 EnvironmentSystem::Update 里 SetTime + MarkSkyDirty 实现)。

## 10. 常见坑(历史教训)

| 坑 | 现象 | 规则 |
|---|---|---|
| 只 `MarkDirty` 不 `Update()` | UBO 数据永远没上 GPU(sky 全黑/无天光) | 这类 UBO 必须显式 `Update()`;或交给 ViewUBOCommitSystem 固定写 |
| 懒创建 UBO 时机太晚 | 第一帧绑定的是未上传 buffer(离屏一次性渲染永久定格) | default 在 GraphicsContext 初始化即物化;其余 profile 首次 `GetSkyUBO` 物化 |
| 每 world 自建环境 UBO | 多 RT 争抢共享 Scene Set binding,后写者覆盖 | 环境数据只归 EnvironmentManager,world/RT 只持句柄 |
| `SetClearColor` 晚于 `BeginRendering` | 清屏值一帧滞后;一次性渲染清出未初始化黑色 | clear 值必须在 `vkCmdBeginRendering` 前写入 |
