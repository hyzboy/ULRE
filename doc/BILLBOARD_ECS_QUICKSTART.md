# Billboard ECS 快速参考

## 🎯 一分钟快速开始

### 最小化示例

```cpp
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/BillboardComponent.h>
#include<hgl/ecs/components/TransformComponent.h>

// 获取 ECS 世界
ECSContext* world = GetECSContext();

// 创建 Billboard 实体
Entity* bb = world->CreateEntity<Entity>("MyBillboard");

// 添加变换
auto transform = bb->AddComponent<TransformComponent>();
transform->SetLocalPosition(glm::vec3(0, 5, 0));

// 添加 Billboard 组件
auto billboard = bb->AddComponent<BillboardComponent>();
billboard->SetPrimitive(my_primitive);
billboard->SetVisible(true);
billboard->SetFixedPixelSize(true);
billboard->SetPixelSize(128, 128);
```

## 📝 常见操作

### 修改 Billboard 大小

```cpp
// 固定像素大小
billboard->SetFixedPixelSize(true);
billboard->SetPixelSize(256, 256);

// 世界空间大小
billboard->SetFixedPixelSize(false);
billboard->SetWorldSize(10.0f, 10.0f);
```

### 改变贴图

```cpp
auto material = billboard->GetMaterialInstance()->GetMaterial();
material->BindTextureSampler(
    DescriptorSetType::PerMaterial,
    mtl::SamplerName::BaseColor,
    new_texture,
    sampler
);
```

### 控制可见性

```cpp
billboard->SetVisible(true);   // 显示
billboard->SetVisible(false);  // 隐藏

// 或检查可见性
if (billboard->IsVisible()) {
    // ...
}
```

### 获取 Billboard 属性

```cpp
bool is_fixed = billboard->IsFixedPixelSize();
auto pixel_sz = billboard->GetPixelSize();
auto world_sz = billboard->GetWorldSize();
auto face = billboard->GetFrontFace();
```

## 🏗️ 初始化流程

### 完整的初始化步骤

```cpp
// 1. 创建材质
mtl::BillboardMaterialCreateConfig cfg(PrimitiveType::Billboard);
cfg.fixed_size = true;
MaterialInstance* mi = material_manager->CreateMaterialInstance(
    mtl::inline_material::Billboard2D, &cfg
);

// 2. 创建管道
Pipeline* pipeline = render_pass->CreatePipeline(mi, InlinePipeline::Solid3D);

// 3. 加载纹理
Texture2D* texture = tex_manager->LoadTexture2D(...);
Sampler* sampler = sampler_manager->CreateSampler();

// 4. 绑定纹理
mi->GetMaterial()->BindTextureSampler(
    DescriptorSetType::PerMaterial,
    mtl::SamplerName::BaseColor,
    texture,
    sampler
);

// 5. 创建几何体（单点）
GeometryCreater pc(device, mi->GetVIL());
pc.Init("Billboard", 1);
pc.WriteVAB(VAN::Position, VF_V3F, position_data);
Primitive* prim = primitive_manager->CreatePrimitive(&pc, mi, pipeline);

// 6. 创建 ECS 实体
Entity* billboard = world->CreateEntity<Entity>("Billboard");
auto bb = billboard->AddComponent<BillboardComponent>();
bb->SetPrimitive(prim);
bb->SetFixedPixelSize(true);
bb->SetPixelSize(256, 256);
```

## 📊 关键类

| 类 | 位置 | 用途 |
|---|------|------|
| `BillboardComponent` | `hgl/ecs/components/BillboardComponent.h` | Billboard 数据和行为 |
| `BillboardRenderSystem` | `hgl/ecs/systems/render/BillboardRenderSystem.h` | Billboard 动态更新 |
| `PrimitiveComponent` | `hgl/ecs/components/PrimitiveComponent.h` | 基础图元组件 |
| `TransformComponent` | `hgl/ecs/components/TransformComponent.h` | 位置、旋转、缩放 |
| `ECSContext` | `hgl/ecs/core/Context.h` | ECS 世界管理 |

## 🔧 Billboard 配置参数

### 材质配置

```cpp
struct BillboardMaterialCreateConfig : public Material3DCreateConfig {
    bool fixed_size = true;              // 使用固定大小
    Vector2u pixel_size = {256, 256};    // 像素大小
    VkFrontFace front_face = VK_FRONT_FACE_CLOCKWISE;  // 正面朝向
};
```

### BillboardComponent 属性

| 属性 | 类型 | 默认值 | 说明 |
|-----|------|--------|------|
| `fixed_size` | `bool` | `true` | 是否使用固定像素大小 |
| `pixel_size` | `Vector2u` | `{256, 256}` | 像素为单位的大小 |
| `world_size` | `glm::vec2` | `{1.0f, 1.0f}` | 世界单位的大小 |
| `front_face` | `VkFrontFace` | `VK_FRONT_FACE_CLOCKWISE` | 正面朝向 |

## 🎬 完整示例运行

### 编译

```bash
# 编译新的 BillboardECS 示例
cmake --build build --config Debug --target 05b_BillboardECS
```

### 运行

```bash
./build/out/Windows_64_Debug/05b_BillboardECS.exe
```

### 预期行为

- 显示一个平面网格（参考地面）
- in 棋盘中心显示一个纹理 Billboard
- 可以通过摄像头控制查看 Billboard

## 🐛 故障排查

### Billboard 不显示

1. 检查 `SetVisible(true)` 是否被调用
2. 验证 `SetPrimitive()` 已设置有效的 Primitive
3. 确认材质和管道初始化成功
4. 检查视锥体剔除（可能 Billboard 在相机视锥外）

### 纹理不显示

1. 检查纹理加载是否成功
2. 验证 `GetMaterial()->BindTextureSampler()` 返回 true
3. 检查材质实例数据是否已写入（`WriteMIData()`）

### 大小不正确

1. 对固定大小：检查 `SetFixedPixelSize(true)` 和像素大小
2. 对世界大小：检查 `SetFixedPixelSize(false)` 和世界大小
3. 验证视口和摄像头配置

## 📚 相关文档

- `doc/Billboard_ECS_Integration.md` - 完整的架构文档
- `example/Basic/BillboardECS.cpp` - 完整示例代码
- `example/Basic/BillboardTest.cpp` - 原始实现（参考）
- 源代码注释 - 所有 API 的详细说明

## ✨ 关键特性

✅ 完整的 ECS 集成  
✅ 组件继承和组合  
✅ 序列化/反序列化支持  
✅ 与现有系统无缝协作  
✅ 高性能批处理渲染  
✅ 灵活的大小配置（固定或动态）  
✅ 完整的文档和示例  

## 🚀 下一步

1. 查看 `example/Basic/BillboardECS.cpp` 了解完整示例
2. 阅读 `doc/Billboard_ECS_Integration.md` 了解架构细节
3. 尝试修改示例（改变大小、位置、纹理）
4. 集成到你的游戏或应用中

---

**版本：** 1.0  
**最后更新：** 2024  
**状态：** ✅ 正式版
