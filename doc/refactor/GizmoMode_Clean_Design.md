# Gizmo 系统重新设计方案

## 1. 诊断：当前方案为何不直观

### 根本问题：Phase 2（数据迁移）从未完成

原有计划明确要求 Phase 2（数据迁入通道）先于 Phase 3（行为迁入通道）完成。但实际执行时跳过了 Phase 2 直接做 Phase 3，造成了比原始代码更复杂的结构。

当前状态的结构问题：

```
GizmoECS {               ← 主结构，数据全在这里
    channels[3].entity   ← Move/Rotate/Scale 的实体（还在这里）
    channels[3].primitives
    asset_drag           ← 所有拖拽状态（还在这里）
    asset_drag.move      ← Move 拾取快照（还在这里）
}

MoveGizmoChannel {       ← "通道对象"，但没有任何自己的数据
    RefreshHoverState(GizmoECS* gizmo, ...) // 接收主结构指针
    BeginDragIfNeeded(GizmoECS* gizmo, ...) // 接收主结构指针
}                        // → 通道只是 GizmoECS 上的路由外壳
```

通道接受 `GizmoECS*` 参数意味着通道没有自治性。它只是一个访问 `gizmo->` 字段的路由器，这违背了"通道拥有数据"的目标。

### 直接后果：双重代码路径、逻辑重复

`MoveGizmoChannel::BeginDragIfNeeded` 与 `GizmoController::BeginDragIfNeeded` 的 fallback 分支逻辑几乎完全相同：

```
// 两处几乎相同的代码：
UpdateAssetVisualHover(gizmo, ...)
picked = gizmo->asset_hovered_visual_index
if (picked >= 0) {
    BeginAssetMouseCapture(...)
    GizmoController::StartDragCommonState(...)
    SetAssetActivePickState(...)
    SyncAssetChannelPickFromActive(...)
}
```

通道迁移没有消灭旧代码，只是在 GizmoController 上加了一层壳，再在壳里再复制一遍相同逻辑。

### 追踪一次拖拽开始：5 层调用链

```
UpdateTransformGizmo(gizmo, ...)
  └─ GizmoController::RunDragUpdateStage(gizmo, ...)
       └─ GizmoController::BeginDragIfNeeded(gizmo, ...)
            └─ MoveGizmoChannel::BeginDragIfNeeded(gizmo, ...)
                 └─ GizmoController::StartDragCommonState(gizmo, ...)
                      └─ gizmo->asset_drag.dragging = true （最终写入点）
```

原始手写版本：1 层，直接在主更新函数里写。

### 其他设计缺陷

| 问题 | 表现 |
|---|---|
| 双返回值 | `BeginDragIfNeeded` 同时用 `bool &handled` out参数和 `bool` return，含义不同 |
| `.inl` 过度拆分 | 理解 hover 需要同时阅读 `Lifecycle.inl` + `MoveGizmoChannel.Runtime.inl` |
| 所有方法都是 static | `GizmoController` 实际是伪装成类的命名空间 |
| fallback 永远存在 | channel-first + fallback 模式意味着"通道不是真正的主人" |

---

## 2. 新设计的核心原则

**通道 = 数据 + 行为的完整单元**

- 通道持有自己的实体、视觉基元、拖拽状态、拾取状态
- 通道方法只接受外部上下文（鼠标坐标、相机、视口）——不接受 `GizmoECS*`
- 没有 fallback；通道接管就是完全接管
- 主更新循环不超过 ~30 行，可以直接读懂

---

## 3. 新架构

### 3.1 顶层结构

```
TransformGizmo（重命名自 GizmoECS）
├── ECSContext*  world
├── Entity*      root
├── shared_ptr<TransformComponent> root_transform
├── Entity*      target_entity
├── GizmoChangedCallback on_changed
├── GizmoMode    current_mode
├── float        fixed_pixel_diameter
├── bool         allow_negative_scale
├── bool         root_visible
│
├── MoveGizmoMode   move    ← 持有 move 的全部视觉+交互状态
├── RotateGizmoMode rotate  ← 持有 rotate 的全部视觉+交互状态
└── ScaleGizmoMode  scale   ← 持有 scale 的全部视觉+交互状态
```

主结构不再包含 `GizmoController`、`IGizmoChannel`、或任何 `channels[3]` 数组。

### 3.2 模式对象（以 Move 为例）

```cpp
// ─── 拖拽状态（完全归 MoveGizmoMode 所有）───
struct MoveDragState {
    bool       active                = false;
    bool       mouse_captured        = false;
    InputSystem* capture_input_sys   = nullptr;
    int        picked_axis           = -1;   // 0/1/2=X/Y/Z，-1=无
    int        picked_plane_normal   = -1;   // 0/1/2=YZ/XZ/XY，-1=无
    GizmoShape pick_shape            = GizmoShape::Sphere;
    Vector2i   start_mouse;
    Vector3f   start_position;
    Quat       start_rotation;
    Vector3f   start_scale;
};

class MoveGizmoMode {
public:
    // ─── 视觉实体（本模式持有，不在 GizmoECS 里）───
    Entity*  entity = nullptr;
    shared_ptr<AssetInstanceComponent> asset_instance;
    vector<GizmoVisualPrimitive> primitives;
    int  hovered_index = -1;

    // ─── 拖拽状态（本模式持有，不在 GizmoECS 里）───
    MoveDragState drag;

    // ─── 生命周期 ───
    void BuildVisual(ECSContext* world, Entity* parent, InstanceId iid);
    void DestroyVisual();
    void SetVisible(bool visible);

    // ─── 帧更新（只接受外部上下文，不接受 GizmoECS*）───
    void SyncTransform(Vector3f pos, Quat rot, float pixel_scale);
    void UpdateHover(Vector2i mouse, const CameraInfo*, const ViewportInfo*);

    // ─── 拖拽交互 ───
    bool TryBeginDrag(Vector2i mouse, const CameraInfo*, const ViewportInfo*,
                      InputSystem*, Vector3f start_pos, Quat start_rot, Vector3f start_scale);
    void ApplyDrag(Vector2i mouse, const CameraInfo*, const ViewportInfo*,
                   Vector3f& inout_pos) const;
    void EndDrag();
    void RecoverIfOrphaned(bool left_down);

    bool IsDragging() const { return drag.active; }
};
```

`RotateGizmoMode` 和 `ScaleGizmoMode` 结构相同，各自持有 Rotate/Scale 专有状态（旋转轴、view ring transform、缩放方向等）。

### 3.3 主更新循环（目标形态）

```cpp
void TransformGizmo::Update(Vector2i mouse, const CameraInfo* cam, const ViewportInfo* vp,
                            InputSystem* input_sys,
                            bool left_down, bool left_pressed, bool left_released)
{
    // 1. 非拖拽时同步位置到 target
    if (!IsAnyModeDragging())
        SyncPositionFromTarget(cam, vp);

    // 2. 分发到当前激活模式
    switch (current_mode)
    {
    case GizmoMode::MoveWorld:
    case GizmoMode::MoveLocal:
        DispatchMode(move, mouse, cam, vp, input_sys, left_down, left_pressed, left_released);
        break;
    case GizmoMode::RotateWorld:
    case GizmoMode::RotateLocal:
        DispatchMode(rotate, mouse, cam, vp, input_sys, left_down, left_pressed, left_released);
        break;
    case GizmoMode::ScaleLocal:
        DispatchMode(scale, mouse, cam, vp, input_sys, left_down, left_pressed, left_released);
        break;
    }

    // 3. 提交变化并触发回调
    CommitToTarget();
}

template <typename Mode>
void TransformGizmo::DispatchMode(Mode& mode, Vector2i mouse,
                                  const CameraInfo* cam, const ViewportInfo* vp,
                                  InputSystem* input_sys,
                                  bool left_down, bool left_pressed, bool left_released)
{
    if (!mode.IsDragging())
        mode.UpdateHover(mouse, cam, vp);

    if (left_pressed && !mode.IsDragging())
    {
        const auto& tf = GetCurrentTransform();
        mode.TryBeginDrag(mouse, cam, vp, input_sys,
                          tf.position, tf.rotation, tf.scale);
    }

    if (mode.IsDragging())
    {
        if (left_released)
        {
            mode.EndDrag();
        }
        else
        {
            Vector3f new_pos = root_transform->GetLocalPosition();
            mode.ApplyDrag(mouse, cam, vp, new_pos);
            root_transform->SetLocalPosition(new_pos);
        }
        mode.RecoverIfOrphaned(left_down);
    }
}
```

**主循环共 ~30 行，每一行做一件明确的事，和原始手写版本的可读性对等，但数据封装更好。**

switch 在最顶层，阅读者一眼看到分派路径，不需要追踪虚函数。

---

## 4. 文件组织

```
src/SceneGraph/gizmo/
  TransformGizmo.h            ← 对外 API 头文件（声明 TransformGizmo、GizmoMode 等）
  TransformGizmo.cpp          ← Update 主循环、Create/Destroy、模式切换、CommitToTarget

  modes/
    GizmoModeCommon.h         ← 共用类型：GizmoVisualPrimitive、PickResult、辅助函数声明
    GizmoModeCommon.cpp       ← 共用算法：WorldToScreen、AxisFromIndex、AxisProjectDelta 等

    MoveGizmoMode.h           ← MoveDragState + MoveGizmoMode 类声明
    MoveGizmoMode.Visual.cpp  ← BuildVisual、SetVisible、SyncTransform（视觉层）
    MoveGizmoMode.Input.cpp   ← UpdateHover、TryBeginDrag、ApplyDrag、EndDrag（交互层）

    RotateGizmoMode.h
    RotateGizmoMode.Visual.cpp
    RotateGizmoMode.Input.cpp

    ScaleGizmoMode.h
    ScaleGizmoMode.Visual.cpp
    ScaleGizmoMode.Input.cpp

  GizmoResource.h             ← 不动
  GizmoResource.cpp           ← 不动
  TransformGizmoSystem.cpp    ← 不动（只调用对外 API）
  SunDirectionControlSystem.cpp ← 不动
```

**核心原则：读 Move 行为只需打开 `MoveGizmoMode.Input.cpp`，读 Move 视觉构建只需打开 `MoveGizmoMode.Visual.cpp`，不需要跨文件跳转。**

---

## 5. 迁移路径

### Phase 1：建立 Mode 结构骨架（半天，不改行为）

- 创建 `modes/` 目录
- 创建 `MoveGizmoMode`、`RotateGizmoMode`、`ScaleGizmoMode` 空类（只有字段定义，方法体为空）
- `TransformGizmo` 结构体（内嵌在 `GizmoUnified.cpp`，即现在的 `GizmoECS`）增加成员：`MoveGizmoMode move; RotateGizmoMode rotate; ScaleGizmoMode scale;`
- **现有所有代码继续工作，新字段暂未使用**
- 编译 + smoke：PASS

### Phase 2：Move 数据迁移（半天，不改行为）

把 `GizmoECS` 中 Move 相关的数据字段移入 `MoveGizmoMode`：

| 迁移前（`GizmoECS`） | 迁移后（`MoveGizmoMode`） |
|---|---|
| `channels[0].entity` | `move.entity` |
| `channels[0].asset_instance` | `move.asset_instance` |
| `channels[0].primitives` | `move.primitives` |
| `asset_hovered_visual_index`（Move时） | `move.hovered_index` |
| `asset_drag.dragging`（Move时） | `move.drag.active` |
| `asset_drag.move.pick_index/group/shape` | `move.drag.picked_axis, picked_plane_normal, pick_shape` |
| `asset_drag.start_mouse/position/rotation/scale` | `move.drag.start_*` |
| `asset_drag.mouse_captured`（Move时） | `move.drag.mouse_captured` |
| `asset_drag.capture_input_system`（Move时） | `move.drag.capture_input_sys` |

更新所有引用这些字段的访问站点（用搜索替换）。不改任何算法逻辑。

- 编译 + smoke：PASS

### Phase 3：Move 行为迁移（一天）

把已有的 free function 实现迁入 `MoveGizmoMode` 方法：

- `BuildMoveAssetVisual(GizmoECS*)` → `MoveGizmoMode::BuildVisual(...)`
  - 将 `gizmo->MoveChannel()` 改为 `this->`，其余算法不变
- `ApplyAssetMoveDragChannel(GizmoECS*, ...)` → `MoveGizmoMode::ApplyDrag(...)`
  - 将 `gizmo->asset_drag.*` 改为 `drag.*`，其余算法不变
- hover/begin/end/recover 实现迁入 `MoveGizmoMode` 方法
  - 消除 `GizmoController::BeginDragIfNeeded` 中 Move 的 fallback 分支

- 编译 + smoke：PASS

### Phase 4：Rotate、Scale 数据+行为迁移（一天）

重复 Phase 2+3 的步骤应用于 Rotate 和 Scale。

- 编译 + smoke：PASS

### Phase 5：清理主循环（半天）

- 将 `GizmoUnified.cpp` 中的 `UpdateTransformGizmo` 替换为 §3.3 中的 `DispatchMode` 模式
- 删除 `GizmoController` 类（所有 static 方法已被 Mode 方法取代）
- 删除 `IGizmoChannel`、`MoveGizmoChannel`、`RotateGizmoChannel`、`ScaleGizmoChannel`（已废弃）
- 删除 `GizmoECS::channels` 数组和 `GizmoECS::AssetDragState`（数据已全部迁移）
- 删除 `GizmoController.Update.Lifecycle.inl`、`.Frame.inl`、`.Commit.inl`（逻辑已在 .cpp）
- `GizmoECS` 重命名为 `TransformGizmo`（无数据变化，只改名）

- 编译 + smoke：PASS

---

## 6. 废弃 vs 保留

### 废弃（当前 session 期间产出的）

| 废弃项 | 废弃原因 |
|---|---|
| `IGizmoChannel` 接口 | 接口倒置：传 `GizmoECS*` 而非自持数据 |
| `MoveGizmoChannel` / `RotateGizmoChannel` / `ScaleGizmoChannel` | 接口错误的产物，用 Mode 类替代 |
| `GizmoController` 类（所有 static 方法） | 伪装成类的命名空间，被 Mode 方法和主循环取代 |
| `bool &handled` + return bool 双返回值模式 | 被清晰的 `IsDragging()` 状态检查取代 |
| channel-first + fallback 路由模式 | Mode 完全接管，不存在 fallback |
| `GizmoController.Update.Lifecycle/Frame/Commit.inl` | 过度拆分，被 Mode 方法的 .cpp 文件取代 |
| `GizmoFrameInput` / `GizmoTransformSnapshot`（接口设计用的结构体） | 用直接参数替代 |

### 保留

| 保留项 | 原因 |
|---|---|
| `GizmoUnified.AssetChannels.Move/Rotate/Scale.inl` 中的数学算法 | 算法正确，只改参数来源 |
| `GizmoUnified.AssetVisual.Move/Rotate/Scale.inl` 中的视觉构建逻辑 | 逻辑正确，迁入 Mode.Visual.cpp |
| `GizmoResource.h/.cpp` | 不变 |
| `TransformGizmoSystem.cpp`、`SunDirectionControlSystem.cpp` | 不变，只调用对外 API |
| `GizmoMode` 枚举、`GizmoChangedCallback`、`GizmoTransformChange` | 不变 |
| `GizmoUnified.AssetCore.inl` 中的 helper 函数 | 迁入 `GizmoModeCommon.cpp` |

---

## 7. 与原始手写版本的对比

| 维度 | 原始手写版本 | 当前状态（session 产出） | 新方案 |
|---|---|---|---|
| 追踪 BeginDrag 需要读几个文件 | 1 | 3+ | 1（MoveGizmoMode.Input.cpp）|
| 主更新函数行数 | ~40 | ~30（但隐藏了 5 层调用） | ~30（完全透明） |
| 是否存在重复逻辑 | 少量（3-way switch） | 大量（channel + fallback 各一份） | 无 |
| 数据访问路径 | `gizmo->asset_drag.*` | `gizmo->asset_drag.*`（没变） | `mode.drag.*` |
| 添加新模式的改动范围 | 全局 switch 处各加一个分支 | 全局 switch + IGizmoChannel 新子类 | 新建 Mode 类 + 在主循环加一个 case |

---

## 8. 执行检查清单

- [ ] Phase 1：建立 Mode 结构骨架，编译 + smoke PASS
- [ ] Phase 2：Move 数据迁移，编译 + smoke PASS
- [ ] Phase 3：Move 行为迁移，编译 + smoke PASS
- [ ] Phase 4：Rotate 数据+行为迁移，编译 + smoke PASS
- [ ] Phase 4b：Scale 数据+行为迁移，编译 + smoke PASS
- [ ] Phase 5：清理主循环、删除废弃文件，编译 + smoke PASS
- [ ] 确认 `MoveGizmoMode.Input.cpp` 不含任何 `GizmoECS*` 参数
- [ ] 确认主更新函数无 `switch` 嵌套超过 1 层
- [ ] 确认无 `channel-first + fallback` 模式残留
