#pragma once

#include<hgl/ecs/core/Component.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/core/ComponentRecords.h>
#include<hgl/type/UnorderedMap.h>
#include<glm/glm.hpp>

namespace hgl::ecs
{
    /**
     * FacingTransformComponent - Configuration for face-towards rotation behavior
     *
     * This component specifies HOW an entity should be oriented towards a target.
     * The actual rotation calculation is performed by FacingTransformSystem.
     *
     * Facing Modes:
     * - LookAtCamera: Always face the camera (for billboards, labels, etc.)
     * - LookAtTarget: Face towards a specific world position
    * - BillboardY: Only rotate around Y axis, top-to-camera (partial billboard)
    * - BillboardZ: Z-up billboard, only rotates around Z axis (yaw in XY plane)
     *
     * Use Cases:
     * - Billboards
     * - Particle systems with oriented particles
     * - Dynamic UI labels
     * - Indicator symbols
     * - Sprite animations that need camera awareness
     *
     * Usage Example:
     *     auto facing = entity->AddComponent<FacingTransformComponent>();
     *     facing->SetFacingMode(FacingMode::LookAtCamera);
     *     // FacingTransformSystem will automatically update rotation each frame
     */

    enum class FacingMode : uint8_t
    {
        LookAtCamera = 0,    ///< Face towards the camera (full rotation)
        LookAtTarget = 1,    ///< Face towards a specific world position
        BillboardY = 2,      ///< Only rotate around Y axis, but face camera from top
        BillboardZ = 3,      ///< Z-up billboard, only rotates around Z axis (yaw in XY plane)
    };

    class FacingTransformComponent : public Component
    {
    private:

        FacingMode facing_mode = FacingMode::LookAtCamera;
        glm::vec3 target_position = glm::vec3(0.0f);      ///< Used when facing_mode == LookAtTarget
        float rotation_speed = 1.0f;                       ///< Speed factor for smooth rotation (1.0 = instant)
        bool enabled = true;                               ///< Can disable facing without removing component

    public:

        explicit FacingTransformComponent(const std::string& name = "FacingTransform")
            : Component(name)
        {
        }

        virtual ~FacingTransformComponent() = default;

    public:

        // Facing mode control
        void SetFacingMode(FacingMode mode) { facing_mode = mode; }
        FacingMode GetFacingMode() const { return facing_mode; }

        // Target position (used for LookAtTarget mode)
        void SetTargetPosition(const glm::vec3& pos) { target_position = pos; }
        const glm::vec3& GetTargetPosition() const { return target_position; }

        // Rotation smoothing
        void SetRotationSpeed(float speed) { rotation_speed = glm::max(0.0f, speed); }
        float GetRotationSpeed() const { return rotation_speed; }

        // Enable/disable facing without removing component
        void SetEnabled(bool e) { enabled = e; }
        bool IsEnabled() const { return enabled; }

    public:

        // Component lifecycle
        void OnAttach() override;
        void OnUpdate(float deltaTime) override;
        void OnDetach() override;

        static const char* GetSerializationType();
        static bool SerializeToRecord(const std::shared_ptr<Component>& component,
                                      const hgl::UnorderedMap<EntityID, int32_t>& entity_index,
                                      ComponentRecord& out_record);
        static void DeserializeFromRecord(const ComponentRecord& record,
                                          Entity* entity,
                                          std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>& pending_parents);
    };
}//namespace hgl::ecs
