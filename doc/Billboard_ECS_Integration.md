# Billboard ECS 集成文档

## 概述

本文档描述了 Billboard 功能与现有 ECS 架构的完整集成。该实现包括新的 `BillboardComponent` 组件和 `BillboardRenderSystem` 系统，用于在 ECS 框架中渲染公告板。

## 组件和系统

### 1. **BillboardComponent** (`inc/hgl/ecs/components/BillboardComponent.h`)

一个专为 Billboard 渲染优化的 ECS 组件，继承自 `PrimitiveComponent`。

**功能：**
- 管理 Billboard 大小（固定像素大小或世界空间大小）
- 跟踪正面朝向方向（顺时针或逆时针）
- 支持序列化/反序列化

**成员：**
- `fixed_size` - 是否使用固定像素大小
- `pixel_size` - 像素为单位的大小（当固定大小时）
- `world_size` - 世界单位的大小（当非固定大小时）
- `front_face` - Vulkan 正面朝向（VK_FRONT_FACE_CLOCKWISE 或 VK_FRONT_FACE_COUNTER_CLOCKWISE）

**API：**
```cpp
// 大小管理
void SetFixedPixelSize(bool fixed);
void SetPixelSize(uint32_t width, uint32_t height);
void SetPixelSize(const hgl::math::Vector2u& size);
void SetWorldSize(float width, float height);
void SetWorldSize(const glm::vec2& size);

// 正面朝向
void SetFrontFace(VkFrontFace face);
```

**继承自 PrimitiveComponent：**
- `SetPrimitive()` - 设置要渲染的 Primitive
- `SetOverrideMaterial()` - 覆盖 Primitive 的材质
- `SetVisible()` - 控制可见性

### 2. **BillboardRenderSystem** (`inc/hgl/ecs/systems/render/BillboardRenderSystem.h`)

一个渲染系统，用于处理 Billboard 特定的渲染操作。

**设计说明：**
- Billboard 的实际渲染由 `RenderPrimitiveBatchSystem` 处理（处理所有 PrimitiveComponent 派生类）
- `BillboardRenderSystem` 可用于日后的动态更新（例如基于距离调整大小）

**API：**
```cpp
void SetWorld(ECSContext* w);
void SetCameraInfo(const graph::CameraInfo* info);
void Update(float deltaTime) override;
```

## 示例代码

### 基础示例：创建 Billboard 实体

```cpp
// 创建 Billboard 实体
Entity* billboard_entity = ecs_world->CreateEntity<Entity>("MyBillboard");

// 添加 Transform 组件
auto billboard_transform = billboard_entity->AddComponent<TransformComponent>();
billboard_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
billboard_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));

// 添加 Billboard 组件
auto billboard = billboard_entity->AddComponent<BillboardComponent>();
billboard->SetPrimitive(prim_billboard);
billboard->SetVisible(true);
billboard->SetFixedPixelSize(true);
billboard->SetPixelSize(256, 256);
billboard->SetFrontFace(VK_FRONT_FACE_CLOCKWISE);
```

### 完整示例应用

参考 `example/Basic/BillboardECS.cpp`，该示例展示了：

1. **材质初始化**
   - 使用 `BillboardMaterialCreateConfig` 创建 Billboard 材质
   - 配置固定大小选项

2. **纹理绑定**
   - 加载纹理
   - 创建采样器
   - 将纹理绑定到 Billboard 材质

3. **几何体创建**
   - 使用 `GeometryCreater` 创建 Billboard 几何体
   - 设置为单个点以支持 Billboard 着色器

4. **ECS 实体设置**
   - 创建 Billboard 实体
   - 添加 TransformComponent 用于位置/旋转/缩放
   - 添加 BillboardComponent 用于 Billboard 特定属性
   - 关联摄像头和渲染系统

## 与现有 ECS 的集成

### 继承链

```
RenderableComponent
    ↓
PrimitiveComponent
    ↓
BillboardComponent
```

### 系统执行流程

```
1. TransformSystem - 更新变换矩阵
2. BoundingBoxUpdateSystem - 更新包围盒
3. VisibilitySystem - 可见性更新
4. RenderPrimitiveCollectSystem - 收集可渲染项支持（包括 BillboardComponent）
5. RenderPrimitiveBatchSystem - 批处理和剔除
6. BillboardRenderSystem - Billboard 特定更新（可选）
7. RenderPrimitiveSubmitSystem - 提交渲染命令
```

### 材质和管道

Billboard 使用专门的材质：
- **材质类型：** `mtl::inline_material::Billboard2D`
- **配置：** `BillboardMaterialCreateConfig`
- **关键参数：**
  - `fixed_size` - 使用固定像素大小
  - `pixel_size` - 像素为单位的大小
  - `front_face` - 正面朝向

## 着色器支持

Billboard 着色器基础设施已在以下文件中定义：
- `src/ShaderGen/common/MFBillboard.h` - Billboard 数据结构和着色器函数
- `src/ShaderGen/3d/S_BillboardVertex.h` - Billboard 顶点着色器

着色器使用以下数据：
- 中心位置
- 大小（世界单位或像素）
- 相机方向（右向量、上向量）

## 文件变更

### 新增文件
- `inc/hgl/ecs/components/BillboardComponent.h` - 组件头文件
- `src/ecs/components/BillboardComponent.cpp` - 组件实现
- `inc/hgl/ecs/systems/render/BillboardRenderSystem.h` - 系统头文件
- `src/ecs/systems/render/BillboardRenderSystem.cpp` - 系统实现
- `example/Basic/BillboardECS.cpp` - Billboard ECS 示例应用

### 修改的文件
- `src/ecs/CMakeLists.txt` - 添加新的组件和系统源文件
- `example/Basic/CMakeLists.txt` - 添加新的示例项目

## 编译说明

### 使用 CMake 生成构建文件
```bash
cd d:\ULRE
cmake -B build -S .
```

### 构建 ECS 模块
```bash
cmake --build build --config Debug --target ULRE.ECS
```

### 构建 Billboard 示例
```bash
cmake --build build --config Debug --target 05b_BillboardECS
```

### 构建原始 Billboard 示例（兼容性验证）
```bash
cmake --build build --config Debug --target 05_Billboard
```

## 扩展建议

### 1. 动态大小调整
可以在 `BillboardRenderSystem::UpdateBillboardProperties()` 中实现：
```cpp
// 基于到相机的距离调整大小
float distance = glm::distance(billboard_position, camera_position);
billboard->SetPixelSize(base_size * (1.0f + distance * 0.1f));
```

### 2. Billboard 动画
- 添加旋转、缩放、颜色变化等时间参数
- 在 `BillboardComponent` 中存储动画状态
- 在系统的 `Update()` 中处理时间更新

### 3. Billboard 池优化
- 为频繁创建/销毁的 Billboard 实现对象池
- 使用 ECS 的实体创建/销毁 API

### 4. Billboard 碰撞检测
- 在 Billboard 实体中添加 `BoundingBoxComponent`
- 使用 `PhysicsSystem` （如果存在）处理碰撞

## 常见问题

**Q: BillboardComponent 和 PrimitiveComponent 的区别是什么？**
A: BillboardComponent 继承自 PrimitiveComponent，添加了 Billboard 特定属性（大小、朝向）。如果不需要这些特性，可以直接使用 PrimitiveComponent。

**Q: 如何改变 Billboard 的纹理？**
A: 通过 MaterialInstance 的 API：
```cpp
billboard->GetMaterialInstance()->GetMaterial()->BindTextureSampler(
    DescriptorSetType::PerMaterial,
    mtl::SamplerName::BaseColor,
    new_texture,
    sampler
);
```

**Q: Billboard 的性能如何？**
A: Billboard 利用 `RenderPrimitiveBatchSystem` 的批处理功能，多个 Billboard 可以在单个 draw call 中渲染（如果材质和管道相同）。

## 参考

- `inc/hgl/ecs/components/PrimitiveComponent.h` - 父类实现
- `inc/hgl/ecs/systems/render/RenderPrimitiveBatchSystem.h` - 批处理系统
- `example/Basic/BillboardTest.cpp` - 原始 Billboard 示例
- `example/Basic/BillboardECS.cpp` - 新的 ECS Billboard 示例
