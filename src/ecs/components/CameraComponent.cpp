#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/core/Entity.h>
#include<array>

namespace hgl::ecs
{
    CameraComponent::CameraComponent(const std::string& name)
        : Component(name)
        , position(0.0f, 0.0f, 5.0f)
        , target(0.0f, 0.0f, 0.0f)
        , world_up(0.0f, 0.0f, 1.0f)
        , fov(45.0f)
        , near_plane(0.1f)
        , far_plane(1000.0f)
        , yaw(0.0f)
        , pitch(0.0f)
        , roll(0.0f)
        , forward(1.0f, 0.0f, 0.0f)
        , right(0.0f, 1.0f, 0.0f)
        , up(0.0f, 0.0f, 1.0f)
        , control_mode(ControlMode::Free)
        , distance(10.0f)
        , min_distance(1.0f)
        , max_distance(100.0f)
        , rotation_sensitivity(0.2f)
        , zoom_sensitivity(0.1f)
        , move_speed(5.0f)
        , input_invert(1.0f, 1.0f)
        , camera_data(&local_camera_data)
        , camera_info(&local_camera_info)
        , viewport_info(nullptr)
        , camera_id(CameraComponent::kInvalidSlot)
        , is_main_camera(false)
        , matrix_dirty(true)
    {
    }

    CameraComponent::~CameraComponent()
    {
        // 相机槽是**世界资源**：相机销毁即归还（否则相机实体的反复创建/销毁会把 16 个槽漏空，
        // 满了以后所有新相机都拿不到槽）。0 号槽 = 本世界默认相机专属、不参与分配 ⇒ 无需归还；
        // `kInvalidSlot`（未分配）同样跳过。
        if (slot_releaser && camera_id < kSlotCapacity && camera_id != kDefaultSlot)
            slot_releaser(camera_id);

        slot_releaser = nullptr;    // 先摘挂钩：归还函数可能重入本对象的析构路径
    }
}//namespace hgl::ecs


