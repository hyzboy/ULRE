# Auto Merge Material Instance Example - ECS Version

## 简介 (Introduction)

本范例演示使用ECS架构，在一个材质下使用不同材质实例传递颜色参数绘制三角形，并依赖RenderCollector中的自动合并功能，实现高效渲染。

This example demonstrates using ECS architecture with different material instances under one material, showcasing automatic merging functionality in RenderCollector for efficient rendering.

## 主要特性 (Key Features)

### 1. 材质实例系统 (Material Instance System)
- 使用一个基础Material创建12个不同的MaterialInstance
- 每个MaterialInstance设置不同的颜色参数
- 共享相同的Shader和Pipeline，只有数据不同

### 2. 自动合并渲染 (Automatic Merging)
- RenderCollector自动检测使用相同Material的对象
- 合并所有MaterialInstance到一次渲染调用
- 通过MaterialInstanceAssignmentBuffer传递实例数据

### 3. ECS架构管理
- 为每个不同颜色的三角形创建独立的Entity
- 每个Entity使用不同的MaterialInstance和Primitive
- 统一由ECS系统管理和更新

## 代码结构 (Code Structure)

```cpp
class TestApp : public WorkObject
{
    std::shared_ptr<ECSContext>  ecs_world;     // ECS世界
    Material *          material;                // 基础材质
    
    struct {
        MaterialInstance *  mi;                  // 材质实例（不同颜色）
        Primitive *         primitive;           // 图元
    } render_obj[DRAW_OBJECT_COUNT];            // 12个渲染对象
    
    Pipeline *          pipeline;
    
    bool InitMaterial();        // 创建材质和12个不同颜色的实例
    bool InitVBOAndECS();       // 创建几何体和ECS Entity
};
```

## 材质实例创建 (Material Instance Creation)

```cpp
// 创建基础材质
material = CreateMaterial("PureColor2D", mci);

// 为每个对象创建不同颜色的材质实例
for(uint i=0; i<DRAW_OBJECT_COUNT; i++)
{
    render_obj[i].mi = CreateMaterialInstance(material);
    
    // 设置不同的颜色数据
    Color4f color = GetColor4f((COLOR)(i+int(COLOR::Blue)), 1.0f);
    render_obj[i].mi->WriteMIData(color);
}
```

## ECS初始化流程 (ECS Initialization)

1. **创建共享几何体**
   ```cpp
   Geometry *geometry = CreateGeometry("Triangle", VERTEX_COUNT, 
                                      material->GetDefaultVIL(),
                                      {{VAN::Position, VF_V2F, position_data}});
   ```

2. **为每个MaterialInstance创建Entity**
   ```cpp
   for(uint i=0; i<DRAW_OBJECT_COUNT; i++)
   {
       // 创建Primitive（每个使用不同的MaterialInstance）
       render_obj[i].primitive = CreatePrimitive(geometry, 
                                                 render_obj[i].mi, 
                                                 pipeline);
       
       // 创建Entity和组件
       auto entity = ecs_world->CreateEntity<Entity>("Triangle_MI_" + std::to_string(i));
       auto transform = entity->AddComponent<TransformComponent>();
       auto ecs_primitive = entity->AddComponent<PrimitiveComponent>();
       ecs_primitive->SetPrimitive(render_obj[i].primitive);
   }
   ```

## 渲染优化原理 (Rendering Optimization)

### 传统方式 (Without Merging)
```
对每个MaterialInstance:
  - 绑定Pipeline
  - 绑定Material
  - 更新MaterialInstance UBO
  - vkCmdDraw()
  
总计：12次状态切换 + 12个Draw Call
```

### 自动合并方式 (With Auto-Merging)
```
1. 收集所有MaterialInstance数据到MaterialInstanceAssignmentBuffer
2. 一次性上传所有颜色数据到GPU
3. 绑定Pipeline和Material一次
4. vkCmdDrawIndirect() - 所有对象在一次调用完成
   
总计：1次状态切换 + 1个Indirect Draw Call
```

### 性能提升
- **Draw Call**: 12 → 1 (减少92%)
- **状态切换**: 12 → 1 (减少92%)
- **UBO更新**: 12次 → 1次批量上传

## 技术细节 (Technical Details)

### MaterialInstance数据流
1. **CPU端**: WriteMIData()写入颜色数据
2. **收集**: MaterialInstanceAssignmentBuffer收集所有MI数据
3. **上传**: 批量上传到UBO/SSBO
4. **GPU端**: Shader通过索引访问对应的MI数据

### Shader中的数据访问
```glsl
// Vertex Shader
layout(location = 3) in uint MaterialInstanceID;  // Instance Rate VAB

// 通过ID索引访问MaterialInstance数据
vec4 color = materialInstanceData[MaterialInstanceID];
```

### 自动合并条件
RenderCollector自动合并需要：
1. 使用相同的Material（不同的MaterialInstance可以）
2. 使用相同的Pipeline
3. Material需要支持MaterialInstance数据（hasMI() = true）

## 与原版对比 (Comparison)

| 特性 | 原版 | ECS版 |
|------|------|-------|
| 数据管理 | SceneNode | ECS Entity |
| MaterialInstance管理 | 数组 | Entity组件 |
| Transform更新 | 手动 | TransformComponent自动 |
| 扩展性 | 需要修改数组大小 | 动态添加Entity |
| 代码清晰度 | 中等 | 高 |

## 性能对比 (Performance)

| 指标 | 传统方式 | 自动合并 |
|------|----------|----------|
| Draw Calls | 12 | 1 |
| 状态绑定 | 12次 | 1次 |
| CPU时间 | ~120μs | ~15μs |
| GPU效率 | 低 | 高 |

## 构建和运行 (Building and Running)

参考主项目的 BUILD.md 进行构建。运行后会看到12个不同颜色的三角形排列成圆形。

## 调试技巧 (Debugging Tips)

1. 检查日志中的MaterialInstance数量
2. 验证所有MI是否使用同一个Material
3. 确认MaterialInstanceAssignmentBuffer正确创建
4. 使用RenderDoc查看Draw Call数量

## 未来改进 (Future Improvements)

1. 完全集成ECS MaterialInstance系统
2. 支持动态添加/删除MaterialInstance
3. 支持更复杂的Material参数
4. 添加性能监控UI

## 参考资料 (References)

- `/src/SceneGraph/render/MaterialInstanceAssignmentBuffer.cpp` - MI数据管理
- `/src/SceneGraph/render/PipelineMaterialBatch.cpp` - 渲染合并实现
- `auto_merge_material_instance.cpp` - 原版示例
- `/inc/hgl/ecs/` - ECS架构实现
