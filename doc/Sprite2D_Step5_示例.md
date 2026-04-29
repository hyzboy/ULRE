# Sprite2D 迁移 — Step 5：示例 `03_Sprite2DPerspectiveECS`

> 状态：第一个真正用上 Sprite2D 全栈的示例；旧 03_BillboardPerspectiveECS 仍保留共存
> 风险等级：**高（关键验收点）**
> 预计耗时：1–2 天
> 关键原则：在新示例里把所有"破坏性新能力"全跑一遍；旧示例零侵入。

---

## 0. 目标

新建 `example/Basic/Sprite2DTest_use_ECS.cpp`（注册名 `03_Sprite2DPerspectiveECS`），覆盖以下全部场景，每条都用 RenderDoc 抓帧 + 截图归档：

| 场景 | 验收点 |
|---|---|
| ① AxisLocked + fixed_size | 屏幕固定像素，相机移动时屏幕大小不变；图像方向正向 |
| ② CameraFacing + world_size | 世界尺寸朝相机；近大远小；图像正向 |
| ③ pivot ≠ (0.5,0.5) | 锚在底部时贴片随相机旋转的"脚"不动 |
| ④ rotation ≠ 0 | 绕屏幕法线旋转，pivot 为旋转中心 |
| ⑤ tint | 颜色乘到贴图上正确显示 |
| ⑥ Texture2DArray 域批处理 | 多 sprite 同 domain → **1 drawcall** |
| ⑦ 切换模式 | 运行时按 `[1] [2] [3]` 在三种 pipeline preset 间切换 |

---

## 1. 前置条件

- Step 1–4 全部通关；Sprite2D group 可手动 install。
- 旧 `03_BillboardPerspectiveECS` 仍可作为 baseline 对比。

---

## 2. 涉及文件

| 路径 | 改动 |
|---|---|
| `example/Basic/Sprite2DTest_use_ECS.cpp` | **新建** |
| `example/Basic/CMakeLists.txt` | 注册新 target `03_Sprite2DPerspectiveECS` |
| 已有 Billboard 示例 | **不动** |

> 共享几何 / 资源类（`BillboardIconECSBase` 等）**不复用**。新示例尽量自包含，避免再次踩"基类隐式依赖旧系统"的坑。

---

## 3. 执行步骤

### 3.1 示例骨架

```cpp
// example/Basic/Sprite2DTest_use_ECS.cpp
#include<hgl/Framework.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/DefaultSystems.h>
#include<hgl/ecs/components/Sprite2DComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/support/sprite2d/Sprite2DRenderPipelineGroup.h>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

class Sprite2DPerspectiveECSApp : public CameraAppFramework
{
    ECSContext* ecs_context = nullptr;
    Entity*     camera_entity = nullptr;
    std::vector<Entity*> sprites;

public:
    bool Init() override
    {
        SetClearColor(Color4f(0.15f, 0.15f, 0.18f, 1.0f));

        ecs_context = GetECSContext();
        if (!ecs_context) return false;

        // —— Sprite2D group (opt-in) ——
        if (!ecs::EnsureSystemGroupSystems(ecs_context, "Sprite2D", GetSwapchainRenderTarget()))
            return false;

        // —— Camera ——
        if (!InitCamera()) return false;

        // —— Sprites ——
        if (!CreateSpriteGrid()) return false;

        std::cout << "[Sprite2D] Keys: [1]=AxisLocked Fixed  [2]=CameraFacing World  [3]=Rotated\n";
        return true;
    }

    void Tick(double delta) override { /* 按键切换 pipeline preset */ }

private:
    bool InitCamera()
    {
        if (!ecs_context->EnsureCameraSystem()) return false;
        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        auto cam = camera_entity->AddComponent<CameraComponent>();
        cam->control_mode   = CameraComponent::ControlMode::ViewModel;
        cam->target         = math::Vector3f(0, 0, 0);
        cam->distance       = 30.0f;
        cam->yaw            = 45.0f;
        cam->pitch          = -25.0f;
        cam->is_main_camera = true;
        cam->matrix_dirty   = true;

        cam->camera_data    = GetCamera();
        cam->camera_info    = const_cast<graph::CameraInfo*>(GetCameraInfo());
        cam->viewport_info  = GetViewportInfo();
        return true;
    }

    bool CreateSpriteGrid()
    {
        // 加载 lena.Tex2D，分别按 ①②③④⑤⑥ 摆 6 类 sprite。
        // 每类一个 Entity，挂 TransformComponent + Sprite2DComponent。
        // —— 详细代码略，按上表 7 个验收场景写出对应 6+1 个 entity。
        return true;
    }
};

int os_main(int argc, os_char** argv)
{
    return RunFramework<Sprite2DPerspectiveECSApp>(
        OS_TEXT("Sprite2D Perspective ECS"), argc, argv, 1280, 720);
}
```

### 3.2 关键场景实现细节

#### ① AxisLocked + fixed_size

```cpp
auto e = ecs_context->CreateEntity<Entity>("AxisLockedFixed");
auto t = e->AddComponent<TransformComponent>(Mobility::Static);
t->SetLocalPosition(glm::vec3(0, 0, 0));

auto s = e->AddComponent<Sprite2DComponent>();
s->SetFixedSize(true);
s->SetPixelSize({256, 256});
s->SetPivot({0.5f, 0.5f});
s->SetRotation(0.0f);
s->SetTexturePath(OS_TEXT("res/image/lena.Tex2D"));
```

#### ② CameraFacing + world_size

```cpp
s->SetFixedSize(false);
s->SetWorldSize({4.0f, 4.0f});   // 4 米见方
```

#### ③ Pivot 非中心（脚踩在 anchor 上）

```cpp
s->SetPivot({0.5f, 1.0f});       // pivot 在贴片底边中点
// 实体 anchor 在地面 y=0；预期：贴片"长"在地面上，旋转相机时脚不动
```

#### ④ Rotation

```cpp
s->SetRotation(glm::radians(30.f));
```

#### ⑤ Tint

```cpp
s->SetTint({255, 100, 100, 255}); // 红色 tint
```

#### ⑥ Texture2DArray 域

```cpp
s->SetDomainTag("sprite_atlas_A");  // 同 tag 的多 sprite → 单 drawcall
// 创建 N=20 个 sprite 都用同一 domain_tag，用 RenderDoc 确认 cmd buffer 里只有 1 个 vkCmdDrawIndexed
```

#### ⑦ 运行时切换

```cpp
void Tick(double) override {
    auto* input = ecs_context->GetSystem<InputSystem>().get();
    if (input->IsKeyDown(io::KeyboardButton::_1)) ApplyMode(/*AxisLocked*/);
    if (input->IsKeyDown(io::KeyboardButton::_2)) ApplyMode(/*CameraFacing*/);
    if (input->IsKeyDown(io::KeyboardButton::_3)) ApplyMode(/*Rotated*/);
}

void ApplyMode(/*...*/) {
    for (auto* e : sprites) {
        auto s = e->GetComponent<Sprite2DComponent>();
        if (s) { s->SetFixedSize(...); s->SetWorldSize(...); ... }
    }
}
```

### 3.3 CMake target

`example/Basic/CMakeLists.txt`：

```cmake
add_basic_example(03_Sprite2DPerspectiveECS Sprite2DTest_use_ECS.cpp)
```

> 用与已有 example 相同的宏；不要新发明 target 命名规则。

---

## 4. 验证（每条都要有截图 + RenderDoc 抓帧归档）

### 4.1 视觉

- [ ] ① AxisLocked：相机推近拉远，sprite 屏幕像素尺寸不变。
- [ ] ② CameraFacing：相机推近 → sprite 变大；旋转相机 → sprite 始终面向相机。
- [ ] ③ Pivot：相机绕场景旋转时，pivot 在底部的 sprite 的"脚"不离地面。
- [ ] ④ Rotation：visible 旋转 30°；切换 pivot 后旋转中心相应改变。
- [ ] ⑤ Tint：贴图带红色调；切到白色 tint 恢复原色。
- [ ] ⑥ Domain：N=20 sprite，RenderDoc EID 列表里只有 1 个 `vkCmdDrawIndexed indexCount=120(=20×6)`。
- [ ] ⑦ 按键 `1/2/3` 切换平滑，无 flicker / Validation 报错。

### 4.2 性能

- [ ] 与旧 `03_BillboardPerspectiveECS` 同等 sprite 数下，CPU/GPU 时间在 ±10% 以内。
- [ ] mi_data SSBO 大小 = N × 32B（用 RenderDoc 检查）。

### 4.3 Validation

- [ ] 全程 0 Vulkan validation error / warning。
- [ ] ECS profiler 不报"system without dependencies ready"。

---

## 5. 常见坑（基于前次重构血泪）

- ❌ **camera UBO 全 0** → 检查 `Sprite2DRenderPipelineGroup::Install()` 是否调用了 `EnsureCoreEcsSystems()`（Step 4 §5 已强调）。
- ❌ **图像上下颠倒** → Step 2 的 unit-square UV 必须 V=0 在顶部；AxisLocked variant 如有 NDC Y 反转，需要在 `main_forward_sprite2d_fixed.vert.glsl` 里 `out_uv = vec2(in_texcoord.x, 1.0 - in_texcoord.y);` 补偿（用 dynamic 路径作为对照基准）。
- ❌ **Sprite 不显示** → `EnsureSystemGroupSystems(..., "Sprite2D", ...)` 调用时机太早（render context 还没准备好）。建议放在 `Init()` 里所有 ECS 准备好之后、camera 之前。
- ❌ **多 sprite 没合批** → `domain_tag` 必须完全相同（区分大小写）；纹理 channel hint 也要相同。
- ❌ **Pivot 与 TransformComponent 混淆** → pivot 只在 mesh 局部偏移，不要试图用 pivot 实现"实体在场景中的锚点"，那是 TransformComponent 的责任。
- ❌ **AxisLocked 当 anchor 在相机背后**：`clip.w < 0` 时 NDC offset 公式会反向，sprite 会出现在屏幕另一侧。可加 `if (clip.w <= 0) { gl_Position = vec4(2,2,2,1); return; }` 把背后的 sprite 剔除。

---

## 6. 回滚方案

```pwsh
git rm example/Basic/Sprite2DTest_use_ECS.cpp
git restore example/Basic/CMakeLists.txt
```

---

## 7. Step 5 通关条件

- [ ] 7 个验收场景全部 OK，截图归档到 `doc/screenshots/sprite2d_step5/`。
- [ ] 旧 `03_BillboardPerspectiveECS` 仍可跑，作为对比基准。
- [ ] RenderDoc 抓帧确认：1 个 domain → 1 个 drawcall；mi_data 偏移与 GLSL 匹配。
- [ ] 没有任何 Validation error / warning。
- [ ] 本 .md 末尾追加 `已通过：YYYY-MM-DD by xxx`，并附截图相对路径。
