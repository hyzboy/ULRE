#include<hgl/ecs/systems/render/BillboardRenderSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/BillboardComponent.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/graph/CameraInfo.h>
#include<glm/gtx/quaternion.hpp>
#include<glm/gtx/matrix_decompose.hpp>
#include<iostream>
#include<cmath>

namespace hgl::ecs
{
    BillboardRenderSystem::BillboardRenderSystem(const std::string& name)
        : System(name)
    {
        std::cout << "[BillboardRenderSystem] Constructor called: " << name << std::endl;
    }

    void BillboardRenderSystem::Update(float deltaTime)
    {
        if (!world)
        {
            std::cout << "[BillboardRenderSystem] Update: world is null!" << std::endl;
            return;
        }

        if (!cameraInfo)
        {
            std::cout << "[BillboardRenderSystem] Update: cameraInfo is null!" << std::endl;
            return;
        }

        // Get all entities and iterate through them
        std::vector<Entity*> entities;
        world->GetAllEntities(entities);

        std::cout << "[BillboardRenderSystem] ==> Update called! Found " << entities.size() << " total entities" << std::endl;

        int billboardCount = 0;
        int visibleCount = 0;
        int transformCount = 0;
        int rotatedCount = 0;

        for (Entity* entity : entities)
        {
            if (!entity)
                continue;

            auto billboard = entity->GetComponent<BillboardComponent>();
            if (!billboard)
            {
                continue;
            }

            billboardCount++;
            std::cout << "  [Billboard] Found entity: " << entity->GetName() 
                      << ", Visible: " << (billboard->IsVisible() ? "YES" : "NO") << std::endl;

            if (!billboard->IsVisible())
            {
                std::cout << "    -> Skipped (not visible)" << std::endl;
                continue;
            }

            visibleCount++;

            auto transform = entity->GetComponent<TransformComponent>();
            if (!transform)
            {
                std::cout << "    -> ERROR: No TransformComponent!" << std::endl;
                continue;
            }

            transformCount++;
            
            auto pos = transform->GetLocalPosition();
            auto worldPos = transform->GetWorldMatrix()[3]; // Get translation column
            std::cout << "    -> Position (local): (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
            std::cout << "    -> World position: (" << worldPos.x << ", " << worldPos.y << ", " << worldPos.z << ")" << std::endl;
            std::cout << "    -> Fixed size: " << (billboard->IsFixedPixelSize() ? "YES" : "NO") << std::endl;

            if (billboard->IsFixedPixelSize())
            {
                auto pixel_sz = billboard->GetPixelSize();
                std::cout << "    -> Pixel size: " << pixel_sz.x << "x" << pixel_sz.y << std::endl;
            }
            else
            {
                auto world_sz = billboard->GetWorldSize();
                std::cout << "    -> World size: " << world_sz.x << "x" << world_sz.y << std::endl;
            }

            auto prim = billboard->GetPrimitive();
            std::cout << "    -> Primitive: " << (prim ? "EXISTS" : "NULL") << std::endl;

            // Calculate and apply billboard rotation to face camera
            if (UpdateBillboardRotation(billboard.get(), transform.get(), deltaTime))
            {
                rotatedCount++;
                std::cout << "    -> Rotation updated successfully" << std::endl;
            }
            else
            {
                std::cout << "    -> ERROR: Failed to update rotation" << std::endl;
            }
        }

        std::cout << "[BillboardRenderSystem] <== Update COMPLETE: " 
                  << "Total=" << billboardCount 
                  << ", Visible=" << visibleCount 
                  << ", WithTransform=" << transformCount
                  << ", Rotated=" << rotatedCount << std::endl;
    }

    bool BillboardRenderSystem::UpdateBillboardRotation(BillboardComponent* billboard,
                                                        TransformComponent* transform,
                                                        float deltaTime)
    {
        if (!billboard || !transform || !cameraInfo)
        {
            std::cout << "[BillboardRenderSystem] UpdateBillboardRotation: Missing component or camera!" << std::endl;
            return false;
        }

        try
        {
            // Get world position of billboard
            glm::vec3 billboard_world_pos = glm::vec3(transform->GetWorldMatrix()[3]);
            glm::vec3 camera_pos = glm::vec3(cameraInfo->pos.x, cameraInfo->pos.y, cameraInfo->pos.z);

            std::cout << "      [Billboard Rotation] Camera: (" << camera_pos.x << ", " << camera_pos.y << ", " << camera_pos.z << ")" << std::endl;
            std::cout << "      [Billboard Rotation] Billboard: (" << billboard_world_pos.x << ", " << billboard_world_pos.y << ", " << billboard_world_pos.z << ")" << std::endl;

            // Calculate direction from billboard to camera
            const glm::vec3 camera_delta = camera_pos - billboard_world_pos;
            const float dist2 = glm::dot(camera_delta, camera_delta);
            if (dist2 < 1e-6f)
            {
                std::cout << "      [Billboard Rotation] Skipped: camera and billboard are at the same position" << std::endl;
                return false;
            }

            const float inv_dist = 1.0f / std::sqrt(dist2);
            glm::vec3 to_camera = camera_delta * inv_dist;
            std::cout << "      [Billboard Rotation] Direction to camera: (" << to_camera.x << ", " << to_camera.y << ", " << to_camera.z << ")" << std::endl;

            // Use camera's precomputed billboard vectors
            glm::vec3 billboard_up = glm::vec3(cameraInfo->billboard_up.x, cameraInfo->billboard_up.y, cameraInfo->billboard_up.z);
            glm::vec3 billboard_right = glm::vec3(cameraInfo->billboard_right.x, cameraInfo->billboard_right.y, cameraInfo->billboard_right.z);

            std::cout << "      [Billboard Rotation] Up: (" << billboard_up.x << ", " << billboard_up.y << ", " << billboard_up.z << ")" << std::endl;
            std::cout << "      [Billboard Rotation] Right: (" << billboard_right.x << ", " << billboard_right.y << ", " << billboard_right.z << ")" << std::endl;

            const bool up_valid = glm::dot(billboard_up, billboard_up) > 1e-6f;
            const bool right_valid = glm::dot(billboard_right, billboard_right) > 1e-6f;
            if (!up_valid || !right_valid)
            {
                const glm::vec3 world_up(0.0f, 1.0f, 0.0f);
                billboard_right = glm::normalize(glm::cross(world_up, to_camera));
                billboard_up = glm::normalize(glm::cross(to_camera, billboard_right));
                std::cout << "      [Billboard Rotation] Fallback axes computed" << std::endl;
            }

            // Create rotation matrix (billboard facing camera)
            // The billboard's -Z axis should point towards the camera (or +Z depending on convention)
            // Using camera's precomputed billboard axes
            glm::mat4 rotation_matrix(1.0f);
            rotation_matrix[0] = glm::vec4(billboard_right, 0.0f);   // X axis = billboard right
            rotation_matrix[1] = glm::vec4(billboard_up, 0.0f);      // Y axis = billboard up
            rotation_matrix[2] = glm::vec4(-to_camera, 0.0f);        // Z axis = -to_camera (away from camera)
            
            // Extract quaternion from rotation matrix
            glm::quat new_rotation = glm::quat_cast(rotation_matrix);
            std::cout << "      [Billboard Rotation] New quaternion: (" << new_rotation.w << ", " << new_rotation.x << ", " << new_rotation.y << ", " << new_rotation.z << ")" << std::endl;

            if (!std::isfinite(new_rotation.w) || !std::isfinite(new_rotation.x) ||
                !std::isfinite(new_rotation.y) || !std::isfinite(new_rotation.z))
            {
                std::cout << "      [Billboard Rotation] Skipped: quaternion is not finite" << std::endl;
                return false;
            }

            // Apply rotation to transform (keep local scale and position)
            transform->SetLocalRotation(new_rotation);
            std::cout << "      [Billboard Rotation] Applied rotation to transform" << std::endl;

            return true;
        }
        catch (const std::exception& e)
        {
            std::cout << "      [Billboard Rotation] Exception: " << e.what() << std::endl;
            return false;
        }
    }
}//namespace hgl::ecs
