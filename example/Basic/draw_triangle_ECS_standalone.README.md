# Standalone ECS Triangle Rendering Example

## 简介 (Introduction)

本范例演示一个**完全独立**的ECS渲染程序，不依赖WorkObject/WorkManager/RenderFramework体系。这是一个教学性的概念示例，展示如何直接使用Vulkan和ECS系统。

This example demonstrates a **completely standalone** ECS rendering program without dependencies on WorkObject/WorkManager/RenderFramework. This is a pedagogical conceptual example showing how to directly use Vulkan with the ECS system.

## 主要特点 (Key Features)

### 1. 无框架依赖 (No Framework Dependencies)
- **不使用 WorkObject/WorkManager**: 直接实现应用类
- **不使用 RunFramework**: 手动实现main函数和渲染循环
- **不使用 SceneRenderer**: 直接使用ECS RenderCollector

### 2. 手动Vulkan管理 (Manual Vulkan Management)
- 展示Vulkan初始化的完整流程
- 手动创建Device、Swapchain、RenderPass
- 手动管理CommandBuffer和渲染循环

### 3. 纯ECS渲染 (Pure ECS Rendering)
- 使用ECSContext管理Entity
- 使用RenderCollector收集和渲染
- 完全基于ECS架构的渲染管线

## 代码结构 (Code Structure)

```cpp
class StandaloneECSApp
{
private:
    // Vulkan核心对象（手动管理）
    VulkanDevice* device;
    Swapchain* swapchain;
    RenderPass* render_pass;
    CommandBuffer* cmd_buffer;
    
    // 渲染资源
    MaterialInstance* material_instance;
    Primitive* prim_triangle;
    Pipeline* pipeline;
    
    // ECS组件
    std::shared_ptr<ECSContext> ecs_world;
    std::shared_ptr<Entity> triangle_entity;
    RenderCollector* render_collector;
    
public:
    bool Initialize();  // 初始化所有组件
    void Run();         // 运行渲染循环
    void Cleanup();     // 清理资源
    
private:
    bool InitVulkan();    // 初始化Vulkan
    bool InitMaterial();  // 初始化材质
    bool InitGeometry();  // 初始化几何体
    bool InitECS();       // 初始化ECS
    void Update(double delta_time);  // 更新逻辑
    void Render();        // 渲染帧
};

int main(int argc, char** argv)
{
    StandaloneECSApp app;
    if (!app.Initialize()) return -1;
    app.Run();
    app.Cleanup();
    return 0;
}
```

## 与框架版本的对比 (Comparison with Framework Version)

| 特性 | 框架版本 (draw_triangle_use_ECS.cpp) | 独立版本 (draw_triangle_ECS_standalone.cpp) |
|------|--------------------------------------|---------------------------------------------|
| 基类 | 继承 WorkObject | 无基类，独立类 |
| 初始化 | Init() override | Initialize() 自定义 |
| 渲染循环 | 框架自动管理 | 手动实现 main() 循环 |
| Vulkan管理 | 框架自动处理 | 手动初始化和清理 |
| 入口函数 | os_main + RunFramework | 标准 main() |
| 依赖 | WorkManager.h | 仅Vulkan和ECS头文件 |
| 代码量 | ~230行 | ~400行（含注释） |
| 适用场景 | 快速开发 | 学习和完全控制 |

## 初始化流程 (Initialization Flow)

### 1. Vulkan初始化 (InitVulkan)
```cpp
// 概念性流程（需要实际实现）
VkInstance instance = CreateInstance();
VkPhysicalDevice physical_device = SelectPhysicalDevice(instance);
device = CreateLogicalDevice(physical_device);
swapchain = CreateSwapchain(device, width, height);
render_pass = CreateRenderPass(device, swapchain->GetFormat());
framebuffers = CreateFramebuffers(device, swapchain, render_pass);
cmd_buffer = CreateCommandBuffer(device);
```

### 2. 材质初始化 (InitMaterial)
```cpp
mtl::Material2DCreateConfig cfg(...);
VILConfig vil_config;
vil_config.Add(VAN::Position, POSITION_DATA_FORMAT);
vil_config.Add(VAN::Color, COLOR_DATA_FORMAT);

material_instance = device->CreateMaterialInstance(...);
pipeline = device->CreatePipeline(material_instance, render_pass);
```

### 3. 几何体初始化 (InitGeometry)
```cpp
// 转换顶点数据
for (uint i = 0; i < VERTEX_COUNT; i++) {
    position_data[i][0] = position_data_float[i][0] * width;
    position_data[i][1] = position_data_float[i][1] * height;
}

prim_triangle = device->CreatePrimitive(...);
```

### 4. ECS初始化 (InitECS)
```cpp
// 创建ECS世界
ecs_world = std::make_shared<ECSContext>("StandaloneTriangleWorld");
ecs_world->Initialize();

// 注册RenderCollector
render_collector = ecs_world->RegisterSystem<RenderCollector>();
render_collector->SetWorld(ecs_world);
render_collector->SetDevice(device);
render_collector->Initialize();

// 配置相机
CameraInfo camera;
camera.projectionMatrix = glm::ortho(...);
render_collector->SetCameraInfo(&camera);

// 创建Entity并添加组件
triangle_entity = ecs_world->CreateEntity<Entity>("TriangleEntity");
auto transform = triangle_entity->AddComponent<TransformComponent>();
auto primitive = triangle_entity->AddComponent<PrimitiveComponent>();
primitive->SetPrimitive(prim_triangle);
```

## 渲染循环 (Render Loop)

```cpp
void Run()
{
    while (running)
    {
        Update(delta_time);  // 更新ECS
        Render();            // 渲染一帧
    }
}

void Render()
{
    // 1. 获取swapchain图像
    uint32_t image_index = swapchain->AcquireNextImage();
    
    // 2. 记录命令
    cmd_buffer->Begin();
    cmd_buffer->BeginRenderPass(render_pass, framebuffers[image_index]);
    
    // 3. ECS渲染
    render_collector->CollectRenderables();
    render_collector->Render(cmd_buffer);
    
    // 4. 结束并提交
    cmd_buffer->EndRenderPass();
    cmd_buffer->End();
    device->Submit(cmd_buffer);
    
    // 5. 呈现
    swapchain->Present(image_index);
}
```

## 实现说明 (Implementation Notes)

### 概念性示例 (Conceptual Example)
本示例是**概念性的**，完整实现需要：

1. **平台相关代码**:
   - Windows: Win32 窗口创建
   - Linux: XCB/Wayland 窗口创建
   - MacOS: Metal/MoltenVK 支持

2. **Vulkan底层封装**:
   - VkInstance 创建和配置
   - 物理设备选择和队列族查询
   - 逻辑设备和队列创建
   - Swapchain 创建和管理
   - Synchronization (Semaphores/Fences)

3. **资源管理**:
   - 内存分配器
   - 描述符池和集合
   - 着色器加载和编译

### 完整实现需要的组件 (Required Components for Full Implementation)

```cpp
// 1. 窗口系统集成
#ifdef _WIN32
    HWND window = CreateWindowEx(...);
#elif defined(__linux__)
    xcb_window_t window = xcb_create_window(...);
#endif

// 2. Vulkan表面创建
VkSurfaceKHR surface;
#ifdef _WIN32
    vkCreateWin32SurfaceKHR(instance, &surface_info, nullptr, &surface);
#elif defined(__linux__)
    vkCreateXcbSurfaceKHR(instance, &surface_info, nullptr, &surface);
#endif

// 3. Swapchain创建
VkSwapchainCreateInfoKHR swapchain_info = {
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface = surface,
    .minImageCount = 2,
    .imageFormat = VK_FORMAT_B8G8R8A8_UNORM,
    .imageExtent = {width, height},
    // ... 更多配置
};

// 4. 同步对象
VkSemaphore image_available;
VkSemaphore render_finished;
VkFence in_flight_fence;

// 5. 渲染循环同步
vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, 
                      image_available, VK_NULL_HANDLE, &image_index);

vkQueueSubmit(graphics_queue, 1, &submit_info, in_flight_fence);

VkPresentInfoKHR present_info = {
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &render_finished,
    .swapchainCount = 1,
    .pSwapchains = &swapchain,
    .pImageIndices = &image_index,
};
vkQueuePresentKHR(present_queue, &present_info);
```

## 使用场景 (Use Cases)

### 适合使用独立版本的情况:
1. **学习目的**: 理解Vulkan和ECS渲染的完整流程
2. **自定义集成**: 需要集成到现有引擎或框架
3. **性能优化**: 需要完全控制渲染管线
4. **跨平台移植**: 需要适配特定平台

### 适合使用框架版本的情况:
1. **快速原型**: 快速测试想法和功能
2. **标准应用**: 使用标准应用程序结构
3. **减少样板代码**: 框架处理初始化和清理
4. **工具开发**: 利用框架提供的工具和功能

## 构建说明 (Building)

### 添加到CMakeLists.txt
```cmake
CreateProject(01c_draw_triangle_ECS_standalone    draw_triangle_ECS_standalone.cpp)
```

### 完整实现所需依赖
```cmake
target_link_libraries(01c_draw_triangle_ECS_standalone
    ${ULRE}              # ULRE核心库
    ${Vulkan_LIBRARIES}  # Vulkan
    glm                  # GLM数学库
)
```

## 学习路径 (Learning Path)

### 初学者建议顺序:
1. **draw_triangle_use_UBO.cpp**: 学习基本Vulkan渲染
2. **draw_triangle_use_ECS.cpp**: 学习ECS架构和框架集成
3. **draw_triangle_ECS_standalone.cpp**: 学习脱离框架的独立实现

### 进阶学习:
1. 添加窗口系统集成
2. 实现完整的Vulkan初始化
3. 添加输入处理
4. 实现资源热重载
5. 添加多线程渲染

## 参考资料 (References)

- **Vulkan Tutorial**: https://vulkan-tutorial.com/
- **Vulkan Specification**: https://www.khronos.org/registry/vulkan/
- **ULRE ECS Documentation**: `/inc/hgl/ecs/README.md`
- **Framework Version**: `draw_triangle_use_ECS.cpp`

## 总结 (Summary)

此独立版本展示了如何在不依赖高层框架的情况下使用ECS渲染系统。虽然需要更多的手动设置，但提供了：

- **完全的控制权**: 精确控制每个渲染步骤
- **学习价值**: 理解底层工作原理
- **灵活性**: 易于集成到任何系统
- **透明度**: 所有流程清晰可见

对于生产应用，建议使用框架版本以提高开发效率；对于学习和特殊需求，独立版本提供了最大的灵活性。
