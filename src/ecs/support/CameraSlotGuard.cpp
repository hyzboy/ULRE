#include<hgl/ecs/support/CameraSlotGuard.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/support/CameraInfoStorage.h>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    CameraSlotGuard::CameraSlotGuard(ECSContext *world, const AnsiString &owner)
    {
        if (!world)
        {
            GLogError("[CameraSlotGuard] 申请相机槽失败：世界为空（owner=\"%s\"）", owner.c_str());
            return;
        }

        // 只持**弱引用**：世界先销毁（或从未 Initialize）⇒ lock 失败 ⇒ 析构/Reset 自动 no-op
        storage        = world->GetCameraInfoStorageWeak();
        world_identity = world;
        owner_name     = owner;

        auto shared = storage.lock();
        if (!shared || !shared->IsReady())
        {
            GLogError("[CameraSlotGuard] 申请相机槽失败：本世界相机行存储未就绪（owner=\"%s\"）"
                      "——该相机保持未分配，不会被发布",
                      owner.c_str());
            storage.reset();
            return;
        }

        slot = shared->AcquireCameraSlot();

        if (slot == CameraComponent::kInvalidSlot)
        {
            // 容量 16、不扩容（fail-fast）：必须留痕，否则表现为"某个相机安静地不出图"
            GLogError("[CameraSlotGuard] 相机槽耗尽：owner=\"%s\" 未拿到槽（本世界上限 %u 槽）"
                      "——检查是否有相机 / guard 未归还",
                      owner.c_str(), CameraComponent::kSlotCapacity);
        }
        else
        {
            GLogInfo("[CameraSlotGuard] 相机槽已申请：owner=\"%s\" slot=%u", owner.c_str(), slot);
        }
    }

    CameraSlotGuard::~CameraSlotGuard()
    {
        Reset();
    }

    CameraSlotGuard::CameraSlotGuard(CameraSlotGuard && other) noexcept
        : storage(other.storage)
        , world_identity(other.world_identity)
        , owner_name(other.owner_name)
        , slot(other.slot)
    {
        other.storage        = {};
        other.world_identity = nullptr;
        other.owner_name     = {};
        other.slot           = CameraComponent::kInvalidSlot;
    }

    CameraSlotGuard &CameraSlotGuard::operator=(CameraSlotGuard && other) noexcept
    {
        if (this == &other)
            return *this;

        Reset();        // 先归还自己持有的槽（否则 move 赋值会漏槽）

        storage        = other.storage;
        world_identity = other.world_identity;
        owner_name     = other.owner_name;
        slot           = other.slot;

        other.storage        = {};
        other.world_identity = nullptr;
        other.owner_name     = {};
        other.slot           = CameraComponent::kInvalidSlot;

        return *this;
    }

    void CameraSlotGuard::Reset()
    {
        if (slot != CameraComponent::kInvalidSlot)
        {
            // 存储可能随世界一起没了（lock 失败）⇒ 静默跳过：槽账目随存储一起销毁
            if (auto shared = storage.lock())
            {
                if (shared->ReleaseCameraSlot(slot))
                {
                    // 与申请日志对称：申请/归还各一行，漏还槽时一眼能看出是谁
                    GLogInfo("[CameraSlotGuard] 相机槽已归还：owner=\"%s\" slot=%u",
                             owner_name.c_str(), slot);
                }
                else
                {
                    GLogWarning("[CameraSlotGuard] Reset: ReleaseCameraSlot(%u) 被拒（槽已归还过？）", slot);
                }
            }
        }

        storage        = {};
        world_identity = nullptr;
        owner_name     = {};
        slot           = CameraComponent::kInvalidSlot;
    }

    bool CameraSlotGuard::BindTo(CameraComponent *camera) const
    {
        if (!camera)
            return false;

        if (slot == CameraComponent::kInvalidSlot)
            return false;       // 未持槽 ⇒ 相机保持未分配（发布时跳过 + 一次性告警）

        camera->camera_id   = slot;
        camera->world_owner = world_identity;
        return true;
    }
}//namespace hgl::ecs
