#pragma once

#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/type/String.h>

#include<cstdint>
#include<memory>

namespace hgl::ecs
{
    class ECSContext;
    class CameraInfoStorage;

    /**
     * CameraSlotGuard —— 相机槽的 **RAII 拥有者**（C3）。
     *
     * 契约（doc/world-addresses-and-camera-model-plan.md §0.2 / §2）：
     * 「相机 = 世界相机存储里的一个槽 + 一个拥有者」。灯光相机 / 镜子相机 / 系统内建相机
     * 这类**不被实体组件注册表持有**的相机，由它的拥有者（通常是某个系统）持一个 guard：
     * 构造时向本世界相机行存储申请槽，析构（或 `Reset()`）时归还。
     *
     * - 申请失败（槽耗尽 / 存储未就绪 / 世界为空）：保持 invalid 并**报错留痕**，绝不静默；
     *   `BindTo()` 对这种相机什么都不做 ⇒ 相机保持 `kInvalidSlot` ⇒ 发布时被跳过并告警
     *   （fail-fast：宁可这个相机不出图，也不能按越界行号写到别的世界去）。
     * - 槽唯一 ⇒ **不可拷贝，只可移动**。
     * - 只持存储的**弱引用**：世界先销毁（或从未 Initialize）时 `lock()` 失败 ⇒ 析构自动 no-op，
     *   所以 guard 作为拥有者成员、比世界活得久也安全（**不需要**任何"世界销毁时通知 guard"的注册表：
     *   早先的注册表 + Detach 方案在"世界从未 Initialize ⇒ Shutdown 走早退分支"时会留下悬垂指针）。
     *
     * 实体/作者持有的相机不用 guard：它们的槽由 `CameraSystem::EnsureCameraSlot` 认领、
     * 由 `CameraComponent` 析构挂钩归还（见 `CameraComponent::slot_releaser`）。
     */
    class CameraSlotGuard
    {
    public:

        CameraSlotGuard() = default;

        /// 向 `world` 申请一个相机槽；`owner` 只用于日志定位（如 "AutoCSMLightCamera"）
        CameraSlotGuard(ECSContext *world, const AnsiString &owner);
        ~CameraSlotGuard();

        CameraSlotGuard(CameraSlotGuard && other) noexcept;
        CameraSlotGuard &operator=(CameraSlotGuard && other) noexcept;

        CameraSlotGuard(const CameraSlotGuard &) = delete;
        CameraSlotGuard &operator=(const CameraSlotGuard &) = delete;

        /// 归还槽（幂等；存储已销毁时只清自己的状态）
        void Reset();

        bool IsValid() const { return slot != CameraComponent::kInvalidSlot; }
        uint32_t GetSlot() const { return slot; }

        /// 把槽号绑给相机（相机本身由拥有者持有）；未持槽 ⇒ 不碰相机，返回 false
        bool BindTo(CameraComponent *camera) const;

    private:

        /// 本世界相机行存储的弱引用（世界销毁后 lock 失败 ⇒ 归还路径自动失效）
        std::weak_ptr<CameraInfoStorage> storage;

        /// 认领槽的**世界身份**（只做相等比较、不解引用；与 `CameraComponent::world_owner` 同源）
        const void *world_identity = nullptr;

        /// 申请者名字（只为日志：申请/归还是对称的一条生命周期痕迹，便于定位"谁漏还了槽"）
        AnsiString owner_name;

        uint32_t slot = CameraComponent::kInvalidSlot;
    };
}//namespace hgl::ecs
