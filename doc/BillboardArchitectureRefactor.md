# Billboard System - 从混合向解耦的架构演进

## 问题诊断

### 当前架构（紧耦合）

```
BillboardComponent
├─ 矩形渲染参数（size, front_face）
├─ 材质/纹理管理（texture_path, sampler）
└─ 隐含的"面向相机"语义

BillboardRenderSystem
├─ 旋转计算（UpdateBillboardRotation）
├─ 材质资源创建（EnsureBillboardMaterial）
└─ 缩放控制（pixel_size → world_space转换）
```

**问题：**
1. 旋转算法只能用于Billboard，无法复用到粒子、UI、符号等
2. 矩形渲染逻辑写死在Billboard中，无法复用给其他2D元素
3. 如果要给其他东西（如粒子系统）加"面向相机"功能，需要复制代码
4. 组件职责不清晰，难以测试和维护

---

## 解决方案：三角解耦

### 核心理念

```
Billboard = QuadComponent + FacingTransformComponent

旋转和缩放 ─┐
           ├─ 独立的Transform算法模块
材质纹理   ─┤
           └─ 可复用到任何需要的地方

矩形渲染   ─┬─ 独立的Quad/Rectangle渲染模块
           └─ 可复用到UI、粒子等
```

### 架构设计

#### 1. **QuadComponent** (新建)

**职责：** 只管理"绘制矩形"的参数

```cpp
// inc/hgl/ecs/components/QuadComponent.h
class QuadComponent : public PrimitiveComponent
{
private:
    // 只关心矩形本身
    hgl::math::Vector2u pixel_size;      // 像素大小（屏幕固定）
    glm::vec2 world_size;                // 世界大小（缩放随距离）
    bool fixed_size;                     // 大小模式
    VkFrontFace front_face;              // 面向

    // 材质可选
    hgl::OSString texture_path;
    hgl::OSString applied_texture;
    bool texture_dirty;
    
public:
    void SetPixelSize(uint32_t w, uint32_t h);
    void SetWorldSize(float w, float h);
    void SetFrontFace(VkFrontFace face);
    void SetTexturePath(const hgl::OSString& path);
    // ... 只有矩形相关的方法，没有旋转逻辑
};
```

**使用场景：**
- 静止的矩形精灵
- 粒子系统中的quad粒子
- UI元素（如2D图标）
- 贴花（decal）

---

#### 2. **FacingTransformComponent** (新建)

**职责：** 只管理"面向某个对象/相机的旋转"计算

```cpp
// inc/hgl/ecs/components/FacingTransformComponent.h
enum class FacingMode
{
    LookAtCamera,           // 面向相机
    LookAtTarget,           // 面向指定目标
    Billboard,              // 仅绕Y轴旋转，但顶部指向相机
    // 未来可扩展...
};

class FacingTransformComponent : public Component
{
private:
    FacingMode facing_mode = FacingMode::LookAtCamera;
    glm::vec3 target_position;           // 如果是LookAtTarget模式
    float rotation_speed;                 // 平滑旋转速度
    
public:
    void SetFacingMode(FacingMode mode) { facing_mode = mode; }
    void SetTargetPosition(const glm::vec3& pos) { target_position = pos; }
    FacingMode GetFacingMode() const { return facing_mode; }
    
    // 这个组件本身不做旋转，只存储配置
    // 由FacingTransformSystem来处理实际计算
};
```

**可复用场景：**
- Billboard文本标签
- 粒子系统（orientation particles）
- 动态光源符号
- 剧情指示符
- 任何需要"面向相机"的东西

---

#### 3. **BillboardComponent** (重构为组合)

**新身份：** 一个"配方"，不含渲染逻辑

```cpp
// inc/hgl/ecs/components/BillboardComponent.h
class BillboardComponent : public Component
{
private:
    // Billboard = Quad + FacingTransform
    // 所有工作都由这两个子组件完成
    
public:
    // 便捷API（代理）
    void SetSize(uint32_t w, uint32_t h)
    {
        auto quad = entity->GetComponent<QuadComponent>();
        quad->SetPixelSize(w, h);
    }
    
    void SetTexture(const hgl::OSString& path)
    {
        auto quad = entity->GetComponent<QuadComponent>();
        quad->SetTexturePath(path);
    }
    
    void SetFacingMode(FacingMode mode)
    {
        auto facing = entity->GetComponent<FacingTransformComponent>();
        facing->SetFacingMode(mode);
    }
    
    // 工厂方法
    static void CreateOn(Entity* entity)
    {
        // 自动创建和配置两个子组件
        entity->AddComponent<QuadComponent>();
        entity->AddComponent<FacingTransformComponent>();
        entity->AddComponent<BillboardComponent>();  // 自己作为标记/配置容器
    }
};
```

**核心特性：**
- 可在创建时自动添加两个子组件
- 提供便捷API（兼容旧代码）
- 本身不做任何渲染/计算工作

---

### 系统架构

#### 4. **QuadRenderSystem** (新建)

**职责：** 管理所有Quad的渲染资源

```cpp
// inc/hgl/ecs/systems/render/QuadRenderSystem.h
class QuadRenderSystem : public System
{
private:
    static graph::Primitive* shared_quad_primitive;      // 共享矩形几何
    static graph::Pipeline* shared_pipeline;
    static graph::Sampler* shared_sampler;
    
    SamplerCache sampler_pool;                           // 采样器池
    TextureCache texture_cache;                          // 纹理缓存
    
public:
    void Update(float deltaTime) override;
    
private:
    bool EnsureSharedResources();
    bool EnsureQuadMaterial(QuadComponent* quad);
    // 与矩形相关的逻辑：材质、纹理、采样器
};
```

**职责范围：**
- ✅ 创建/管理矩形(Quad)几何
- ✅ 加载/缓存纹理
- ✅ 管理采样器池
- ❌ 不负责旋转计算
- ❌ 不负责面向相机逻辑

---

#### 5. **FacingTransformSystem** (新建)

**职责：** 计算所有需要"面向X"的实体的旋转

```cpp
// inc/hgl/ecs/systems/transform/FacingTransformSystem.h
class FacingTransformSystem : public System
{
private:
    const graph::CameraInfo* camera_info;
    
public:
    void Update(float deltaTime) override;
    
private:
    bool UpdateFacingRotation(FacingTransformComponent* facing,
                             TransformComponent* transform,
                             float dt);
    // 旋转算法 - 与QuadComponent无关
};
```

**职责范围：**
- ✅ 计算"面向相机"的四元数
- ✅ 计算"面向目标"的四元数
- ✅ 平滑旋转过渡
- ❌ 不负责材质
- ❌ 不负责几何

---

#### 6. **BillboardRenderSystem** (过时)

**新角色：** 可删除，功能分解到QuadRenderSystem和FacingTransformSystem

```cpp
// 选项1：保留为空壳，兼容旧代码
class BillboardRenderSystem : public System
{
    // 现在什么都不做，或只做日志/调试
    void Update(float deltaTime) override { /* no-op */ }
};

// 选项2：直接删除
```

---

## 数据流对比

### 现在（紧耦合）

```
Entity
├─ BillboardComponent
│  └─ 包含：size, texture, front_face, 隐含"旋转"
├─ TransformComponent
│
BillboardRenderSystem   (处理旋转计算)
BillboardTexureSystem   (处理材质绑定)
RenderPrimitiveCollectSystem (渲染)
```

### 改进后（解耦）

```
Entity
├─ QuadComponent
│  └─ size, texture, front_face（矩形的）
├─ FacingTransformComponent
│  └─ facing_mode, target_position（旋转的）
├─ TransformComponent
│
FacingTransformSystem  (计算旋转 -> 更新Transform)
QuadRenderSystem       (处理材质、纹理、采样器)
RenderPrimitiveCollectSystem (渲染)
```

---

## 使用示例对比

### 现在

```cpp
auto entity = world->CreateEntity("Billboard");
auto billboard = entity->AddComponent<BillboardComponent>();
billboard->SetPixelSize(256, 256);
billboard->SetTexturePath(OS_TEXT("res/lena.Tex2D"));
billboard->SetVisible(true);

// 一切都在BillboardRenderSystem中处理
```

### 改进后

```cpp
// 方案1：使用便捷工厂
auto entity = world->CreateEntity("Billboard");
auto billboard = entity->AddComponent<BillboardComponent>();  // 自动添加Quad + FacingTransform
billboard->SetSize(256, 256);
billboard->SetTexture(OS_TEXT("res/lena.Tex2D"));

// 方案2：手工组合（更灵活）
auto entity = world->CreateEntity("CustomSprite");
auto quad = entity->AddComponent<QuadComponent>();
quad->SetWorldSize(2.0f, 3.0f);
quad->SetTexturePath(OS_TEXT("res/sprite.Tex2D"));
// 没有FacingTransform，所以它是静止的

// 方案3：粒子系统中的朝向粒子
auto particle = world->CreateEntity("Particle");
auto quad = particle->AddComponent<QuadComponent>();
auto facing = particle->AddComponent<FacingTransformComponent>();
facing->SetFacingMode(FacingMode::LookAtCamera);
// 自动面向相机，无需额外代码
```

---

## 渐进迁移计划

### Phase 1：添加新组件（不影响现有代码）

1. 创建 `QuadComponent`, `FacingTransformComponent`
2. 创建 `QuadRenderSystem`, `FacingTransformSystem`
3. 保持 `BillboardComponent` 和 `BillboardRenderSystem` 不变

### Phase 2：BillboardComponent 重构（兼容模式）

```cpp
// BillboardComponent现在内部创建Quad + FacingTransform
class BillboardComponent : public Component
{
    QuadComponent* quad = nullptr;
    FacingTransformComponent* facing = nullptr;
    
    void OnAttach(Entity* entity) override
    {
        quad = entity->GetOrAddComponent<QuadComponent>();
        facing = entity->GetOrAddComponent<FacingTransformComponent>();
    }
    
    // 代理所有API调用
    void SetPixelSize(uint32_t w, uint32_t h) 
    { 
        quad->SetPixelSize(w, h);   // 转发
    }
};
```

### Phase 3：旧系统废除

删除 `BillboardRenderSystem`（已由新系统替代）

---

## 优势总结

| 方面 | 现在 | 改进后 |
|------|------|--------|
| **代码复用** | Billboard专用 | Quad可用于UI、粒子；FacingTransform可用于任何东西 |
| **职责清晰** | 混乱 | 每个组件/系统只做一件事 |
| **测试难度** | 高（需模拟相机+旋转+渲染） | 低（Quad测试只需参数，FacingTransform只需四元数计算） |
| **维护成本** | 高（改一个地方多处受影响） | 低（修改隔离在单一系统） |
| **灵活组合** | 固定（只能Billboard） | 自由（任意组合Quad+Facing+其他） |
| **新功能成本** | 高（更多Billboard变体？） | 低（新需求 = 组合现有组件） |
| **性能** | 相同 | 基本相同（可能更好，因为QuadRenderSystem更优化） |

---

## 文件结构

```
inc/hgl/ecs/
├── components/
│   ├── BillboardComponent.h          (重构：代理模式)
│   ├── QuadComponent.h               (新：矩形参数)
│   └── FacingTransformComponent.h   (新：旋转配置)
│
└── systems/
    ├── render/
    │   ├── QuadRenderSystem.h        (新：矩形渲染)
    │   └── BillboardRenderSystem.h   (弃用或空)
    │
    └── transform/
        └── FacingTransformSystem.h   (新：旋转更新)

src/ecs/
├── components/...
└── systems/...
```

---

## 实现建议

### 优先级

1. **高优先级** (Phase 1)
   - [ ] 创建 `QuadComponent` (简单复制+简化BillboardComponent)
   - [ ] 创建 `FacingTransformComponent` (存储配置)
   - [ ] 创建 `QuadRenderSystem` (移动BillboardRenderSystem的渲染逻辑)
   - [ ] 创建 `FacingTransformSystem` (移动旋转计算逻辑)

2. **中优先级** (Phase 2)
   - [ ] 重构 `BillboardComponent` 为代理
   - [ ] 更新示例代码

3. **低优先级** (Phase 3)
   - [ ] 删除旧的 `BillboardRenderSystem`
   - [ ] 优化 `QuadRenderSystem` (Texture2DArray等)

---

## 相关设计

这个解耦与之前提出的 [Texture2DArray + LRU系统](BillboardSystem_Design.md) 完全兼容：

- `QuadRenderSystem` 可以集成 `BillboardTextureArrayManager`
- `FacingTransformSystem` -> `FacingTransformSystem` (独立，不变)
- 最终效果：更高性能 + 更模块化的架构

---

**版本：** 1.0  
**建议时间：** 迭代后5-7天（完整Phase 1+2）  
**收益期望：** 减少耦合度。启用10+ 新使用场景。
