# Draw Triangle using ECS Architecture

## 简介 (Introduction)

本范例演示如何使用新的ECS（Entity Component System）架构来管理和绘制一个渐变色的三角形。

This example demonstrates how to use the new ECS (Entity Component System) architecture to manage and draw a gradient colored triangle.

## 主要特性 (Key Features)

### 1. ECS World 管理 (ECS World Management)
- 创建独立的 ECS World 来管理所有实体
- World 的初始化、更新和关闭生命周期管理

### 2. Entity 和 Component (Entity and Components)
- **Entity**: 代表三角形对象
- **TransformComponent**: 管理空间变换（位置、旋转、缩放）
- **PrimitiveComponent**: 管理渲染图元数据

### 3. 与传统渲染系统集成 (Integration with Legacy Rendering System)
由于新的 ECS PrimitiveComponent 尚未完全集成到渲染管线中，本范例展示了如何在过渡期同时使用两个系统：
- ECS 系统用于数据管理和组织
- 传统 Component 系统用于实际渲染

## 代码结构 (Code Structure)

```cpp
class TestApp : public WorkObject
{
    // ECS 组件
    std::shared_ptr<World>  ecs_world;
    std::shared_ptr<Entity> triangle_entity;
    
    // 传统渲染资源
    MaterialInstance *  material_instance;
    Primitive *         prim_triangle;
    Pipeline *          pipeline;
    
    bool InitMaterial();    // 初始化材质和管线
    bool InitVBO();         // 初始化顶点数据
    bool InitECS();         // 初始化 ECS 架构
};
```

## ECS 初始化流程 (ECS Initialization Flow)

1. **创建 World**
   ```cpp
   ecs_world = std::make_shared<World>("TriangleWorld");
   ecs_world->Initialize();
   ```

2. **创建 Entity**
   ```cpp
   triangle_entity = ecs_world->CreateEntity<Entity>("TriangleEntity");
   ```

3. **添加 TransformComponent**
   ```cpp
   auto transform = triangle_entity->AddComponent<TransformComponent>();
   transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
   transform->SetMovable(false);  // Set to static
   ```

4. **添加 PrimitiveComponent**
   ```cpp
   auto ecs_primitive = triangle_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
   ecs_primitive->SetPrimitive(prim_triangle);
   ecs_primitive->SetVisible(true);
   ```

5. **集成传统渲染系统**
   ```cpp
   CreateComponentInfo cci(GetWorldRootNode());
   CreateComponent<component::PrimitiveComponent>(&cci, prim_triangle);
   ```

## 与原始范例的对比 (Comparison with Original Example)

### 原始范例 (draw_triangle_use_UBO.cpp)
- 直接使用传统 Component 系统
- 简单直接，但缺乏灵活的数据组织

### ECS 范例 (draw_triangle_use_ECS.cpp)
- 使用现代 ECS 架构
- 更好的数据组织和缓存友好性
- 支持 SOA (Structure of Arrays) 存储优化
- 更容易扩展和维护

## 技术细节 (Technical Details)

### TransformComponent 的特性
- **SOA 存储**: 使用 Structure of Arrays 提高缓存性能
- **Movable 优化**: 静态对象（movable=false）会缓存世界矩阵，减少计算
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

## 未来改进 (Future Improvements)

1. 完全集成 ECS PrimitiveComponent 到渲染管线
2. 实现 RenderCollector 系统收集 ECS 可渲染对象
3. 支持多线程的 ECS 系统更新
4. 添加更多 ECS 组件类型（动画、物理等）

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
