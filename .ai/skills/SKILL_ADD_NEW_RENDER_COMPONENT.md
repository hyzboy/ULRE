# SKILL: 添加新的RenderComponent和对应系统

## 目标
为游戏添加新的渲染元素类型（如SkySphere、Particle、Decal、Terrain等），并自动集成到自适应RenderGraph中。

## 前置知识
- Component系统基础：继承自`hgl::ecs::Component`
- System执行顺序由`ExecutionPhase`定义，通过`SetRenderElementType()`分组
- RenderGraph通过`context->SetElementTypeSystemsEnabled(elementType, hasComponent)`启用/禁用系统组

## 工作流程

### 1️⃣ **创建Component类** 
```cpp
// inc/hgl/ecs/components/MyElementComponent.h
#pragma once
#include<hgl/ecs/core/Component.h>

namespace hgl::ecs {
    class MyElementComponent : public Component {
    public:
        COMPONENT_HEADER(MyElementComponent);
        
        // 自定义数据
        int quality_level = 0;
        // ... 其他属性
    };
} // namespace hgl::ecs
```

**CheckList:**
- [ ] 类名遵循 `{ElementType}Component` 命名
- [ ] 继承自 `Component`
- [ ] 使用 `COMPONENT_HEADER` 宏注册
- [ ] 包含必要的可序列化字段

---

### 2️⃣ **创建对应的System类**

创建**至少一个**核心System（多数情况需要多个）：

```cpp
// inc/hgl/ecs/systems/render/MyElementRenderSystem.h
#pragma once
#include<hgl/ecs/core/System.h>

namespace hgl::ecs {
    class MyElementRenderSystem : public System {
    private:
        std::shared_ptr<MyElementPipeline> pipeline;
        
    public:
        explicit MyElementRenderSystem(const std::string& name = "MyElementRenderSystem");
        
        void Update(float deltaTime) override;
        void Render(graph::RenderCmdBuffer *cmd, float deltaTime) override;
    };
}
```

```cpp
// src/ecs/systems/render/MyElementRenderSystem.cpp
#include<hgl/ecs/systems/render/MyElementRenderSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/MyElementComponent.h>

namespace hgl::ecs {
    MyElementRenderSystem::MyElementRenderSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::RenderSubmit);  // 选择合适的SystemType
        SetExecutionOrder(ExecutionPhase::RenderPostProcess_LineRenderSystem);  // 选择合适的执行阶段
        SetRenderElementType("MyElement");  // ⭐ 关键：声明element type
    }

    void MyElementRenderSystem::Update(float deltaTime) {
        if (!context) return;
        
        std::vector<std::shared_ptr<MyElementComponent>> elements;
        context->GetComponents<MyElementComponent>(elements);
        
        // 处理收集/更新逻辑
        for (auto& elem : elements) {
            if (!elem->IsEnabled()) continue;
            // TODO: 更新逻辑
        }
    }

    void MyElementRenderSystem::Render(graph::RenderCmdBuffer *cmd, float deltaTime) {
        if (!context || !cmd) return;
        
        // TODO: 渲染提交逻辑
    }
}
```

**CheckList:**
- [ ] System名称遵循 `{ElementType}{Phase}System` 命名（如 `SkySphereRenderSystem`）
- [ ] 在构造函数中调用 `SetRenderElementType("MyElement")`
- [ ] 选择合适的 `ExecutionPhase`（通常在 `RenderCollect_*`, `RenderBatch_*`, `RenderDrawSubmit_*`, `RenderPostProcess_*` 范围）
- [ ] 在 `Update()` 中实现收集/处理逻辑
- [ ] 在 `Render()` 中实现GPU命令提交

---

### 3️⃣ **多系统情况：创建系统组**

如果需要多个系统（如Primitive有6个），应分为阶段：

```cpp
// 收集阶段
class MyElementCollectSystem : public System {
    // SetRenderElementType("MyElement")
    // SetExecutionOrder(RenderCollect_MyElementCollectSystem)
};

// 处理/排序阶段  
class MyElementSortSystem : public System {
    // SetRenderElementType("MyElement")
    // SetExecutionOrder(RenderBatch_MyElementSortSystem)
    // AddDependency<MyElementCollectSystem>()
};

// 提交阶段
class MyElementSubmitSystem : public System {
    // SetRenderElementType("MyElement")
    // SetExecutionOrder(RenderDrawSubmit_MyElementSubmitSystem)
    // AddDependency<MyElementSortSystem>()
};
```

**CheckList:**
- [ ] 所有相关系统使用 **相同的** `SetRenderElementType("MyElement")`
- [ ] 系统之间通过 `AddDependency<>()` 建立执行顺序
- [ ] 使用合适的 `ExecutionPhase` 前缀（Collect→Sort→Finalize→Submit）

---

### 4️⃣ **注册系统到Context**

在 `DefaultSystemsCP::Setup()` 中注册：

```cpp
// src/ecs/core/DefaultSystemsCP.cpp
void DefaultSystemsCP::Setup(ECSContext& context) {
    // ... 现有系统 ...
    
    // 新增系统组
    context.RegisterRenderSystem<MyElementCollectSystem>();
    context.RegisterRenderSystem<MyElementSortSystem>();
    context.RegisterRenderSystem<MyElementSubmitSystem>();
}
```

**CheckList:**
- [ ] 使用 `RegisterRenderSystem<T>()` 注册render系统
- [ ] 如果是tick系统，使用 `RegisterTickSystem<T>()`
- [ ] 系统按执行顺序注册（或依赖自动排序）

---

### 5️⃣ **更新GatherSceneStats**（如需要自动检测）

在 `src/ecs/core/RenderGraph.cpp` 中添加检测：

```cpp
SceneStats GatherSceneStats(ECSContext* context) {
    SceneStats stats;
    // ... 现有检测 ...
    
    // 新增检测
    std::vector<std::shared_ptr<MyElementComponent>> elements;
    context->GetComponents<MyElementComponent>(elements);
    stats.hasMyElement = !elements.empty();
    
    return stats;
}
```

然后在 `SceneStats` 结构中添加字段，`CreateAdaptiveRenderGraph()` 中添加启用/禁用：

```cpp
// inc/hgl/ecs/core/RenderGraph.h
struct SceneStats {
    bool hasPrimitives = false;
    bool hasText = false;
    bool hasLines = false;
    bool hasBillboards = false;
    bool hasMyElement = false;  // ✅ 新增
};

// src/ecs/core/RenderGraph.cpp
RenderGraph CreateAdaptiveRenderGraph(ECSContext* context) {
    // ...
    context->SetElementTypeSystemsEnabled("MyElement", stats.hasMyElement);
    // ...
}
```

**CheckList:**
- [ ] 在 `SceneStats` 中添加对应字段
- [ ] 在 `GatherSceneStats()` 中实现检测逻辑
- [ ] 在 `CreateAdaptiveRenderGraph()` 中添加启用/禁用调用
- [ ] （可选）添加诊断日志

---

## 📝 示例：添加SkySphere元素

```cpp
// inc/hgl/ecs/components/SkySphereComponent.h
class SkySphereComponent : public Component {
    COMPONENT_HEADER(SkySphereComponent);
public:
    enum class Quality { Low, High, Ultra };
    Quality quality = Quality::High;
};

// inc/hgl/ecs/systems/render/SkySphereRenderSystem.h
class SkySphereRenderSystem : public System {
public:
    SkySphereRenderSystem(const std::string& name = "SkySphereRenderSystem");
    void Update(float deltaTime) override;
    void Render(graph::RenderCmdBuffer *cmd, float deltaTime) override;
private:
    std::shared_ptr<SkySphereRenderer> renderer;
};

// src/ecs/systems/render/SkySphereRenderSystem.cpp
SkySphereRenderSystem::SkySphereRenderSystem(const std::string& name)
    : System(name) {
    SetSystemType(SystemType::RenderSubmit);
    SetExecutionOrder(ExecutionPhase::RenderPostProcess_LineRenderSystem);  // 天空球在后处理
    SetRenderElementType("SkySphere");  // ⭐ 关键
}
```

---

## 🔍 验证清单

完成后检查：

- [ ] Component类编译无误
- [ ] System类编译无误  
- [ ] 所有System都调用了 `SetRenderElementType()`
- [ ] 在 `DefaultSystemsCP::Setup()` 中注册
- [ ] （如需要）更新 `SceneStats` 和 `GatherSceneStats()`
- [ ] 编译全项目无错误
- [ ] 试验场景：创建该Component，验证对应系统被启用

---

## 常见问题

**Q: 我的系统需要多个阶段吗？**  
A: 看情况。简单元素一个System足够。复杂元素（如Primitive）需要Collect→Cull→Sort→Batch→Finalize→Submit六个阶段。

**Q: SetRenderElementType()设什么名字？**  
A: 使用驼峰命名的元素名（如"SkySphere", "Particle", "Terrain"）。保持一致，因为 `SetElementTypeSystemsEnabled()` 按字符串匹配。

**Q: 需要添加ExecutionPhase枚举值吗？**  
A: 需要。在 `System.h` 中的 `ExecutionPhase enum` 中添加。建议遵循 `RenderPhase_SystemName` 的命名约定。

**Q: 如何测试新系统是否工作？**  
A: 
1. 创建测试场景，添加对应Component
2. 运行 `CreateAdaptiveRenderGraph(context)`
3. 检查系统是否被启用：`GetSystemsByElementType("MyElement")` 应返回系统列表
4. 检查日志："Enabling MyElement system group"

---

## 参考资源

- `inc/hgl/ecs/components/PrimitiveComponent.h` - Component示例
- `src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp` - 多系统示例
- `src/ecs/core/RenderGraph.cpp` - 自适应图实现
- `inc/hgl/ecs/core/System.h` - ExecutionPhase定义
