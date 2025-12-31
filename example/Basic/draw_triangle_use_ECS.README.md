# Draw Triangle using ECS Architecture

## 简介 (Introduction)

本范例演示如何使用新的ECS（Entity Component System）架构来管理和绘制一个渐变色的三角形，**完全使用ECS RenderCollector进行渲染**。

This example demonstrates how to use the new ECS (Entity Component System) architecture to manage and draw a gradient colored triangle, **using ECS RenderCollector for actual rendering**.

## 主要特性 (Key Features)

### 1. ECS World 管理 (ECS World Management)
- 创建独立的 ECS World 来管理所有实体
- World 的初始化、更新和关闭生命周期管理

### 2. Entity 和 Component (Entity and Components)
- **Entity**: 代表三角形对象
- **TransformComponent**: 管理空间变换（位置、旋转、缩放）
- **PrimitiveComponent**: 管理渲染图元数据

### 3. ECS RenderCollector 渲染系统 (ECS RenderCollector Rendering)
本范例展示了**完全使用ECS渲染系统**：
- 使用 RenderCollector 系统收集可渲染的 Entity
- 自动进行批次渲染优化
- 支持间接渲染（Indirect Rendering）
- **不再依赖传统 Component 系统**

## 代码结构 (Code Structure)

```cpp
class TestApp : public WorkObject
{
    // ECS 组件
    std::shared_ptr<ECSContext>  ecs_world;
    std::shared_ptr<Entity> triangle_entity;
    RenderCollector* render_collector;      // ECS 渲染收集器
    
    // 渲染资源
    MaterialInstance *  material_instance;
    Primitive *         prim_triangle;
    Pipeline *          pipeline;
    
    bool InitMaterial();    // 初始化材质和管线
    bool InitVBO();         // 初始化顶点数据
    bool InitECS();         // 初始化 ECS 架构和 RenderCollector
    void Draw(RenderCmdBuffer* cmd_buf) override;  // 使用 ECS 渲染
};
```

## ECS 初始化流程 (ECS Initialization Flow)

1. **创建 World**
   ```cpp
   ecs_world = std::make_shared<ECSContext>("TriangleWorld");
   ecs_world->Initialize();
   ```

2. **注册并初始化 RenderCollector**
   ```cpp
   render_collector = ecs_world->RegisterSystem<RenderCollector>();
   render_collector->SetWorld(ecs_world);
   render_collector->SetDevice(GetDevice());
   render_collector->Initialize();
   
   // 配置相机
   CameraInfo camera;
   camera.projectionMatrix = glm::ortho(...);
   render_collector->SetCameraInfo(&camera);
   ```

3. **创建 Entity**
   ```cpp
   triangle_entity = ecs_world->CreateEntity<Entity>("TriangleEntity");
   ```

4. **添加 TransformComponent**
   ```cpp
   auto transform = triangle_entity->AddComponent<TransformComponent>();
   transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
   transform->SetMovable(false);
   ```

5. **添加 PrimitiveComponent**
   ```cpp
   auto ecs_primitive = triangle_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
   ecs_primitive->SetPrimitive(prim_triangle);
   ecs_primitive->SetVisible(true);
   ```

6. **使用 ECS 渲染**
   ```cpp
   void Draw(RenderCmdBuffer* cmd_buf) override
   {
       if(render_collector)
       {
           render_collector->CollectRenderables();
           render_collector->Render(cmd_buf);
       }
   }
   ```

## 与原始范例的对比 (Comparison with Original Example)

### 原始范例 (draw_triangle_use_UBO.cpp)
- 直接使用传统 Component 系统
- 简单直接，但缺乏灵活的数据组织

### ECS 范例 (draw_triangle_use_ECS.cpp)
- 使用现代 ECS 架构
- **完全使用 ECS RenderCollector 渲染**
- 更好的数据组织和缓存友好性
- 支持 SOA (Structure of Arrays) 存储优化
- 支持间接渲染（Indirect Rendering）和批次优化
- 更容易扩展和维护

## 技术细节 (Technical Details)

### RenderCollector 的工作流程
1. **收集阶段** (`CollectRenderables()`):
   - 遍历所有 Entity，查找具有 TransformComponent 和 PrimitiveComponent 的对象
   - 进行视锥剔除（Frustum Culling）
   - 按距离排序
   
2. **批次化阶段**:
   - 按 Material 和 Pipeline 分组
   - 合并使用相同几何数据的对象
   - 生成间接绘制命令
   
3. **渲染阶段** (`Render()`):
   - 遍历所有批次
   - 使用 vkCmdDrawIndirect 进行高效渲染
   - 自动优化状态切换

### TransformComponent 的特性
- **SOA 存储**: 使用 Structure of Arrays 提高缓存性能
- **Mobility 优化**: Static 对象 (movable=false) 会缓存世界矩阵，减少计算
- **层级支持**: 支持父子关系和世界坐标转换

### PrimitiveComponent 的特性
- 管理 Primitive 图元引用
- 支持材质覆盖 (Material Override)
- 自动计算包围体 (Bounding Volume)
- 可见性控制

## 性能考虑 (Performance Considerations)

1. **静态对象优化**: 使用 `SetMovable(false)` 来缓存不变的变换矩阵
2. **SOA 数据布局**: TransformComponent 使用 SOA 存储，提高批处理性能
3. **组件缓存**: Entity 使用哈希表快速查找组件
4. **间接渲染**: 使用 vkCmdDrawIndirect 减少 CPU 开销
5. **批次渲染**: 自动合并相同材质和几何的对象

## 与传统系统的性能对比 (Performance Comparison)

| 特性 | 传统系统 | ECS 系统 |
|------|----------|----------|
| Draw Call | 每个对象一次 | 批次合并 |
| 状态切换 | 频繁 | 最小化 |
| 内存布局 | AoS | SOA (缓存友好) |
| 渲染方式 | 直接渲染 | 间接渲染 |
| CPU 开销 | 高 | 低 |

## 未来改进 (Future Improvements)

1. ~~完全集成 ECS PrimitiveComponent 到渲染管线~~ ✅ **已完成**
2. ~~实现 RenderCollector 系统收集 ECS 可渲染对象~~ ✅ **已完成**
3. 支持多线程的 ECS 系统更新
4. 添加更多 ECS 组件类型（动画、物理等）
5. GPU Culling 支持

## 构建和运行 (Building and Running)

参考主项目的 BUILD.md 文件进行构建。本示例需要：
- Windows 11
- Visual Studio 2019/2022
- Vulkan SDK
- CMake 3.28+

构建后，运行 `01b_draw_triangle_ECS.exe` 即可看到使用 ECS 架构渲染的三角形。

## 参考资料 (References)

- `/inc/hgl/ecs/` - ECS 架构头文件
- `/src/ecs/` - ECS 实现源码
- `/src/ecs/test_ecs.cpp` - ECS 基础测试
- `/src/ecs/test_primitive_component.cpp` - PrimitiveComponent 使用文档
- `draw_triangle_use_UBO.cpp` - 原始三角形绘制范例
