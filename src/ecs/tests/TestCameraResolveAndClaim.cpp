// TestCameraResolveAndClaim.cpp —— §3-⑥ 判据：相机"纯读（Resolve）"与"认领（Ensure）"必须分离
//
// 争议背景（§3-⑥）：`GetActiveCameraComponent()` 名像 Getter，实际会**认领 0 号槽 / 在空世界
// 凭空生成常驻 fallback 相机** ⇒ 只读意图（gizmo 朝向 / 拾取射线 / 诊断）读一次就改了世界。
//
// 判据（红的样子）：
//   段 1 = 空世界纯读后 `GetDefaultCamera()==nullptr` 且相机数 0（老行为：纯读会造出 fallback）
//   段 2 = 有相机世界的纯读**不得**认领（`GetDefaultCamera()` 仍为空；老行为：读取即升格）
//   段 3 = 认领版仍保留"渲染必须有一个相机"的兜底契约（fallback + 0 号槽）
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/components/CameraComponent.h>
#include <hgl/ecs/support/CameraInfoStorage.h>
#include <hgl/ecs/systems/tick/CameraSystem.h>
#include <hgl/graph/ubo/ViewportInfo.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

using namespace hgl;
using namespace hgl::ecs;

namespace
{
    uint32_t CountCameras(ECSContext &ctx)
    {
        ValueArray<CameraComponent *> cameras;
        ctx.GetComponents<CameraComponent>(cameras);
        return static_cast<uint32_t>(cameras.GetCount());
    }
}

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestCameraResolveAndClaim"));

    // ── 段 1：空世界（一个相机都没有）────────────────────────────────────────
    {
        ECSContext world("TestResolveEmptyWorld");

        auto cam_sys = world.RegisterSystem<CameraSystem>();

        if (!cam_sys)
        {
            GLogError(u8"Test 1 Failed: CameraSystem 注册失败");
            return 1;
        }

        if (world.ResolveActiveCameraInfo() != nullptr || cam_sys->ResolveActiveCamera() != nullptr)
        {
            GLogError(u8"Test 1 Failed: 空世界的**纯读**不该解析出相机（应 nullptr）");
            return 1;
        }

        if (world.GetDefaultCamera() != nullptr || CountCameras(world) != 0)
        {
            GLogError(u8"Test 1 Failed: 纯读改动了世界（默认相机=%p / 相机数=%u）",
                      static_cast<const void *>(world.GetDefaultCamera()), CountCameras(world));
            return 1;
        }

        // 认领版：必须兜底生成常驻 fallback 并占 0 号槽（渲染契约不变）
        auto *fallback = cam_sys->EnsureActiveCamera();

        if (!fallback || !world.IsFallbackCamera(fallback))
        {
            GLogError(u8"Test 1 Failed: 认领版必须生成常驻 fallback 相机");
            return 1;
        }

        if (world.GetDefaultCamera() != fallback
         || world.GetCameraSlot(fallback) != CameraInfoStorage::kDefaultCameraSlot)
        {
            GLogError(u8"Test 1 Failed: fallback 未成为 0 号槽宿主");
            return 1;
        }

        // 0 号槽已认领后再纯读：解析到同一 fallback，且不改动
        if (world.ResolveActiveCameraInfo() != &fallback->camera_info)
        {
            GLogError(u8"Test 1 Failed: 0 号槽已认领后，纯读应解析到同一 fallback");
            return 1;
        }

        GLogInfo(u8"Test 1 Passed: 空世界纯读不写世界（nullptr / 默认相机空 / 相机数 0）；"
                  u8"认领版生成 fallback 并占 0 号槽。");
    }

    // ── 段 2：有相机（都没标 is_main_camera）⇒ 纯读只选择不认领 ───────────────
    {
        ECSContext world("TestResolveWithCameras");

        auto cam_sys = world.RegisterSystem<CameraSystem>();

        auto e1 = world.CreateEntity<Entity>("CamA");
        auto e2 = world.CreateEntity<Entity>("CamB");

        auto *cam_a = world.GetOrCreateCamera(e1->GetEntityID());   // 先建 ⇒ EntityID 较小
        auto *cam_b = world.GetOrCreateCamera(e2->GetEntityID());

        if (!cam_a || !cam_b || cam_a == cam_b || CountCameras(world) != 2)
        {
            GLogError(u8"Test 2 Failed: 前置搭建失败（a=%p b=%p 相机数=%u）",
                      static_cast<void *>(cam_a), static_cast<void *>(cam_b), CountCameras(world));
            return 2;
        }

        if (world.GetDefaultCamera() != nullptr)
        {
            GLogError(u8"Test 2 Failed: 前置断言失败 —— 搭建期不该有默认相机");
            return 2;
        }

        const CameraComponent *resolved = cam_sys->ResolveActiveCamera();

        if (resolved != cam_a)
        {
            GLogError(u8"Test 2 Failed: 纯读应选出最小 EntityID 的相机（got=%p want=%p）",
                      static_cast<const void *>(resolved), static_cast<const void *>(cam_a));
            return 2;
        }

        if (world.GetDefaultCamera() != nullptr)
        {
            GLogError(u8"Test 2 Failed: **纯读认领了相机**（默认相机=%p）—— 只读意图不该改世界",
                      static_cast<const void *>(world.GetDefaultCamera()));
            return 2;
        }

        // 认领版：把选中的相机升格为 0 号槽宿主
        auto *ensured = cam_sys->EnsureActiveCamera();

        if (ensured != cam_a || world.GetDefaultCamera() != cam_a)
        {
            GLogError(u8"Test 2 Failed: 认领版未把选中的相机升格为默认相机（ensured=%p default=%p）",
                      static_cast<void *>(ensured), static_cast<void *>(world.GetDefaultCamera()));
            return 2;
        }

        // 认领之后再纯读：仍是它，默认相机不变
        if (cam_sys->ResolveActiveCamera() != cam_a || world.GetDefaultCamera() != cam_a)
        {
            GLogError(u8"Test 2 Failed: 认领后纯读结果/默认相机发生了变化");
            return 2;
        }

        // Simulate swapchain recreation: the new RT owns a different ViewportInfo.
        graph::ViewportInfo old_viewport;
        graph::ViewportInfo new_viewport;
        old_viewport.Set(1280, 720);
        new_viewport.Set(900, 700);

        cam_sys->SetViewportInfo(&old_viewport);
        cam_sys->ForceRefreshSelectedCamera();
        cam_sys->SetViewportInfo(&new_viewport);
        cam_sys->ForceRefreshSelectedCamera();

        if (cam_a->viewport_info != &new_viewport)
        {
            GLogError(u8"Test 2 Failed: resize 后主相机仍绑定旧 ViewportInfo");
            return 2;
        }

        cam_sys->Update(0.0f);
        if (cam_a->viewport_info != &new_viewport || cam_b->viewport_info != &new_viewport)
        {
            GLogError(u8"Test 2 Failed: CameraSystem 更新后相机没有绑定当前 ViewportInfo");
            return 2;
        }

        GLogInfo(u8"Test 2 Passed: 有相机世界的纯读只选择不认领；认领版才升格 0 号槽（两口的解析顺序同源）。");
    }

    GLogInfo(u8"=== Camera resolve-vs-claim contracts passed! ===");
    return 0;
}
