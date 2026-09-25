# SKILL: ECS 架构解耦与组件设计规范

## 目标
建立 ULRE 引擎中关于 ECS（Entity-Component-System）、场景环境与渲染管线解耦的核心设计原则，防止未来因“基类膨胀”、“职责不清”或“底层渲染细节泄漏至应用层”导致架构退化。

---

## 一、 核心设计三大铁则

### 1. 场景环境归属 vs 实体行为归属（严防越界）
* **场景环境属性**：太阳光方向与颜色（`SkyInfo`）、环境雾效、全局阴影开闭与级联距离配置、全局曝光与调色（ColorPalette）。
  * ❌ **错误**：在实体或可渲染组件上强行塞入全局阴影级联 RT、光照矩阵计算逻辑。
  * ✅ **正确**：统一由场景/世界级系统（如 `EnvironmentSystem`）管理，由引擎内部自动申请/回收离屏 RT 并物化 UBO。
* **实体行为属性**：单个物体是否投射阴影、最大阴影距离、是否受影、材质着色属性、空间位姿。
  * ✅ **正确**：作为实体上的独立组件挂载（如 `TransformComponent`, `MaterialComponent`, `ShadowComponent`）。

### 2. 正交子组件 vs 臃肿上帝基类（拒绝 God Class）
* 引擎中不可避免存在多个通用的渲染基类（如 `RenderableComponent`）。
* **判定准则**：如果一个属性不是所有可渲染对象（包括普通网格、实例化网格、UI 矩形、文本 Text、调试线条 Lines、粒子）都严格必需的底层几何特性，**严禁平铺进基类**！
* ❌ **反面模式**：
  ```cpp
  // 糟糕的设计：所有对象都要为阴影、材质行、物理数据买单
  class RenderableComponent : public Component {
      // 几何...
      // 阴影状态（文本和UI不需要，被迫浪费内存）
      bool cast_shadow;
      float shadow_distance;
      // 材质数据...
      // 碰撞代理...
  };
  ```
* ✅ **正向模式（子组件正交挂接）**：
  保持 `RenderableComponent` 轻量纯粹；特异化功能独立为伴随组件：
  * 材质特性 → `MaterialComponent`
  * 阴影特性 → `ShadowComponent`
  * 空间变换 → `TransformComponent`
  * 包围盒 → `BoundingBoxComponent`

### 3. 约定优于配置（缺省回退范式：未挂载即默认，挂载即特异化）
独立组件最容易引起的诟病是“增加模板代码”（每次创建物体都要手动挂载十几个组件）。
* **规范约定**：
  * **未显式挂载组件**：管线视为**全局标准默认行为**。
    * 例如：物体未挂载 `ShadowComponent`，管线默认其 `cast_shadow = true`, `receive_shadow = true`, 距离跟随场景全局。
  * **显式挂载组件**：仅代表**特异化控制 / 调优定制**。
    * 例如：地面只想接收不想投射，挂载 `ShadowComponent` 并设置 `SetCastShadow(false)`；远景树木只在 50 米内投阴，挂载并设置 `SetMaxDistance(50.0f)`。
* **效果**：普通几何体一行代码都不用多写，特异化几何体意图明确。

---

## 二、 渲染管线托管边界（应用层非必要不干预）

### 1. 应用层职责：声明意图 (Declare Intent)
应用层开发者的心智模型应当聚焦在“场景有什么”和“表现如何”：
* 场景初始化：`env->SetSkyInfo(...)`，`env->EnableMainLightShadow(...)`
* 实体特异化：`ground_shadow->SetCastShadow(false)`
* 帧循环：`world->Render(deltaTime)`

### 2. 引擎管线职责：自动编排与闭环执行 (Orchestrate & Execute)
严禁将渲染 Pass 的中间状态暴露给应用层代码去驱动。
* ❌ **反面模式（底层细节泄漏）**：
  * 应用层 `Tick()` 内部显式持有 `csm_controller`；
  * 应用层手动写 `for(c = 0; c < 4; ++c) ecs_context->RenderTo(...)`；
  * 应用层为了防止地面自阴影，在每帧阴影渲染前后手动调用 `ground->SetVisible(false)` 与 `SetVisible(true)`。
* ✅ **正确模式（管线内聚托管）**：
  * 由 ECS 内部注册的系统（如 `ShadowSystem`）在主 Pass 渲染前自动调度离屏阴影 Pass；
  * 收集系统 `RenderPrimitiveCollectSystem` 自动识别 `is_shadow_pass` 并依据组件的 `CanCastShadow()` 自动剔除；
  * 自动化完成，外部完全无感知。

---

## 三、 跨组件访问与性能保护范式

在 ECS 中，解耦为多个组件后，渲染收集系统等核心热路径（Hot Path）若频繁使用反射式哈希查找（如 `entity->GetComponent<T>()`），会导致明显的 CPU 指针跳跃与寻址开销。

### 标准优化方案：挂载事件弱指针缓存 (Cached Weak Pointer)
在主体组件（如 `PrimitiveComponent`）内部保留轻量伴随指针：
```cpp
class PrimitiveComponent : public RenderableComponent
{
private:
    ShadowComponent *cached_shadow_component = nullptr; // 非拥有弱引用

public:
    void SetCachedShadowComponent(ShadowComponent *sc) { cached_shadow_component = sc; }

    bool CanCastShadow() const
    {
        // 存在则用组件配置，不存在回退到默认约定（true）
        return cached_shadow_component ? cached_shadow_component->CanCastShadow() : true;
    }
};
```
* **生命周期绑定**：在 `Entity::AddComponent<ShadowComponent>()` 或 `Entity::RemoveComponent()` 时触发一次弱引用更新。
* **收益**：收集系统热循环中直接调用 `primitive->CanCastShadow()`，性能等同于直接访问成员变量，彻底消除逐帧哈希开销。

---

## 四、 常见系统解耦检查清单 (Checklist)

在设计或引入新渲染特性时，对照此清单评估：

- [ ] **是否属于场景全局**？若是全局环境属性（雾、光照、大气），放入 `EnvironmentSystem` / Profile，不要放组件。
- [ ] **是否所有图元都需要**？若只有部分网格需要（如布料模拟、阴影代理、LOD生成），必须做成独立 Sub-Component。
- [ ] **是否实现了缺省回退**？不挂载该组件的普通实体是否依然能正常运行（符合常规预期）。
- [ ] **是否避免了逐帧 GetComponent**？热路径上是否建立了弱引用缓存。
- [ ] **Pass 是否做到了内部闭环**？是否把本该由 RenderGraph / System 编排的离屏 Pass 暴露给上层 `Tick` 手动循环。
