# Gizmo ECS 重构说明

## 概述

参照 `Gizmo3DMove.cpp` 的 ECS 架构，完成了 `Gizmo3DRotate.cpp` 和 `Gizmo3DScale.cpp` 的移植和完善。

## 主要改动

### 1. Gizmo3DRotate.cpp

**架构变更：**
- 从旧的 `SceneNode` 架构迁移到 ECS 架构
- 创建 `GizmoRotateECS` 结构体，包含所有旋转 Gizmo 的状态和组件

**主要特性：**
- **三个轴向圆环**：红色(X)、绿色(Y)、蓝色(Z)
- **白色圆环**：永远面向相机（实现了 GizmoTest.cpp 33-75 行的 TransformBillboard 逻辑）
- **交互支持**：
  - 鼠标悬停高亮
  - 拖拽旋转
  - 角度计算和应用（待完善）

**新增函数：**
```cpp
GizmoRotateECS *CreateGizmoRotateECS(hgl::ecs::ECSContext *world,
                                      const char *name,
                                      const math::Vector3f &position);

void DestroyGizmoRotateECS(GizmoRotateECS *gizmo);

bool GetGizmoRotateECSState(const GizmoRotateECS *gizmo, GizmoRotateECSState &out_state);

void UpdateGizmoRotateECS(GizmoRotateECS *gizmo,
                          const math::Vector2i &mouse_coord,
                          const graph::CameraInfo *camera_info,
                          const graph::ViewportInfo *viewport_info,
                          hgl::ecs::InputSystem *input_system,
                          bool left_down,
                          bool left_pressed,
                          bool left_released);
```

**白色圆环面向相机实现：**
```cpp
// 在 UpdateGizmoRotateECS 中每帧更新白色圆环朝向
if(gizmo->white_torus_transform)
{
    const math::Vector3f gizmo_pos = gizmo->root_transform->GetWorldPosition();
    const glm::quat facing_rotation = CalculateFacingRotation(gizmo_pos, camera_info->view);
    gizmo->white_torus_transform->SetLocalRotation(facing_rotation);
}
```

### 2. Gizmo3DScale.cpp

**架构变更：**
- 从旧的 `SceneNode` 架构迁移到 ECS 架构
- 创建 `GizmoScaleECS` 结构体

**主要特性：**
- **中心立方体**：白色，统一缩放
- **三轴圆柱和立方体**：红色(X)、绿色(Y)、蓝色(Z)
- **双轴调节平面**：位于轴之间的方形调节器
- **交互支持**：
  - 鼠标悬停高亮
  - 拖拽缩放
  - 缩放比例计算（待完善）

**新增函数：**
```cpp
GizmoScaleECS *CreateGizmoScaleECS(hgl::ecs::ECSContext *world,
                                    const char *name,
                                    const math::Vector3f &position);

void DestroyGizmoScaleECS(GizmoScaleECS *gizmo);

bool GetGizmoScaleECSState(const GizmoScaleECS *gizmo, GizmoScaleECSState &out_state);

void UpdateGizmoScaleECS(GizmoScaleECS *gizmo,
                         const math::Vector2i &mouse_coord,
                         const graph::CameraInfo *camera_info,
                         const graph::ViewportInfo *viewport_info,
                         hgl::ecs::InputSystem *input_system,
                         bool left_down,
                         bool left_pressed,
                         bool left_released);
```

### 3. 新增状态结构

在 `Gizmo.h` 中添加：

```cpp
struct GizmoRotateECSState
{
    int cur_axis = -1;        // 当前悬停的轴 (-1 表示无)
    int pick_axis = -1;       // 当前拖拽的轴
    bool dragging = false;    // 是否正在拖拽
    float cur_angle = 0.0f;   // 当前旋转角度
    float pick_angle = 0.0f;  // 拖拽开始时的角度
};

struct GizmoScaleECSState
{
    int cur_axis = -1;        // 当前悬停的轴 (-1 表示无)
    int pick_axis = -1;       // 当前拖拽的轴
    bool dragging = false;    // 是否正在拖拽
    float cur_scale = 1.0f;   // 当前缩放比例
    float pick_scale = 1.0f;  // 拖拽开始时的缩放比例
};
```

## 使用示例

参见 `GizmoUsageExample.cpp`，展示了如何同时使用三种 Gizmo 并在它们之间切换。

### 基本使用流程

```cpp
// 1. 初始化
hgl::ecs::ECSContext *world = GetECSContext();
math::Vector3f position(0, 0, 0);

GizmoRotateECS *gizmo_rotate = CreateGizmoRotateECS(world, "GizmoRotate", position);
GizmoScaleECS *gizmo_scale = CreateGizmoScaleECS(world, "GizmoScale", position);

// 2. 每帧更新
if(gizmo_rotate)
{
    UpdateGizmoRotateECS(gizmo_rotate,
                         mouse_coord,
                         camera_info,
                         viewport_info,
                         input_system,
                         left_down,
                         left_pressed,
                         left_released);
}

// 3. 查询状态
GizmoRotateECSState state;
if(GetGizmoRotateECSState(gizmo_rotate, state))
{
    // 使用状态信息
}

// 4. 清理
DestroyGizmoRotateECS(gizmo_rotate);
DestroyGizmoScaleECS(gizmo_scale);
```

## 待完善功能

### Rotate
- [ ] 完善旋转应用逻辑（目前只计算角度，未应用到对象）
- [ ] 添加多轴旋转支持
- [ ] 优化白色圆环的检测逻辑

### Scale
- [ ] 完善缩放应用逻辑（目前只计算比例，未应用到对象）
- [ ] 添加双轴和三轴缩放支持（通过平面和中心立方体）
- [ ] 添加缩放约束（最小值/最大值）

## 技术细节

### 射线检测
- **Move**: 射线到轴线的最近点距离
- **Rotate**: 射线与圆环平面的交点距离圆心的距离
- **Scale**: 同 Move，使用轴线距离检测

### 材质切换
三个 Gizmo 都实现了统一的材质切换逻辑：
- 悬停时：切换为黄色高亮材质
- 拖拽时：保持黄色高亮
- 其他时：显示轴颜色（红/绿/蓝）

### 输入捕获
使用 `InputSystem` 的鼠标捕获功能：
- `BeginMouseCapture(gizmo)`: 开始拖拽时捕获
- `EndMouseCapture(gizmo)`: 结束拖拽时释放
- `IsMouseCapturedBy(gizmo)`: 检查是否被当前 Gizmo 捕获

## 参考
- Blender 4.x Gizmo 系统
- 原 `GizmoTest.cpp` 33-75 行的 TransformBillboard 实现
