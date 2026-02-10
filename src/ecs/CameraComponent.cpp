#include<hgl/ecs/CameraComponent.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/ECSComponentRecords.h>
#include<array>

namespace hgl::ecs
{
    namespace
    {
        struct CameraRecord
        {
            std::array<float, 3> position{};
            std::array<float, 3> target{};
            std::array<float, 3> worldUp{};

            float fov = 60.0f;
            float nearPlane = 0.1f;
            float farPlane = 1000.0f;

            float yaw = 0.0f;
            float pitch = 0.0f;
            float roll = 0.0f;

            std::array<float, 3> forward{};
            std::array<float, 3> right{};
            std::array<float, 3> up{};

            int controlMode = 0;

            float distance = 0.0f;
            float minDistance = 0.0f;
            float maxDistance = 0.0f;

            float rotationSensitivity = 0.0f;
            float zoomSensitivity = 0.0f;
            float moveSpeed = 0.0f;

            std::array<float, 2> inputInvert{};

            bool isMainCamera = false;
            bool matrixDirty = false;
        };

        std::array<float, 3> ToArray3(const hgl::math::Vector3f& value)
        {
            return {value.x, value.y, value.z};
        }

        std::array<float, 2> ToArray2(const hgl::math::Vector2f& value)
        {
            return {value.x, value.y};
        }
    }

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

    const char* CameraComponent::GetSerializationType()
    {
        return "Camera";
    }

    bool CameraComponent::SerializeToRecord(const std::shared_ptr<Component>& component,
                                            const std::unordered_map<EntityID, int32_t>&,
                                            ComponentRecord& out_record)
    {
        auto camera = std::dynamic_pointer_cast<CameraComponent>(component);
        if (!camera)
            return false;

        CameraRecord data{};
        data.position = ToArray3(camera->position);
        data.target = ToArray3(camera->target);
        data.worldUp = ToArray3(camera->world_up);
        data.fov = camera->fov;
        data.nearPlane = camera->near_plane;
        data.farPlane = camera->far_plane;
        data.yaw = camera->yaw;
        data.pitch = camera->pitch;
        data.roll = camera->roll;
        data.forward = ToArray3(camera->forward);
        data.right = ToArray3(camera->right);
        data.up = ToArray3(camera->up);
        data.controlMode = static_cast<int>(camera->control_mode);
        data.distance = camera->distance;
        data.minDistance = camera->min_distance;
        data.maxDistance = camera->max_distance;
        data.rotationSensitivity = camera->rotation_sensitivity;
        data.zoomSensitivity = camera->zoom_sensitivity;
        data.moveSpeed = camera->move_speed;
        data.inputInvert = ToArray2(camera->input_invert);
        data.isMainCamera = camera->is_main_camera;
        data.matrixDirty = camera->matrix_dirty;

        out_record.type = GetSerializationType();
        out_record.payload = data;
        return true;
    }

    void CameraComponent::DeserializeFromRecord(const ComponentRecord& record,
                                                Entity* entity,
                                                std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>&)
    {
        const auto& data = std::any_cast<const CameraRecord&>(record.payload);
        auto camera = std::make_shared<CameraComponent>();
        camera->position = hgl::math::Vector3f(data.position[0], data.position[1], data.position[2]);
        camera->target = hgl::math::Vector3f(data.target[0], data.target[1], data.target[2]);
        camera->world_up = hgl::math::Vector3f(data.worldUp[0], data.worldUp[1], data.worldUp[2]);
        camera->fov = data.fov;
        camera->near_plane = data.nearPlane;
        camera->far_plane = data.farPlane;
        camera->yaw = data.yaw;
        camera->pitch = data.pitch;
        camera->roll = data.roll;
        camera->forward = hgl::math::Vector3f(data.forward[0], data.forward[1], data.forward[2]);
        camera->right = hgl::math::Vector3f(data.right[0], data.right[1], data.right[2]);
        camera->up = hgl::math::Vector3f(data.up[0], data.up[1], data.up[2]);
        camera->control_mode = static_cast<CameraComponent::ControlMode>(data.controlMode);
        camera->distance = data.distance;
        camera->min_distance = data.minDistance;
        camera->max_distance = data.maxDistance;
        camera->rotation_sensitivity = data.rotationSensitivity;
        camera->zoom_sensitivity = data.zoomSensitivity;
        camera->move_speed = data.moveSpeed;
        camera->input_invert = hgl::math::Vector2f(data.inputInvert[0], data.inputInvert[1]);
        camera->is_main_camera = data.isMainCamera;
        camera->matrix_dirty = data.matrixDirty;

        camera->camera_data = nullptr;
        camera->camera_info = nullptr;
        camera->viewport_info = nullptr;
        camera->camera_ubo = nullptr;

        entity->AddComponentInstance(camera);
    }
}//namespace hgl::ecs
