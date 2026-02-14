# 渲染架构迁移快速开始指南

## 🚀 5分钟快速理解

### 旧架构的问题

```cpp
// ❌ 旧方式：隐晦的 API，隐藏真实依赖
class WorkObject {
    #define FUNC_FROM_RENDER_FRAMEWORK(return_type, func_name) \
        template<typename ...ARGS> \
        return_type func_name(ARGS...args) { \
            return render_framework ? render_framework->func_name(args...) : nullptr; \
        }
    
    // 看起来是 WorkObject 的方法，实际上是代理
    FUNC_FROM_RENDER_FRAMEWORK(Material*, CreateMaterial);
    FUNC_FROM_RENDER_FRAMEWORK(DeviceBuffer*, CreateUBO);
};

// 问题：
// 1. 依赖关系不清晰
// 2. 难以单元测试
// 3. 难以追踪数据流
// 4. 难以替换实现
```

### 新架构的优势

```cpp
// ✅ 新方式：清晰的接口，显式的依赖
class WorkObject {
    RenderContext* render_context;      // 显式依赖
    std::unique_ptr<RenderAPI> api;     // 便捷门面
    
public:
    // 清晰的接口
    RenderAPI* GetRenderAPI() { return api.get(); }
    RenderContext* GetRenderContext() { return render_context; }
    ECSContext* GetECSContext() { return ecs_context; }
};

// 优点：
// 1. 依赖显式清晰
// 2. 易于单元测试和 Mock
// 3. 易于追踪数据流和错误
// 4. 易于扩展和定制
```

---

## 📖 三层 API 架构

```
┌─────────────────────────────────────────────┐
│    Level 1: RenderAPI (最简洁)              │
│  - 常用操作的简便方法                        │
│  - 推荐用于业务代码                         │
│  - 隐藏管理器细节                           │
├─────────────────────────────────────────────┤
│    Level 2: RenderContext (标准)             │
│  - 完整的资源创建接口                        │
│  - 推荐用于需要控制的代码                    │
│  - 直接映射到 Manager                       │
├─────────────────────────────────────────────┤
│    Level 3: Manager (最底层)                 │
│  - 完全的底层控制                           │
│  - 仅在需要特殊处理时使用                    │
│  - 需要理解各 Manager 的具体实现             │
└─────────────────────────────────────────────┘
```

---

## 💡 使用示例

### 示例 1: 基础资源创建

#### 旧方式
```cpp
class MyScene : public WorkObject {
    void Init() override {
        // 通过宏隐藏的委托，不明显
        Material* mtl = CreateMaterial("diffuse");
        DeviceBuffer* ubo = CreateUBO("camera", sizeof(CameraData));
        Texture2D* tex = LoadTexture2D("woodTexture.png");
    }
};
```

#### 新方式（推荐）
```cpp
class MyScene : public WorkObject {
    void Init() override {
        // 清晰的接口，显式依赖
        auto api = GetRenderAPI();
        
        // 强类型 UBO
        Material* mtl = api->CreateMaterial("diffuse");
        DeviceBuffer* ubo = api->CreateUBO<CameraData>("camera");
        Texture2D* tex = api->LoadTexture2D("woodTexture.png");
    }
};
```

### 示例 2: 材质实例创建

#### 旧方式
```cpp
class MyScene : public WorkObject {
    void CreateRenderObject() {
        Material* base_mtl = CreateMaterial("pbr");
        MaterialInstance* instance = CreateMaterialInstance(base_mtl);
        instance->SetFloat("roughness", 0.5f);
    }
};
```

#### 新方式
```cpp
class MyScene : public WorkObject {
    void CreateRenderObject() {
        auto api = GetRenderAPI();
        
        Material* base_mtl = api->CreateMaterial("pbr");
        MaterialInstance* instance = api->CreateMaterialInstance(base_mtl);
        
        // 可选：如果需要更底层的控制
        auto mat_mgr = api->GetContext()->GetMaterialManager();
        // ... 进行高级操作
    }
};
```

### 示例 3: 纹理加载

#### 旧方式
```cpp
class MyScene : public WorkObject {
    void LoadAssets() {
        // 不知道这会通过哪个 Manager 处理
        Texture2D* color = LoadTexture2D("color.png");
        Texture2D* normal = LoadTexture2D("normal.png");
        TextureCube* skybox = LoadTextureCube("skybox/");
    }
};
```

#### 新方式
```cpp
class MyScene : public WorkObject {
    void LoadAssets() {
        auto api = GetRenderAPI();
        
        // 操作清晰明确
        Texture2D* color = api->LoadTexture2D("color.png", true);    // 自动MipMap
        Texture2D* normal = api->LoadTexture2D("normal.png", false); // 不生成MipMap
        TextureCube* skybox = api->LoadTextureCube("skybox/");       // 立方体纹理
    }
};
```

### 示例 4: ECS 集成

#### 旧方式（混乱）
```cpp
class MyScene : public WorkObject {
    void SetupScene() {
        auto ecs = GetECSContext();          // 怎么得到的？
        
        // 创建实体
        auto entity = ecs->CreateEntity("player");
        
        // 创建渲染资源时需要回到 WorkObject
        Material* mtl = CreateMaterial(...); // 又要用 WorkObject
        
        // 数据流不清晰
    }
};
```

#### 新方式（清晰）
```cpp
class MyScene : public WorkObject {
    void SetupScene() {
        // 三层接口，各施其职
        auto api = GetRenderAPI();          // 资源创建
        auto ctx = GetRenderContext();      // 底层控制
        auto ecs = GetECSContext();         // ECS 逻辑
        
        // 清晰的责任划分
        
        // 1. 创建ECS实体
        auto entity = ecs->CreateEntity("player");
        
        // 2. 添加ECS组件
        auto prim_comp = entity->AddComponent<PrimitiveComponent>();
        auto transform = entity->AddComponent<TransformComponent>();
        
        // 3. 创建渲染资源（独立的关心）
        Material* mtl = api->CreateMaterial("player_shader");
        MaterialInstance* instance = api->CreateMaterialInstance(mtl);
        
        // 4. 关联资源和组件
        prim_comp->SetMaterialInstance(instance);
        
        // 数据流清晰！
    }
};
```

---

## 🔄 迁移步骤

### 步骤 1: 更新包含文件

```cpp
// 旧
#include<hgl/graph/render/RenderFramework.h>
#include<hgl/WorkObject.h>

// 新（添加）
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/render/RenderAPI.h>
```

### 步骤 2: 替换 WorkObject 方法调用

```cpp
// 旧代码
class OldScene : public WorkObject {
    void Init() override {
        Material* m = CreateMaterial("shader");
        auto ubo = CreateUBO("ubo", 256);
    }
};

// 新代码（最小改动）
class NewScene : public WorkObject {
    void Init() override {
        auto api = GetRenderAPI();
        Material* m = api->CreateMaterial("shader");
        auto ubo = api->CreateUBO("ubo", 256);
    }
};
```

### 步骤 3: 使用强类型 API

```cpp
// 旧
auto ubo = CreateUBO("camera", sizeof(CameraData));  // 易出错

// 新（强类型）
auto ubo = GetRenderAPI()->CreateUBO<CameraData>("camera");  // 类型安全
```

### 步骤 4: 分离 ECS 和渲染逻辑

```cpp
// 旧（混乱）
class Scene : public WorkObject {
    void SetupScene() {
        // ECS
        auto ecs = GetECSContext();
        auto entity = ecs->CreateEntity();
        
        // 渲染资源（突然切换到WorkObject）
        auto mat = CreateMaterial(...);
        
        // 再回到 ECS
        entity->AddComponent<...>();
    }
};

// 新（清晰）
class Scene : public WorkObject {
    void SetupScene() {
        // 第一部分：创建资源
        auto api = GetRenderAPI();
        auto material = api->CreateMaterial("shader");
        auto instance = api->CreateMaterialInstance(material);
        
        // 第二部分：设置 ECS
        auto ecs = GetECSContext();
        auto entity = ecs->CreateEntity("myObject");
        auto prim_comp = entity->AddComponent<PrimitiveComponent>();
        prim_comp->SetMaterialInstance(instance);
    }
};
```

---

## 🎯 常见迁移模式

### 模式 1: 简单资源创建

```cpp
// 检查清单
□ 将 GetRenderFramework() 改为 GetRenderAPI()
□ 将方法调用改为 api->MethodName()
□ 强类型模板化 CreateUBO/CreateSSBO
```

### 模式 2: 复杂场景设置

```cpp
// 检查清单
□ 分离资源创建和 ECS 逻辑
□ 资源创建使用 RenderAPI
□ ECS 逻辑使用 GetECSContext()
□ 需要底层控制时使用 GetRenderContext()
```

### 模式 3: 自定义 Manager 访问

```cpp
// 检查清单
□ 优先使用 RenderAPI 的高级方法
□ 如需要，使用 RenderContext 访问 Manager
□ 仅在特殊情况下直接使用 Manager API
```

---

## ⏱️ 预计迁移时间

| 任务 | 时间 |
|-----|-----|
| 理解新架构 | 15 分钟 |
| 更新一个简单场景类 | 20 分钟 |
| 更新一个复杂场景类 | 1-2 小时 |
| 单元测试 | 因人而异 |
| 全项目迁移（假设 10 个场景）| 1-2 周 |

---

## 🆘 故障排除

### 问题 1: 编译错误 "Member of RenderAPI is undefined"

**原因:** 未包含 `RenderAPI.h`

**解决:**
```cpp
#include<hgl/graph/render/RenderAPI.h>
```

### 问题 2: WorkObject 中 GetRenderAPI() 返回 nullptr

**原因:** RenderFramework 未正确初始化 RenderContext

**解决:** 检查 RenderFramework 的 Init 方法是否完整

### 问题 3: 类型不匹配 "cannot convert DeviceBuffer* to T*"

**原因:** CreateUBO 返回的是通用 DeviceBuffer，不是特定类型

**解决:** 使用强类型模板
```cpp
// 错误
auto ubo = api->CreateUBO("ubo", sizeof(MyData));
MyData* data = (MyData*)ubo;  // 不安全

// 正确
auto ubo = api->CreateUBO<MyData>("ubo");
```

### 问题 4: 在 ECS 系统中无法访问渲染资源

**原因:** ECS 系统还未集成 RenderContext

**解决:** 在 System 中使用 GetWorld()->GetRenderContext()

```cpp
class MyRenderSystem : public System {
    void Update(float dt) override {
        auto ctx = GetWorld()->GetRenderContext();
        if (ctx) {
            // 现在可以创建资源了
        }
    }
};
```

---

## 📚 相关资源

### 文档
- [完整迁移计划](./ArchitectureMigrationPlan.md)
- [RenderContext API 参考](./inc/hgl/graph/render/RenderContext.h)
- [RenderAPI API 参考](./inc/hgl/graph/render/RenderAPI.h)
- [ECS 系统集成指南](#)

### 示例代码
- 基础场景示例: `examples/RenderAPIBasic.cpp`
- ECS 集成示例: `examples/ECSRenderIntegration.cpp`
- 复杂场景示例: `examples/ComplexSceneSetup.cpp`

### 贡献和反馈
- 反馈新 API 的易用性问题
- 分享迁移过程中的最佳实践
- 报告遗漏的 API 或文档

---

## ✅ 验证迁移成功

```cpp
// 迁移清单
□ 所有 WorkObject 子类已迁移
□ 编译时无 deprecation 警告（或警告已理解）
□ 单元测试全部通过
□ 性能基准测试无回退
□ 代码审查完成
□ 文档已更新

// 代码质量指标
□ 没有循环依赖
□ 依赖方向清晰（单向）
□ 95%+ 新代码使用 RenderAPI / RenderContext
□ 旧宏相关代码 < 5%
□ 代码注释覆盖率 > 80%
```

---

## 🎓 进阶主题

### 主题 1: 为 RenderAPI 添加自定义方法

```cpp
// 扩展 RenderAPI
class MyRenderAPI : public RenderAPI {
    Texture2D* LoadTexture2DWithLOD(const OSString& path, 
                                   uint32_t max_lod) {
        // 自定义逻辑
        auto tex = LoadTexture2D(path);
        // ... 配置 LOD
        return tex;
    }
};
```

### 主题 2: 多渲染目标支持

```cpp
// 未来版本中支持
auto ctx = GetRenderContext();
ctx->SetCurrentRenderTarget(shadow_map_target);
// ... 渲染阴影
ctx->SetCurrentRenderTarget(main_target);
// ... 主渲染
```

### 主题 3: 自定义资源工厂

```cpp
class CustomMaterialFactory {
    RenderAPI* api;
    
    Material* CreateAdvancedPBR() {
        auto mtl = api->CreateMaterial("pbr_advanced");
        // ... 配置
        return mtl;
    }
};
```

---

## 📞 Get Help

- 💬 Discussion Forum: [link]
- 🐛 Issue Tracker: [link]
- 📧 Email: [email]
- 👥 Community: [link]
