# CameraComponent 实现总结

## 实现内容

### 1. 头文件 (`inc/hgl/ecs/CameraComponent.h`)

实现了 `CameraComponent` 类，继承自 `hgl::ecs::Component`，包含：

**核心成员：**
- `graph::CameraInfo camera_info` - 摄像机数据（投影矩阵、视图矩阵等）
- `std::unique_ptr<CameraController> controller` - 可插拔的控制策略

**主要接口：**
- `GetCameraInfo()` / `SetCameraInfo()` - 访问摄像机数据
- `SetController()` - 设置控制器（支持运行时切换）
- `GetController()` - 获取当前控制器
- `RemoveController()` - 移除控制器
- `OnAttach()` / `OnUpdate()` / `OnDetach()` - 组件生命周期

### 2. 实现文件 (`src/ecs/CameraComponent.cpp`)

**控制器管理：**
- `SetController()` - 安全切换控制器（先Shutdown旧的，再Initialize新的）
- `RemoveController()` - 清理控制器资源
- `OnUpdate()` - 每帧调用控制器的Update方法

**生命周期处理：**
- `OnAttach()` - 组件附加到实体时初始化控制器
- `OnDetach()` - 组件分离时清理控制器

### 3. 构建系统 (`src/ecs/CMakeLists.txt`)

更新了CMake配置：
- 添加了 `ECS_CAMERA_HEADERS` 和 `ECS_CAMERA_SOURCE`
- 将 `CameraComponent.cpp` 加入编译
- 设置了 Visual Studio 源文件分组（Camera\\Public 和 Camera\\Private）

### 4. 示例代码 (`src/ecs/test_camera_component.cpp`)

提供了完整的使用示例：
- **FPSCameraController** - 第一人称相机控制器示例
- **OrbitCameraController** - 轨道相机控制器示例
- 演示了控制器的创建、切换和生命周期管理
- 展示了如何访问和使用 CameraInfo

### 5. 文档 (`inc/hgl/ecs/CameraComponent.md`)

详细的使用文档，包含：
- 设计理念和架构说明
- 核心类的API文档
- 多个实际使用场景的代码示例
- 最佳实践和注意事项

## 设计特点

### 1. 策略模式（Strategy Pattern）

```
CameraComponent
    ├── CameraInfo (数据)
    └── CameraController (策略，可运行时替换)
```

**优势：**
- 数据与逻辑分离
- 运行时灵活切换控制方式
- 易于扩展新的控制器类型
- 控制器切换不影响摄像机状态

### 2. 简洁的ECS设计

遵循新的ECS框架风格：
- 单一类，无需分离的 Data 和 Manager
- 直接继承 `Component` 基类
- 使用标准库（`std::unique_ptr`）
- 清晰的生命周期管理

### 3. 可扩展性

用户可以轻松创建自定义控制器：

```cpp
class MyCustomController : public CameraController
{
public:
    void Update(float deltaTime) override 
    {
        // 自定义控制逻辑
    }
    
    const std::string& GetControllerName() const override 
    {
        return name;
    }
};

// 使用
camera->SetController(std::make_unique<MyCustomController>());
```

## 使用流程

### 基本用法

```cpp
// 1. 创建实体
auto cameraEntity = context->CreateEntity<Entity>("MainCamera");

// 2. 添加Transform（可选但推荐）
auto transform = cameraEntity->AddComponent<TransformComponent>();
transform->SetLocalPosition(glm::vec3(0, 5, 10));

// 3. 添加CameraComponent
auto camera = cameraEntity->AddComponent<CameraComponent>();

// 4. 设置控制器
camera->SetController(std::make_unique<FPSCameraController>());

// 5. 系统自动调用Update（通过Context::Tick）
context->Tick(deltaTime);

// 6. 访问数据
auto& info = camera->GetCameraInfo();
glm::vec3 pos = info.pos;
```

### 控制器切换

```cpp
// 从FPS切换到轨道相机
camera->SetController(std::make_unique<OrbitCameraController>());

// 从轨道相机切换回FPS
camera->SetController(std::make_unique<FPSCameraController>());

// 移除控制器（摄像机静止）
camera->RemoveController();
```

## 常见应用场景

1. **游戏主摄像机**
   - FPS控制器（第一人称）
   - TPS控制器（第三人称）
   - 轨道相机（查看角色/物体）

2. **编辑器摄像机**
   - 自由飞行控制
   - 多模式切换（平移/旋转/缩放）
   - 焦点跟踪

3. **过场动画摄像机**
   - 关键帧动画
   - 路径跟随
   - 平滑插值

4. **调试摄像机**
   - 快速移动
   - 坐标锁定
   - 视角保存/恢复

## 技术亮点

1. **RAII资源管理**
   - 使用 `std::unique_ptr` 自动管理控制器生命周期
   - 无需手动 delete，防止内存泄漏

2. **生命周期钩子**
   - `Initialize()` / `Shutdown()` 确保资源正确初始化和清理
   - `OnAttach()` / `OnDetach()` 处理组件级生命周期

3. **类型安全**
   - 使用 C++17 标准库
   - 编译时类型检查
   - 无需类型转换

4. **性能友好**
   - 轻量级设计
   - 每帧只调用一次 Update
   - 无额外虚函数调用开销

## 编译验证

✅ **编译成功** - 所有代码已通过编译验证

生成的库文件：`ULRE.ECS.lib` / `ULRE.ECS.a`

## 文件清单

```
inc/hgl/ecs/
├── CameraComponent.h        (头文件，108行)
├── CameraComponent.md       (文档，600+行)
└── CameraController.h       (已存在，控制器基类)

src/ecs/
├── CameraComponent.cpp      (实现，60行)
├── test_camera_component.cpp (示例，200+行)
└── CMakeLists.txt           (更新，添加CameraComponent编译)
```

## 下一步建议

### 可选的增强功能

1. **预置控制器库**
   - FPSController
   - OrbitController
   - EditorController
   - CinematicController

2. **相机系统（System）**
   ```cpp
   class CameraSystem : public System
   {
   public:
       void Update(float deltaTime) override
       {
           // 批量更新所有CameraComponent
       }
   };
   ```

3. **多摄像机管理**
   ```cpp
   class CameraManager
   {
       std::vector<std::shared_ptr<CameraComponent>> cameras;
       std::shared_ptr<CameraComponent> activeCamera;
       
       void SetActive(int index);
       void BlendBetween(int from, int to, float t);
   };
   ```

4. **相机效果**
   - 摇晃效果（Shake）
   - 平滑跟随（SmoothFollow）
   - 视角抖动（ScreenShake）
   - 景深效果（DOF）

## 总结

✅ **完整实现** - CameraComponent的所有核心功能均已实现  
✅ **编译通过** - 代码已通过编译验证  
✅ **文档齐全** - 提供了详细的API文档和使用示例  
✅ **易于使用** - 简洁的API，符合ECS框架设计理念  
✅ **可扩展** - 策略模式允许轻松添加新的控制器类型  

CameraComponent现在可以投入使用了！🎉
