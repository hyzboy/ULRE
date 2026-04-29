# Sprite2D 迁移 — Step 2：内置 Unit-Square Primitive

> 状态：可独立编译；运行时无副作用（系统注册了但不生效，因为还没有 `Sprite2DComponent`）
> 风险等级：低
> 预计耗时：2–3 小时
> 关键原则：**只增不删**。`QuadResourcePrepareSystem` 必须保持完全不变。

---

## 0. 目标

1. 新建 `Sprite2DResourcePrepareSystem`，实现 `EnsureSharedResources()`：
   - 创建 1 个 4 顶点 / 6 索引、中心 `(0,0)`、边长 `1×1` 的共享 `graph::Primitive*`。
   - 顶点格式 = `vec2 Position + vec2 TexCoord`（与 Step 1 的 VS 输入对齐）。
2. 不接入任何 `RenderPipelineGroup`（那是 Step 4）。
3. 不删除/不修改 `QuadResourcePrepareSystem`。

---

## 1. 前置条件

- Step 1 通关：两个新 variant 已在 ShaderGen 注册，对应 SPIR-V 文件存在。
- `03_BillboardPerspectiveECS` 仍可正常跑通（baseline）。

---

## 2. 涉及文件

| 路径 | 改动 |
|---|---|
| `inc/hgl/ecs/systems/render/Sprite2DResourcePrepareSystem.h` | **新建** |
| `src/ecs/systems/render/Sprite2DResourcePrepareSystem.cpp` | **新建** |
| `src/ecs/CMakeLists.txt` | 把上面两个文件加入 ECS 模块编译 + `source_group` |
| `inc/hgl/ecs/systems/render/QuadResourcePrepareSystem.h` | **不动** |
| `src/ecs/systems/render/QuadResourcePrepareSystem.cpp` | **不动** |

---

## 3. 执行步骤

### 3.1 拷贝 + 改名

```pwsh
Copy-Item inc/hgl/ecs/systems/render/QuadResourcePrepareSystem.h `
          inc/hgl/ecs/systems/render/Sprite2DResourcePrepareSystem.h
Copy-Item src/ecs/systems/render/QuadResourcePrepareSystem.cpp `
          src/ecs/systems/render/Sprite2DResourcePrepareSystem.cpp
```

然后在新文件中：

- 类名：`QuadResourcePrepareSystem` → `Sprite2DResourcePrepareSystem`
- include guard / `#pragma once` 不动
- include 头文件：暂时仍引用 `QuadComponent.h`，**Step 3 再换成 `Sprite2DComponent.h`**（避免本步骤产生编译依赖）
- `SystemType` 字段：考虑新增枚举 `SystemType::Sprite2DResourcePrepare`；如果不想改 ECS 核心枚举，可暂时复用一个不会冲突的值，Step 4 再调整。

> ⚠️ 不要在本步骤就把类名改回 `QuadResource...` 然后"原地修改" — 那等于改了旧系统，违反"只增不删"原则。

### 3.2 改写 `EnsureSharedResources()`

旧 `QuadResourcePrepareSystem` 创建的是单点 mesh（1 顶点，给 BillboardCameraFacing 在 shader 里展开）。新系统改为创建 4 顶点 unit square。

`Sprite2DResourcePrepareSystem.cpp` 关键代码：

```cpp
namespace hgl::ecs
{
namespace
{
    constexpr float SPRITE2D_VB[] = {
        // pos.x  pos.y   uv.x  uv.y    （Vulkan: V=0 顶部, V=1 底部）
        -0.5f,  -0.5f,    0.0f, 1.0f,   // 顶点 0：模型左下 → UV 左下（屏幕底）
         0.5f,  -0.5f,    1.0f, 1.0f,   // 顶点 1：模型右下 → UV 右下
         0.5f,   0.5f,    1.0f, 0.0f,   // 顶点 2：模型右上 → UV 右上
        -0.5f,   0.5f,    0.0f, 0.0f,   // 顶点 3：模型左上 → UV 左上
    };

    constexpr uint16_t SPRITE2D_IB[] = {
        0, 1, 2,
        0, 2, 3,
    };
}

bool Sprite2DResourcePrepareSystem::EnsureSharedResources(graph::GraphicsContext* gc)
{
    if (!gc) return false;
    if (shared_unit_square_primitive) return true;   // 已建好

    GeometryVertexFormat gvf;
    gvf.Set(VAN::Position, VF_V2F);
    gvf.Set(VAN::TexCoord, VF_V2F);

    // 拆出独立的 Position / TexCoord buffer（与 ULRE 现有 VBO 一份属性一个 buffer 的约定一致）
    static const float pos[8] = {
        -0.5f,-0.5f,  0.5f,-0.5f,  0.5f,0.5f, -0.5f,0.5f
    };
    static const float uv[8] = {
        0.f,1.f,  1.f,1.f,  1.f,0.f,  0.f,0.f
    };

    shared_unit_square_primitive = GraphicsGeometryFactory::CreateGeometry(
        gc,
        "Sprite2DUnitSquare",
        4, 6, IndexType::U16,
        {
            { VAN::Position, VF_V2F, pos },
            { VAN::TexCoord, VF_V2F, uv  },
        },
        SPRITE2D_IB);

    if (!shared_unit_square_primitive)
        return false;

    GraphicsGeometryFactory factory(gc);
    return factory.RegisterGeometry(shared_unit_square_primitive) != nullptr;
}
```

> 与 `BillboardIconECSBase::CreateSharedSpriteGeometry()` 路径完全一致，已经在前一段重构中验证过 OK。

### 3.3 头文件接口最小化

`Sprite2DResourcePrepareSystem.h` 只暴露：

```cpp
class Sprite2DResourcePrepareSystem : public System
{
public:
    explicit Sprite2DResourcePrepareSystem(ECSContext* ctx = nullptr);

    void  SetGraphicsContext(graph::GraphicsContext* gc);
    bool  EnsureSharedResources();   // 内部调用上面那个 gc 参数版本
    graph::Primitive* GetSharedPrimitive() const { return shared_unit_square_primitive; }

    void Update(float deltaTime) override;   // 实现里只调用 EnsureSharedResources()，无其他逻辑

private:
    graph::GraphicsContext* graphics_context = nullptr;
    graph::Primitive*       shared_unit_square_primitive = nullptr;
};
```

> 不要把 `texture_array` 之类逻辑现在搬过来。Step 3 改 `Sprite2DMaterialBindingSystem` 时再处理域纹理路径。本步只负责"那块共享几何体"。

### 3.4 CMake 接入

`src/ecs/CMakeLists.txt` 找到 `QuadResourcePrepareSystem` 所在 source group，**追加**新文件：

```cmake
set(ECS_SYS_XX_RenderResource_Sprite2DResourcePrepareSystem_FILES
    ${ECS_SOURCE_PATH}/systems/render/Sprite2DResourcePrepareSystem.cpp
    ${ECS_INCLUDE_PATH}/systems/render/Sprite2DResourcePrepareSystem.h
)

# 加入 ECS_SOURCE 列表
list(APPEND ECS_SOURCE ${ECS_SYS_XX_RenderResource_Sprite2DResourcePrepareSystem_FILES})

# 给 IDE 分组
source_group("ECS\\Systems\\XX RenderResource_Sprite2DResourcePrepareSystem"
             FILES ${ECS_SYS_XX_RenderResource_Sprite2DResourcePrepareSystem_FILES})
```

> ⚠️ 之前重构时多次出现 `source_group(...` 缺右括号导致 CMake parse error。**复制旧条目改名时记得保留完整的 `FILES ${...})` 段**。

---

## 4. 验证

### 4.1 编译

```pwsh
cmake --build build --config Debug
```

- 0 error。
- IDE 的"解决方案资源管理器"里能看到 `ECS / Systems / XX RenderResource_Sprite2DResourcePrepareSystem` 分组。

### 4.2 单元自检（无运行时副作用）

由于本步不接入任何 group，运行 `03_BillboardPerspectiveECS`、`01_Billboard` 的行为应当**完全等同 Step 1**。

- [ ] 旧示例图像、Validation、帧率与 Step 1 一致。
- [ ] 用调试断点确认 `Sprite2DResourcePrepareSystem` 没有被任何代码 `RegisterRenderSystem` 调用。

### 4.3 离线 sanity（可选但推荐）

写一个最小测试：

```cpp
// 在 BillboardPerspectiveECS 的 Init() 里临时加几行（验证完删掉）：
auto sys = ecs_context->RegisterRenderSystem<Sprite2DResourcePrepareSystem>();
sys->SetGraphicsContext(GetRenderContext()->GetGraphicsContext());
bool ok = sys->EnsureSharedResources();
LogInfo("[Sprite2D Step2] unit square primitive = %p, ok=%d",
        sys->GetSharedPrimitive(), int(ok));
```

跑一帧，确认日志里 `primitive != nullptr` 且 `ok=1`，然后**删掉这段临时代码**。

---

## 5. 常见坑

- ❌ Vulkan UV 写错（V=0 在底）→ Step 5 跑示例时图片上下颠倒。本步骤如已按上面表格写就 OK。
- ❌ 把 4 顶点 VBO 写成单 buffer（pos+uv 交错）→ ULRE 的 `GraphicsGeometryFactory::CreateGeometry` API 是按属性传独立 buffer 的，写错会触发 `VK_ERROR_INVALID_FORMAT` 或 vertex 错乱。
- ❌ 索引类型用 U32 → 兼容是兼容，但 4 顶点用 U16 更标准；保持一致避免别处 cast。
- ❌ `RegisterGeometry` 之后没 hold primitive 指针 → 工厂可能持有所有权，要确认在系统析构里不重复 delete。沿用 `QuadResourcePrepareSystem` 的所有权模式即可。

---

## 6. 回滚方案

```pwsh
git rm inc/hgl/ecs/systems/render/Sprite2DResourcePrepareSystem.h
git rm src/ecs/systems/render/Sprite2DResourcePrepareSystem.cpp
git restore src/ecs/CMakeLists.txt
```

---

## 7. Step 2 通关条件

- [ ] 全量构建 0 error。
- [ ] CMake configure 0 error / 0 warning（不能有右括号缺失）。
- [ ] 旧 baseline (`03_BillboardPerspectiveECS`, `01_Billboard`) 完全一致。
- [ ] 临时自检日志确认 unit-square primitive 创建成功。
- [ ] 本 .md 末尾追加 `已通过：YYYY-MM-DD by xxx`。
