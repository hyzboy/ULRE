# Auto Instance Example - ECS Version

## 简介 (Introduction)

本范例演示使用ECS架构和RenderCollector系统绘制多个三角形，展示自动实例化（Instance Rendering）功能。

This example demonstrates using ECS architecture with RenderCollector to draw multiple triangles with automatic instance rendering.

## 主要特性 (Key Features)

### 1. ECS架构管理多个实体
- 创建12个三角形Entity，每个具有不同的旋转角度
- 使用TransformComponent管理每个Entity的空间变换
- 所有Entity共享同一个Primitive（几何数据）

### 2. 自动实例化渲染
- RenderCollector自动检测使用相同几何数据的对象
- 自动合并为Instance渲染，减少Draw Call
- 提高渲染性能，特别是在大量重复对象场景

### 3. 与传统系统对比
- **原版** (`auto_instance.cpp`): 使用传统Component系统
- **ECS版** (`auto_instance_ECS.cpp`): 使用现代ECS架构

## 代码结构 (Code Structure)

```cpp
class TestApp : public WorkObject
{
    std::shared_ptr<ECSContext>  ecs_world;      // ECS世界
    MaterialInstance *  material_instance;        // 材质实例
    Primitive *         render_obj;               // 共享的几何体
    Pipeline *          pipeline;                 // 渲染管线
    
    bool InitMaterial();    // 初始化材质
    bool InitVBO();         // 创建几何数据
    bool InitECS();         // 初始化ECS并创建12个Entity
};
```

## ECS初始化流程 (ECS Initialization)

1. **创建ECS世界**
   ```cpp
   ecs_world = std::make_shared<ECSContext>("AutoInstanceWorld");
   ecs_world->Initialize();
   ```

2. **循环创建12个三角形Entity**
   ```cpp
   for(uint i=0; i<TRIANGLE_NUMBER; i++)
   {
       auto entity = ecs_world->CreateEntity<Entity>("Triangle_" + std::to_string(i));
       
       // 设置旋转变换
       auto transform = entity->AddComponent<TransformComponent>();
       rad = deg2rad((360.0f/TRIANGLE_NUMBER)*i);
       glm::quat rotation = glm::angleAxis((float)rad, glm::vec3(0,0,1));
       transform->SetLocalRotation(rotation);
       
       // 添加渲染组件（所有Entity共享同一Primitive）
       auto ecs_primitive = entity->AddComponent<PrimitiveComponent>();
       ecs_primitive->SetPrimitive(render_obj);
   }
   ```

## 性能优势 (Performance Benefits)

### 传统方式 (Traditional)
- 12个Draw Call（每个三角形一个）
- 重复绑定相同的顶点缓冲和材质

### 实例化方式 (Instanced)
- 1个Draw Call（使用vkCmdDrawIndirect）
- 所有三角形在一次调用中完成
- **性能提升**: ~12倍减少Draw Call开销

## 技术细节 (Technical Details)

### Instance渲染条件
RenderCollector自动合并需要满足：
1. 使用相同的Material和Pipeline
2. 使用相同的Geometry数据
3. 使用相同的DrawRange

### Transform数据传递
- 通过TransformAssignmentBuffer传递LocalToWorld矩阵
- 使用Instance Rate的VAB传递Transform索引
- Shader通过索引访问正确的Transform矩阵

### ECS优化
- TransformComponent使用SOA存储，提高缓存性能
- 静态对象（movable=false）缓存世界矩阵
- 批量更新所有Transform数据

## 与原版对比 (Comparison)

| 特性 | 原版 | ECS版 |
|------|------|-------|
| 数据组织 | SceneNode树 | ECS Entity |
| Transform管理 | NodeTransform | TransformComponent (SOA) |
| 组件添加 | CreateComponent() | entity->AddComponent() |
| 缓存友好性 | 较低 | 高 (SOA) |
| 扩展性 | 中等 | 高 |

## 构建和运行 (Building and Running)

参考主项目的 BUILD.md 进行构建。运行后会看到12个彩色三角形排列成圆形。

## 未来改进 (Future Improvements)

1. 完全集成ECS RenderCollector（目前仍需传统Component协助）
2. 支持动态对象的实例化
3. 添加性能统计显示（Draw Call数量）

## 参考资料 (References)

- `/inc/hgl/ecs/` - ECS架构实现
- `/src/SceneGraph/render/PipelineMaterialBatch.cpp` - Instance渲染实现
- `auto_instance.cpp` - 原版示例
- `draw_triangle_use_ECS.cpp` - ECS基础示例
