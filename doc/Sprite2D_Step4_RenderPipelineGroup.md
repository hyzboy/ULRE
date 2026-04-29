# Sprite2D 迁移 — Step 4：Sprite2DRenderPipelineGroup

> 状态：可独立编译；group 已注册但未被任何示例引用，旧示例仍走 Billboard 路径
> 风险等级：中
> 预计耗时：2–3 小时
> 关键原则：**只增不删**；新 group 与旧 BillboardRenderPipelineGroup 并列存在。

---

## 0. 目标

1. 新建 `Sprite2DRenderPipelineGroup`，把 Step 2、Step 3 的两个系统打包注册：
   - `Sprite2DResourcePrepareSystem`
   - `Sprite2DMaterialBindingSystem`
2. 在 `DefaultSystems.cpp` 的 group 注册表里登记 `"Sprite2D"` group（**不**自动 ensure，必须由示例显式调用）。
3. `BillboardRenderPipelineGroup` 完全不动。

---

## 1. 前置条件

- Step 1–3 全部通关。
- `Sprite2DResourcePrepareSystem`、`Sprite2DMaterialBindingSystem`、`Sprite2DComponent` 编译通过且未被使用。

---

## 2. 涉及文件

| 路径 | 改动 |
|---|---|
| `inc/hgl/ecs/support/sprite2d/Sprite2DRenderPipelineGroup.h` | **新建** |
| `src/ecs/support/sprite2d/Sprite2DRenderPipelineGroup.cpp` | **新建** |
| `src/ecs/core/DefaultSystems.cpp` | **追加** `InstallSprite2DGroup` + 注册 |
| `src/ecs/CMakeLists.txt` | 加 2 个新文件 + `source_group` |
| `inc/hgl/ecs/support/billboard/BillboardRenderPipelineGroup.h` 等旧文件 | **不动** |

---

## 3. 执行步骤

### 3.1 新建 Group 文件

`inc/hgl/ecs/support/sprite2d/Sprite2DRenderPipelineGroup.h`：

```cpp
#pragma once
#include<hgl/ecs/core/Context.h>

namespace hgl::ecs
{
class Sprite2DRenderPipelineGroup
{
public:
    /// 把 Sprite2DResourcePrepareSystem + Sprite2DMaterialBindingSystem 注册到 ctx
    /// 已注册过则幂等返回 true
    static bool Install(ECSContext* ctx, graph::IRenderTarget* default_rt);
};
}
```

`src/ecs/support/sprite2d/Sprite2DRenderPipelineGroup.cpp`：

```cpp
#include<hgl/ecs/support/sprite2d/Sprite2DRenderPipelineGroup.h>
#include<hgl/ecs/systems/render/Sprite2DResourcePrepareSystem.h>
#include<hgl/ecs/systems/render/Sprite2DMaterialBindingSystem.h>
#include<hgl/ecs/core/DefaultSystems.h>

namespace hgl::ecs
{
bool Sprite2DRenderPipelineGroup::Install(ECSContext* ctx, graph::IRenderTarget* default_rt)
{
    if (!ctx) return false;

    // 确保 core systems 已就绪（CameraSystem、RenderTargetSystem、RenderDescriptorBindingSystem ...）
    EnsureCoreEcsSystems(ctx, default_rt);

    auto prepare = ctx->GetSystem<Sprite2DResourcePrepareSystem>();
    if (!prepare)
        prepare = ctx->RegisterRenderSystem<Sprite2DResourcePrepareSystem>();
    if (prepare)
    {
        if (auto* rc = ctx->GetRenderContext())
            prepare->SetGraphicsContext(rc->GetGraphicsContext());
        if (ctx->IsActive())
        {
            prepare->OnDependenciesReady();
            prepare->Initialize();
        }
    }

    auto binding = ctx->GetSystem<Sprite2DMaterialBindingSystem>();
    if (!binding)
        binding = ctx->RegisterRenderSystem<Sprite2DMaterialBindingSystem>();
    if (binding && ctx->IsActive())
    {
        binding->OnDependenciesReady();
        binding->Initialize();
    }

    return prepare && binding;
}
}
```

### 3.2 在 DefaultSystems 注册 group 名

`src/ecs/core/DefaultSystems.cpp` —— 仿照已存在的 `Primitive` / `Text` / `Line` 注册：

```cpp
#include<hgl/ecs/support/sprite2d/Sprite2DRenderPipelineGroup.h>

namespace
{
    bool InstallSprite2DGroup(ECSContext* ctx, graph::IRenderTarget* default_rt)
    {
        return Sprite2DRenderPipelineGroup::Install(ctx, default_rt);
    }

    void RegisterBuiltinSystemGroupInstallers()
    {
        static bool once = false;
        if (once) return;
        once = true;

        auto& reg = SystemGroupRegistry::Get();
        // ... 旧的 Primitive / Text / Line / Billboard 注册（不动） ...
        reg.RegisterGroupInstaller("Sprite2D", InstallSprite2DGroup);
    }
}
```

> ⚠️ **不要**在 `RegisterDefaultEcsSystems()` 里默认调用 `EnsureSystemGroupSystems(ctx, "Sprite2D", default_rt)`。新 group 是 opt-in，由 Step 5 的示例显式调用：

```cpp
ecs::EnsureSystemGroupSystems(ecs_context, "Sprite2D", GetSwapchainRenderTarget());
```

这样旧示例零侵入。

### 3.3 CMake 接入

新增目录 `src/ecs/support/sprite2d/`，类似旧 `src/ecs/support/billboard/`：

```cmake
set(ECS_SUPPORT_SPRITE2D_FILES
    ${ECS_SOURCE_PATH}/support/sprite2d/Sprite2DRenderPipelineGroup.cpp
    ${ECS_INCLUDE_PATH}/support/sprite2d/Sprite2DRenderPipelineGroup.h
)
list(APPEND ECS_SOURCE ${ECS_SUPPORT_SPRITE2D_FILES})
source_group("ECS\\Support\\Sprite2D" FILES ${ECS_SUPPORT_SPRITE2D_FILES})
```

> 再次提醒：`source_group("ECS\\Support\\Sprite2D" FILES ${...})` 必须有右括号且 `FILES` 段完整。前次重构卡在这里多次。

---

## 4. 验证

### 4.1 编译

- 0 error。
- IDE 项目树出现 `ECS / Support / Sprite2D / Sprite2DRenderPipelineGroup.{h,cpp}`。

### 4.2 旧示例零退化

- `01_Billboard`、`03_BillboardPerspectiveECS`：完全等同 Step 3。
- 用 ECS profiler / 启动日志确认：`Sprite2DResourcePrepareSystem`、`Sprite2DMaterialBindingSystem` **未注册**（因为没有调用 `EnsureSystemGroupSystems(..., "Sprite2D", ...)`）。

### 4.3 离线 sanity

临时在 `BillboardPerspectiveECS::Init()` 末尾加：

```cpp
bool ok = ecs::EnsureSystemGroupSystems(ecs_context, "Sprite2D", GetSwapchainRenderTarget());
LogInfo("[Sprite2D Step4] group install ok=%d", int(ok));
```

期望日志：
```
[Sprite2D Step4] group install ok=1
[ECS] Update Begin: Sprite2DResourcePrepareSystem
[ECS] Update End: Sprite2DResourcePrepareSystem
[ECS] Update Begin: Sprite2DMaterialBindingSystem
[ECS] Update End: Sprite2DMaterialBindingSystem
```

旧 Billboard sprite 仍正常显示。**测完删除临时代码**。

---

## 5. 常见坑

- ❌ `Install()` 里忘记调用 `EnsureCoreEcsSystems(ctx, default_rt)` → CameraSystem 没拿到 render context，camera UBO 全为 0（这个坑前次重构踩过，专门记一下）。
- ❌ `IsActive()` 为 true 时忘记调用 `OnDependenciesReady()` 与 `Initialize()` → 系统进入 update 时未初始化，crash 或行为异常。
- ❌ 把 group 自动加到 `RegisterDefaultEcsSystems()` 末尾 → 所有旧示例（即使不用 Sprite2D）都会注册新系统，违反 opt-in 原则；同时如果 Step 3 binding system 还有 bug 会污染所有示例。
- ❌ `SystemGroupRegistry::Get()` 是单例但 `RegisterGroupInstaller` 不幂等的话会被重复注册 → 用 `static bool once = false;` 守护。

---

## 6. 回滚方案

```pwsh
git rm -r inc/hgl/ecs/support/sprite2d
git rm -r src/ecs/support/sprite2d
git restore src/ecs/core/DefaultSystems.cpp
git restore src/ecs/CMakeLists.txt
```

---

## 7. Step 4 通关条件

- [ ] 全量构建 0 error。
- [ ] CMake configure 0 error / 0 warning。
- [ ] 旧示例完全等同 Step 3（默认未启用 Sprite2D group）。
- [ ] sanity 日志确认显式 `EnsureSystemGroupSystems(..., "Sprite2D", ...)` 后两个系统按顺序运行。
- [ ] 本 .md 末尾追加 `已通过：YYYY-MM-DD by xxx`。
