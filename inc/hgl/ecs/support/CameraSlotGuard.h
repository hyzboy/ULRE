#pragma once

#include<hgl/ecs/support/CameraInfoStorage.h>
#include<hgl/type/String.h>

#include<cstdint>
#include<memory>

namespace hgl::ecs
{
    class ECSContext;

    /**
     * CameraSlotGuard —— **系统内建相机**的槽宿主（RAII 持有）。
     *
     * 契约（doc/world-addresses-and-camera-model-plan.md §0.2 / §2）：
     * 「相机 = 世界相机表里的一个槽 + 一个宿主」。灯光相机 / 镜子相机 / 系统内建相机这类
     * **不被实体组件注册表持有**的相机，由它的拥有者（通常是某个系统）持一个 guard：
     * 构造时向本世界相机表申请槽并把相机绑为宿主，析构（或 `Reset()`）时归还。
     *
     * - 申请失败（槽耗尽 / 行池未就绪 / 世界为空 / 相机为空）：保持 invalid 并**报错留痕**，绝不静默；
     *   相机保持"无槽"（世界侧反查不到）⇒ 发布时被跳过并告警
     *   （fail-fast：宁可这个相机不出图，也不能按越界行号写到别的世界去）。
     * - 槽唯一 ⇒ **不可拷贝，只可移动**。
     * - 只持**行池**的弱引用：世界先销毁（或从未 Initialize）时 `lock()` 失败 ⇒ 析构自动 no-op，
     *   所以 guard 作为拥有者成员、比世界活得久也安全（**不需要**任何"世界销毁时通知 guard"的注册表：
     *   早先的注册表 + Detach 方案在"世界从未 Initialize ⇒ Shutdown 走早退分支"时会留下悬垂指针）。
     *
     * 与「实体 / 作者持有的相机」的区别只在**宿主是谁**：实体相机的槽账目同样是
     * `CameraInfoStorage::slot_owners` 里的一条弱引用，宿主是"持有该相机的 shared_ptr"
     * （组件注册表 / 作者），相机销毁 ⇒ 弱引用失效 ⇒ 槽自动空出，不需要任何归还挂钩。
     */
    class CameraSlotGuard
    {
    public:

        CameraSlotGuard() = default;

        /// 向 `world` 的相机表申请一个普通槽，并把 `camera` 绑为该槽的宿主（同时写相机的世界标记）；
        /// `owner` 只用于日志定位（如 "AutoCSMLightCamera"）。申请失败 ⇒ `IsValid()==false`（报错留痕）。
        CameraSlotGuard(ECSContext *world, const std::shared_ptr<CameraComponent> &camera, const AnsiString &owner);
        ~CameraSlotGuard();

        CameraSlotGuard(CameraSlotGuard && other) noexcept;
        CameraSlotGuard &operator=(CameraSlotGuard && other) noexcept;

        CameraSlotGuard(const CameraSlotGuard &) = delete;
        CameraSlotGuard &operator=(const CameraSlotGuard &) = delete;

        /// 归还槽（幂等；行池已销毁时只清自己的状态）
        void Reset();

        bool IsValid() const { return slot != CameraInfoStorage::INVALID_SLOT; }
        uint32_t GetSlot() const { return slot; }

    private:

        /// 本世界相机行池的弱引用（世界销毁后 lock 失败 ⇒ 归还路径自动失效）
        std::weak_ptr<CameraInfoStorage> storage;

        /// 认领槽的**世界身份**（只做相等比较、不解引用；与 `CameraComponent::world_owner` 同源）
        const void *world_identity = nullptr;

        /// 申请者名字（只为日志：申请/归还是对称的一条生命周期痕迹，便于定位"谁漏还了槽"）
        AnsiString owner_name;

        uint32_t slot = CameraInfoStorage::INVALID_SLOT;
    };
}//namespace hgl::ecs
