# SKILL: 快速参考和工作流清单

## 快速导航

需要快速完成某任务？选择对应SKILL：

| 任务 | 选择SKILL | 时间 |
|------|----------|------|
| 添加新系统类型（如SkySphere） | [SKILL_ADD_NEW_RENDER_COMPONENT.md](SKILL_ADD_NEW_RENDER_COMPONENT.md) | 20 min |
| 理解系统分组和启用机制 | [SKILL_SYSTEM_GROUPING_AND_ENABLEMENT.md](SKILL_SYSTEM_GROUPING_AND_ENABLEMENT.md) | 15 min |
| 选择ExecutionPhase和依赖关系 | [SKILL_EXECUTION_PHASE_ORDERING.md](SKILL_EXECUTION_PHASE_ORDERING.md) | 10 min |
| 创建自定义RenderGraph流程 | [SKILL_RENDERGRAPH_USAGE.md](SKILL_RENDERGRAPH_USAGE.md) | 15 min |

---

## 添加新Component/System的标准流程

### ✅ 完整Checklist

```
□ 1. 设计阶段 (5 min)
   □ 确定元素名称（如 "SkySphere"）
   □ 决定需要几个系统（1个还是多个？）
   □ 确定主要执行阶段（Collect/Batch/Submit/PostProcess）

□ 2. 创建Component (10 min)
   □ 创建 inc/hgl/ecs/components/{Element}Component.h
   □ 继承自Component，使用COMPONENT_HEADER宏
   □ 包含必要的可序列化字段

□ 3. 创建System (15 min)
   □ 创建 inc/hgl/ecs/systems/render/{Element}{Phase}System.h
   □ 创建 src/ecs/systems/render/{Element}{Phase}System.cpp
   □ ⭐ 在构造函数中调用 SetRenderElementType("Element")
   □ 实现 Update() 和 Render() 方法
   □ 声明依赖关系 AddDependency<>()

□ 4. 添加ExecutionPhase (5 min)
   □ 在 inc/hgl/ecs/core/System.h 中添加新Phase值
   □ 遵循命名约定

□ 5. 注册系统 (5 min)
   □ 在 src/ecs/core/DefaultSystemsCP.cpp 的Setup()中注册
   □ 使用 RegisterRenderSystem<MySystem>()

□ 6. 更新自适应检测 (可选, 5 min)
   □ 在 SceneStats 中添加 bool has{Element}
   □ 在 GatherSceneStats() 中检测该Component
   □ 在 CreateAdaptiveRenderGraph() 中启用/禁用

□ 7. 编译 (varies)
   □ cmake --build . --target ULRE.ECS

□ 8. 测试 (10 min)
   □ 创建测试场景添加Component
   □ 验证系统被启用
   □ 验证渲染结果正确
```

**总时间：60-90分钟**

---

## 代码模板速查

### 最小Component
```cpp
// inc/hgl/ecs/components/MyElementComponent.h
#include <hgl/ecs/core/Component.h>

namespace hgl::ecs {
    class MyElementComponent : public Component {
    public:
        COMPONENT_HEADER(MyElementComponent);
        // 自定义字段
    };
}
```

### 最小System
```cpp
// inc/hgl/ecs/systems/render/MyElementSystem.h
#include <hgl/ecs/core/System.h>

namespace hgl::ecs {
    class MyElementSystem : public System {
    public:
        explicit MyElementSystem(const std::string& name = "MyElementSystem");
        void Update(float deltaTime) override;
        void Render(graph::RenderCmdBuffer *cmd, float deltaTime) override;
    };
}

// src/ecs/systems/render/MyElementSystem.cpp
#include <hgl/ecs/systems/render/MyElementSystem.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/components/MyElementComponent.h>

namespace hgl::ecs {
    MyElementSystem::MyElementSystem(const std::string& name)
        : System(name) {
        SetSystemType(SystemType::RenderSubmit);
        SetExecutionPhase(ExecutionPhase::RenderPostProcess);  // 通用阶段
        SetRenderElementType("MyElement");  // ⭐ 关键
    }

    void MyElementSystem::Update(float deltaTime) {
        if (!context) return;
        std::vector<std::shared_ptr<MyElementComponent>> elements;
        context->GetComponents<MyElementComponent>(elements);
        // 处理逻辑
    }

    void MyElementSystem::Render(graph::RenderCmdBuffer *cmd, float deltaTime) {
        if (!context || !cmd) return;
        // GPU提交
    }
}
```

### 注册System
```cpp
// src/ecs/core/DefaultSystemsCP.cpp
void DefaultSystemsCP::Setup(ECSContext& context) {
    // ... 现有系统 ...
    context.RegisterRenderSystem<MyElementSystem>();
}
```

### 自动检测
```cpp
// src/ecs/core/RenderGraph.cpp - 在GatherSceneStats()中添加
std::vector<std::shared_ptr<MyElementComponent>> elements;
context->GetComponents<MyElementComponent>(elements);
stats.hasMyElement = !elements.empty();

// inc/hgl/ecs/core/RenderGraph.h
struct SceneStats {
    // ... 现有字段 ...
    bool hasMyElement = false;
};

// src/ecs/core/RenderGraph.cpp - 在CreateAdaptiveRenderGraph()中添加
context->SetElementTypeSystemsEnabled("MyElement", stats.hasMyElement);
```

---

## 常用API速查

### Context接口

```cpp
// 系统分组
context->SetElementTypeSystemsEnabled("ElementType", true/false);
std::vector<std::shared_ptr<System>> systems;
context->GetSystemsByElementType("ElementType", systems);

// 系统管理
context->RegisterRenderSystem<MySystem>();
auto sys = context->GetSystem<MySystem>();

// 渲染
context->Render(deltaTime);  // 使用自适应图
context->Render(deltaTime, custom_graph);
context->Render(deltaTime, graph, [](RenderCmdBuffer* cmd) {
    // 回调
});
```

### System接口

```cpp
// 声明和查询
system->SetRenderElementType("ElementType");
const std::string& type = system->GetRenderElementType();

// 状态
system->SetEnabled(true/false);
bool enabled = system->IsEnabled();

// 执行阶段与依赖
system->SetExecutionPhase(ExecutionPhase::RenderCollect);  // 通用阶段
system->AddDependency<OtherSystem>();  // 显式依赖关系
```

### 调试

```cpp
// 查看系统组
context->GetSystemsByElementType("Primitive", systems);
for (auto& sys : systems) {
    printf("%s: enabled=%d\n", sys->GetName().c_str(), sys->IsEnabled());
}

// 查看RenderGraph
const auto& passes = graph.GetPasses();
printf("Pass count: %zu\n", passes.size());
```

---

## 常见错误及解决

| 错误 | 原因 | 解决 |
|------|------|------|
| System未执行 | 未设置SetRenderElementType() 或未启用 | 添加SetRenderElementType()调用 |
| 手动注册失败 | System未在DefaultSystemsCP中注册 | 添加 RegisterRenderSystem<>() |
| 自动检测不工作 | 未在GatherSceneStats()中添加 | 补充检测逻辑 |
| 执行顺序错误 | dependency关系不对 | 检查AddDependency<>() |
| 编译出错 | 新ExecutionPhase值冲突 | 检查System.h中的Phase编号 |

---

## 多系统情况示例：Particle

```cpp
// 1. Component
class ParticleComponent : public Component {
    COMPONENT_HEADER(ParticleComponent);
    // ...
};

// 2. 创建 PipelineGroup（推荐新架构）
class ParticleRenderPipelineGroup {
public:
    void Initialize(ECSContext* ctx) {
        auto collect_system = EnsureRenderSystem<ParticleCollectSystem>(ctx);
        collect_system->SetRenderElementType("Particle");
        collect_system->SetExecutionPhase(ExecutionPhase::RenderCollect);

        auto simulation_system = EnsureRenderSystem<ParticleSimulationSystem>(ctx);
        simulation_system->SetRenderElementType("Particle");
        simulation_system->SetExecutionPhase(ExecutionPhase::RenderBatch);
        simulation_system->AddDependency<ParticleCollectSystem>();

        auto submit_system = EnsureRenderSystem<ParticleSubmitSystem>(ctx);
        submit_system->SetRenderElementType("Particle");
        submit_system->SetExecutionPhase(ExecutionPhase::RenderDrawSubmit);
        submit_system->AddDependency<ParticleSimulationSystem>();
    }
};

// 3. 注册 Installer
bool InstallParticleGroup(ECSContext* ctx, IRenderTarget* /*default_rt*/) {
    ParticleRenderPipelineGroup group;
    group.Initialize(ctx);
    return true;
}

void RegisterParticleGroup() {
    auto& registry = SystemGroupRegistry::Get();
    registry.RegisterGroupInstaller("Particle", InstallParticleGroup);
}

// 4. 应用代码调用
EnsureSystemGroupSystems(context, "Particle", default_rt);
```

// 7. 自动检测
// 在GatherSceneStats()中
std::vector<std::shared_ptr<ParticleComponent>> particles;
context->GetComponents<ParticleComponent>(particles);
stats.hasParticles = !particles.empty();

// 在CreateAdaptiveRenderGraph()中
context->SetElementTypeSystemsEnabled("Particle", stats.hasParticles);
```

---

## 质量预设示例

```cpp
void SetQualityPreset(ECSContext* context, QualityLevel level) {
    const bool enable_high = (level >= QualityLevel::High);
    const bool enable_ultra = (level == QualityLevel::Ultra);
    
    // 基础
    context->SetElementTypeSystemsEnabled("Primitive", true);
    context->SetElementTypeSystemsEnabled("Text", true);
    
    // 高端特性
    context->SetElementTypeSystemsEnabled("Line", enable_high);
    context->SetElementTypeSystemsEnabled("Particle", enable_high);
    context->SetElementTypeSystemsEnabled("Decal", enable_high);
    context->SetElementTypeSystemsEnabled("DetailGrass", enable_ultra);
}

// 应用预设
SetQualityPreset(context, QualityLevel::High);
context->InvalidateAdaptiveRenderGraph();  // 重新生成
```

---

## 文件位置快查

```
新增文件应该创建在：
├── inc/hgl/ecs/components/
│   └── {Element}Component.h
├── inc/hgl/ecs/systems/render/
│   └── {Element}{Phase}System.h
└── src/ecs/systems/render/
    └── {Element}{Phase}System.cpp

修改现有文件：
├── inc/hgl/ecs/core/System.h
│   └── 添加新ExecutionPhase值
├── src/ecs/core/DefaultSystemsCP.cpp
│   └── 在Setup()中注册
├── inc/hgl/ecs/core/RenderGraph.h
│   └── SceneStats添加bool字段 (可选)
├── src/ecs/core/RenderGraph.cpp
│   ├── GatherSceneStats()添加检测 (可选)
│   └── CreateAdaptiveRenderGraph()添加启用 (可选)
```

---

## 验证步骤

新系统完成后，逐个验证：

```bash
# 1. 编译成功
cd e:\ULRE\build
cmake --build . --config Release --target ULRE.ECS

# 2. 系统被注册（添加调试输出）
printf("System registered: %s (element_type=%s)\n", 
       system->GetName().c_str(), 
       system->GetRenderElementType().c_str());

# 3. 系统在运行时被启用/禁用
if (has_component)
    context->SetElementTypeSystemsEnabled("MyElement", true);
// 检查：GetSystemsByElementType()返回非空，IsEnabled()=true

# 4. 渲染结果正确
// 创建包含该Component的场景，检查视觉结果
```

---

## 下一步：高级主题

完成基础后可进阶学习：

- **多RT渲染** - 延迟渲染/阴影贴图
- **质量预设配置文件** - 从JSON/YAML加载预设
- **动态性能调整** - 根据帧率自动降级
- **ExecutionPhase简化计划** - 未来规划

---

## 相关资源索引

所有SKILL文档：
- `SKILL_ADD_NEW_RENDER_COMPONENT.md` - 详细流程
- `SKILL_SYSTEM_GROUPING_AND_ENABLEMENT.md` - API和模式
- `SKILL_EXECUTION_PHASE_ORDERING.md` - 执行阶段设计
- `SKILL_RENDERGRAPH_USAGE.md` - 图配置和预设

源码参考：
- `inc/hgl/ecs/core/Component.h` - Component基类
- `inc/hgl/ecs/core/System.h` - System基类和ExecutionPhase
- `inc/hgl/ecs/core/Context.h` - ECSContext API
- `inc/hgl/ecs/core/RenderGraph.h` - RenderGraph数据结构
- `src/ecs/core/RenderGraph.cpp` - RenderGraph实现
