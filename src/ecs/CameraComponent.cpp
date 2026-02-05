#include<hgl/ecs/CameraComponent.h>

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
        , camera_data(nullptr)
        , camera_info(nullptr)
        , viewport_info(nullptr)
        , camera_ubo(nullptr)
        , is_main_camera(false)
        , matrix_dirty(true)
    {
    }
}//namespace hgl::ecs
