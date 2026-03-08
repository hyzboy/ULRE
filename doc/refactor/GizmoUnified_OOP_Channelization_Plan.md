# GizmoUnified 面向对象化重构计划（Asset-Only）

## 1. 背景与问题陈述

当前 `src/SceneGraph/gizmo/GizmoUnified.cpp` 已完成 Asset-only 收敛，但仍存在以下可维护性问题：

1. 单文件职责过重：构建、拾取、拖拽、模式切换、回调同步集中在一个实现体。
2. 三类 Gizmo（Move/Rotate/Scale）并列字段与分支较多：阅读时需要频繁跨段追踪。
3. 状态聚合过度：按模式分离的数据仍部分堆叠在统一结构中，后续扩展模式易引发耦合。
4. 行为分发依赖 `switch`：主流程对具体模式知道太多细节，不利于演进。

目标是将系统从“统一过程式调度 + 模式分支”过渡为“通道对象化 + 统一接口调度”。

---

## 2. 设计目标

### 2.1 主目标

1. `GizmoUnified.cpp` 变为轻量装配与外部 API 适配层。
2. Move/Rotate/Scale 各自拥有独立状态、视觉构建、拾取与拖拽实现。
3. 减少主循环中的模式 `switch`，通过多态接口完成调度。
4. 在不改变现有行为的前提下完成重构（优先结构，不优先功能变化）。

### 2.2 非目标（本次不做）

1. 不引入新交互模式（如 Screen-space Move）。
2. 不改变资产定义协议（AssetWorldId、OverrideRef 等保持不变）。
3. 不重写数学算法（先迁移，后优化）。

---

## 3. 新结构草案

## 3.1 目录结构（建议）

```text
src/SceneGraph/gizmo/
  GizmoUnified.cpp                      # 对外 API + 控制器装配
  GizmoController.h
  GizmoController.cpp

  channels/
    IGizmoChannel.h
    GizmoChannelCommon.h                # 公共结构体/工具
    MoveGizmoChannel.h
    MoveGizmoChannel.cpp
    RotateGizmoChannel.h
    RotateGizmoChannel.cpp
    ScaleGizmoChannel.h
    ScaleGizmoChannel.cpp
```

说明：若短期不希望新增 `.h/.cpp` 太多，也可先用 `.inl` 过渡，最终目标仍是按类拆分。

## 3.2 核心对象关系

1. `GizmoController`（或保留名 `GizmoECS` 作为壳）
   - 持有 root/target/callback 等全局协作状态。
   - 持有 3 个通道对象实例。
   - 负责模式映射与生命周期（Create/Destroy/Update）。
2. `IGizmoChannel`（统一接口）
   - 约束每个通道必须实现：构建、显隐、hover、begin/drag/end、同步。
3. `MoveGizmoChannel` / `RotateGizmoChannel` / `ScaleGizmoChannel`
   - 各自维护专属数据与算法。

---

## 4. 关键接口设计

## 4.1 通道上下文

```cpp
struct GizmoFrameInput {
    math::Vector2i mouse_coord;
    const CameraInfo* camera_info = nullptr;
    const ViewportInfo* viewport_info = nullptr;
    hgl::ecs::InputSystem* input_system = nullptr;
    bool left_down = false;
    bool left_pressed = false;
    bool left_released = false;
};

struct GizmoTransformSnapshot {
    math::Vector3f position;
    glm::quat rotation;
    math::Vector3f scale;
};
```

## 4.2 通道接口（示意）

```cpp
class IGizmoChannel {
public:
    virtual ~IGizmoChannel() = default;

    virtual void BuildVisual(hgl::ecs::Entity* parent) = 0;
    virtual void SetVisible(bool visible) = 0;
    virtual void SyncFromRoot(const GizmoTransformSnapshot& root) = 0;
    virtual void UpdateHover(const GizmoFrameInput& in) = 0;
    virtual void BeginDrag(const GizmoFrameInput& in,
                           const GizmoTransformSnapshot& start) = 0;
    virtual void Drag(const GizmoFrameInput& in,
                      GizmoTransformSnapshot& inout_root) = 0;
    virtual void EndDrag(const GizmoFrameInput& in) = 0;

    virtual bool SupportsMode(GizmoMode mode) const = 0;
    virtual bool IsDragging() const = 0;
};
```

备注：实际函数签名可根据当前代码习惯微调，但建议保持“输入上下文 + 状态快照”模型。

---

## 5. 数据归属重组

从控制器下沉到通道（示例）：

1. `move_entity / rotate_entity / scale_entity` -> 各通道私有。
2. `move_asset_instance / rotate_asset_instance / scale_asset_instance` -> 各通道私有。
3. `move_primitives / rotate_primitives / scale_primitives` -> 各通道私有。
4. `asset_drag.ChannelState move/rotate/scale` -> 各通道内部 drag state。

控制器保留：

1. root、root_transform、target_entity、on_changed。
2. 当前模式与全局策略（如 `allow_negative_scale`、`fixed_pixel_diameter`）。
3. 通道容器与模式路由。

---

## 6. 分阶段实施计划

## Phase 0: 基线冻结（0.5 天）

1. 保留当前可编译版本作为重构基线。
2. 记录 smoke tests 基线输出。
3. 新增临时迁移分支。

产出：可回滚稳定点。

## Phase 1: 引入控制器与通道骨架（1 天）

1. 新建 `IGizmoChannel`、`GizmoController`（或在 `GizmoECS` 外包一层）。
2. 只实现空通道 + 当前逻辑透传（行为不变）。
3. 保证编译与测试通过。

验收：接口落地，功能无回归。

## Phase 2: 数据归属迁移（1-2 天）

1. 将三类并列字段迁入各通道对象。
2. 控制器仅保留全局协作状态。
3. 主循环仍允许保留少量 `switch`（过渡期）。

验收：结构显著瘦身，行为不变。

## Phase 3: 行为归属迁移（2 天）

1. 将 Build/Highlight/Pick/Drag 的三类实现迁入对应通道类。
2. `UpdateTransformGizmo` 改为“选中 active channel + 调用统一接口”。
3. 删除 `switch(mode)` 的主流程细节分支。

验收：主流程仅做路由与同步。

## Phase 4: 公共逻辑下沉与重复消除（1 天）

1. 把通用工具函数放入 `GizmoChannelCommon`。
2. 清理重复参数与中间状态。
3. 整理 include 关系，减少互相可见性。

验收：可读性提升，函数职责边界清晰。

## Phase 5: 稳定化与文档收尾（0.5-1 天）

1. 运行全量编译与 smoke tests。
2. 补充设计文档与迁移说明。
3. 清理无用旧文件与临时兼容代码。

验收：文档和代码一致，回归通过。

---

## 7. 验收标准

1. 编译通过：`Build_CMakeTools` 无报错。
2. 测试通过：
   - `ECS_GizmoAssetBackendSmoke.exe` PASS
   - `ECS_GizmoTransformParitySmoke.exe` PASS
3. 结构指标：
   - `GizmoUnified.cpp` 不再包含 Move/Rotate/Scale 具体算法实现。
   - 模式相关主流程分支显著减少（路由级别可接受）。
4. 可维护性指标：
   - 每个通道代码可独立阅读、定位、调试。

---

## 8. 风险与缓解

1. 风险：迁移期间状态同步错位（root 与 target 不一致）。
   - 缓解：统一 `Snapshot` 结构，拖拽开始/结束有明确边界。
2. 风险：拾取与高亮行为细节变化。
   - 缓解：先“搬运不改算法”，最后统一做行为对比。
3. 风险：include/cpp 拆分导致链接或可见性问题。
   - 缓解：优先 `.cpp` 明确编译单元，谨慎控制 `static` 与声明范围。

---

## 9. 执行检查清单（开发用）

- [ ] 建立 `IGizmoChannel` 与通道类骨架
- [ ] 控制器持有通道容器并完成模式路由
- [ ] Move 数据与逻辑迁移完成
- [ ] Rotate 数据与逻辑迁移完成
- [ ] Scale 数据与逻辑迁移完成
- [ ] 删除主流程中三类具体算法分支
- [ ] 编译与 smoke tests 通过
- [ ] 清理遗留字段/函数
- [ ] 文档与实际结构对齐

---

## 10. 建议提交策略

建议按阶段提交，便于回滚与评审：

1. `refactor(gizmo): introduce channel interface and controller skeleton`
2. `refactor(gizmo): move channel-owned data out of GizmoUnified`
3. `refactor(gizmo): migrate move/rotate/scale behavior into channel classes`
4. `refactor(gizmo): simplify unified update loop and remove mode switches`
5. `docs(gizmo): add oop channelization architecture and migration notes`

---

## 11. 结论

本计划核心是“把模式分支转化为对象边界”。

短期收益：`GizmoUnified.cpp` 大幅瘦身，阅读路径清晰。
中期收益：扩展新模式时不再改动主调度核心。
长期收益：交互、渲染、拾取逻辑可按通道独立演进，降低耦合与回归风险。
