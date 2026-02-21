#pragma once

#include<hgl/ecs/core/System.h>
#include<glm/glm.hpp>

namespace hgl
{
    namespace graph
    {
        class CameraInfo;
    }
}

namespace hgl::ecs
{
    class TransformComponent;
    class FacingTransformComponent;

    /**
     * FacingTransformSystem
     *
     * Calculates rotation for entities that need to face towards a target or camera.
     *
     * This system:
     * - Reads FacingTransformComponent configuration
     * - Computes appropriate rotation quaternion
     * - Updates TransformComponent local rotation
     *
     * Responsibilities:
     * - Convert facing direction to quaternion
     * - Handle smooth rotation transitions
    * - Support multiple facing modes (LookAtCamera, LookAtTarget, BillboardY, BillboardZ)
     *
     * The computed rotation is stored in TransformComponent::local_rotation,
     * which is then used by the rendering pipeline.
     */
    class FacingTransformSystem : public System
    {
    private:

        class ECSContext* world = nullptr;
        const graph::CameraInfo* camera_info = nullptr;

    public:

        FacingTransformSystem(const std::string& name = "FacingTransformSystem");
        ~FacingTransformSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }
        void SetCameraInfo(const graph::CameraInfo* info) { camera_info = info; }

        const graph::CameraInfo* GetCameraInfo() const { return camera_info; }

    public:

        void Update(float deltaTime) override;

    private:

        // Helper methods for rotation calculation
        bool UpdateFacingRotation(FacingTransformComponent* facing,
                                 TransformComponent* transform,
                                 float deltaTime);

        // Specific rotation calculation modes
        bool CalculateLookAtCameraRotation(TransformComponent* transform,
                                          float deltaTime);

        bool CalculateLookAtTargetRotation(TransformComponent* transform,
                                          const glm::vec3& target_pos,
                                          float deltaTime);

        bool CalculateBillboardYRotation(TransformComponent* transform,
                                        float deltaTime);

        bool CalculateBillboardZRotation(TransformComponent* transform,
                                        float deltaTime);
    };
}//namespace hgl::ecs
