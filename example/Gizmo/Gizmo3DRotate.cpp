/*
 Gizmo rotate (ECS)

 ref: Blender 4

 三个轴向圆环 + 一个永远面向相机的白色圆环
*/

#include"GizmoResource.h"
#include"Gizmo.h"

// ECS
#include<hgl/ecs/core/World.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/ecs/components/VisibilityComponent.h>

#include<hgl/math/geometry/Ray.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/camera/ViewportInfo.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

#include<vector>
#include<cmath>

namespace hgl::graph{

struct GizmoRotateECS
{
    hgl::ecs::World *world = nullptr;
    hgl::ecs::Entity *root = nullptr;
    std::shared_ptr<hgl::ecs::TransformComponent> root_transform;

    MaterialInstance *pick_mi = nullptr;
    MaterialInstance *white_mi = nullptr;

    struct Axis
    {
        MaterialInstance *mi = nullptr;
        std::shared_ptr<hgl::ecs::PrimitiveComponent> torus;
    };

    Axis axis[3]{};

    // 白色圆环，永远面向相机
    std::shared_ptr<hgl::ecs::PrimitiveComponent> white_torus;
    std::shared_ptr<hgl::ecs::TransformComponent> white_torus_transform;

    math::Ray mouse_ray;

    int cur_axis = -1;
    int pick_axis = -1;
    float cur_angle = 0.0f;
    float pick_angle = 0.0f;

    math::Vector3f pick_base_rotation = math::Vector3f(0.0f, 0.0f, 0.0f);
    math::Vector3f pick_plane_normal = math::Vector3f(0.0f, 0.0f, 1.0f);
    math::Vector3f pick_center = math::Vector3f(0.0f, 0.0f, 0.0f);
    math::Vector3f pick_start_dir = math::Vector3f(1.0f, 0.0f, 0.0f);

    bool dragging = false;

    std::vector<hgl::ecs::EntityID> entity_ids;
};

namespace
{
    const math::Vector3f one_scale(1.0f, 1.0f, 1.0f);
    constexpr float TORUS_HOVER_THRESHOLD = 0.5f;
    constexpr float WHITE_TORUS_HOVER_THRESHOLD = 1.0f;

    Primitive *GetGizmoPrimitive(const GizmoShape &shape)
    {
        return GetGizmoMeshPrimitive(shape);
    }

    math::Vector3f TransformPosition(const math::Matrix4f &mat, const math::Vector3f &pos)
    {
        const glm::vec4 v = mat * glm::vec4(pos, 1.0f);
        return math::Vector3f(v.x, v.y, v.z);
    }

    math::Vector3f TransformDirection(const math::Matrix4f &mat, const math::Vector3f &dir)
    {
        const glm::vec4 v = mat * glm::vec4(dir, 0.0f);
        return glm::normalize(math::Vector3f(v.x, v.y, v.z));
    }

    // 计算面向相机的旋转四元数
    glm::quat CalculateFacingRotation(const math::Vector3f &gizmo_pos,
                                       const math::Matrix4f &view_matrix)
    {
        (void)gizmo_pos;

        const math::Vector3f to_camera = glm::normalize(math::Vector3f(view_matrix[0][2], view_matrix[1][2], view_matrix[2][2]));
        const math::Vector3f from_axis = math::AxisVector::X;

        const float cos_theta = glm::dot(from_axis, to_camera);

        if (cos_theta < -0.9999f)
        {
            const math::Vector3f ortho_axis = glm::normalize(glm::cross(math::AxisVector::Y, from_axis));
            return glm::angleAxis(glm::radians(180.0f), ortho_axis);
        }

        const math::Vector3f rotation_axis = glm::cross(from_axis, to_camera);
        const float s = std::sqrt((1.0f + cos_theta) * 2.0f);
        const float inv_s = 1.0f / s;

        return glm::normalize(glm::quat(s * 0.5f,
                                        rotation_axis.x * inv_s,
                                        rotation_axis.y * inv_s,
                                        rotation_axis.z * inv_s));
    }

    void ApplyAxisMaterials(GizmoRotateECS *gizmo)
    {
        if(!gizmo)
            return;

        for(int i=0;i<3;i++)
        {
            MaterialInstance *mi = (gizmo->cur_axis == i) ? gizmo->pick_mi : gizmo->axis[i].mi;

            if(gizmo->axis[i].torus)
                gizmo->axis[i].torus->SetOverrideMaterial(mi);
        }

        if(gizmo->white_torus)
        {
            MaterialInstance *white_mi = (gizmo->cur_axis == 3) ? gizmo->pick_mi : gizmo->white_mi;
            gizmo->white_torus->SetOverrideMaterial(white_mi);
        }
    }

    bool CreateAxisEntities(GizmoRotateECS *gizmo, Primitive *torus)
    {
        if(!gizmo || !gizmo->world || !gizmo->root)
            return false;

        struct AxisConfig
        {
            math::Vector3f rotation_axis;
            float rotation_angle;
            GizmoColor color;
        };

        const AxisConfig axis_config[3]=
        {
            {math::Vector3f(0.0f, 0.0f, 0.0f),   0.0f, GizmoColor::Red},    // X轴 - 红色，无旋转
            {math::Vector3f(0.0f, 0.0f, 1.0f),  90.0f, GizmoColor::Green},  // Y轴 - 绿色，绕Z轴旋转90度
            {math::Vector3f(0.0f, 1.0f, 0.0f),  90.0f, GizmoColor::Blue}    // Z轴 - 蓝色，绕Y轴旋转90度
        };

        for(int i=0;i<3;i++)
        {
            const AxisConfig &cfg = axis_config[i];

            glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
            if(cfg.rotation_angle != 0.0f)
            {
                rotation = glm::angleAxis(glm::radians(cfg.rotation_angle),
                                          glm::vec3(cfg.rotation_axis.x, cfg.rotation_axis.y, cfg.rotation_axis.z));
            }

            gizmo->axis[i].mi = GetGizmoMI3D(cfg.color);

            auto entity = gizmo->world->CreateEntity<hgl::ecs::Entity>(
                (i==0) ? "GizmoRotate_X_Torus" : (i==1) ? "GizmoRotate_Y_Torus" : "GizmoRotate_Z_Torus");
            if(!entity)
                return false;

            auto transform = entity->AddComponent<hgl::ecs::TransformComponent>();
            const math::Vector3f torus_scale(GIZMO_ARROW_LENGTH, GIZMO_ARROW_LENGTH, GIZMO_ARROW_LENGTH);
            transform->SetLocalTRS(glm::vec3(0.0f), rotation, glm::vec3(torus_scale));
            transform->SetParent(gizmo->root->GetID());

            auto prim_comp = entity->AddComponent<hgl::ecs::PrimitiveComponent>();
            prim_comp->SetPrimitive(torus);
            prim_comp->SetOverrideMaterial(gizmo->axis[i].mi);

            gizmo->axis[i].torus = prim_comp;
            gizmo->entity_ids.push_back(entity->GetID());
        }

        return true;
    }
}

GizmoRotateECS *CreateGizmoRotateECS(hgl::ecs::World *world,
                                      const char *name,
                                      const math::Vector3f &position)
{
    if(!world)
        return nullptr;

    auto *gizmo = new GizmoRotateECS;
    gizmo->world = world;
    gizmo->pick_mi = GetGizmoMI3D(GizmoColor::Yellow);
    gizmo->white_mi = GetGizmoMI3D(GizmoColor::White);

    gizmo->root = world->CreateEntity<hgl::ecs::Entity>(name ? name : "GizmoRotate");
    if(!gizmo->root)
    {
        delete gizmo;
        return nullptr;
    }

    gizmo->root_transform = gizmo->root->AddComponent<hgl::ecs::TransformComponent>();
    gizmo->root_transform->SetLocalTRS(glm::vec3(position), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
    gizmo->root_transform->SetMovable(true);
    gizmo->entity_ids.push_back(gizmo->root->GetID());

    Primitive *torus = GetGizmoPrimitive(GizmoShape::Torus);

    if(!torus)
    {
        DestroyGizmoRotateECS(gizmo);
        return nullptr;
    }

    if(!CreateAxisEntities(gizmo, torus))
    {
        DestroyGizmoRotateECS(gizmo);
        return nullptr;
    }

    // 创建白色圆环（面向相机）
    {
        auto entity = world->CreateEntity<hgl::ecs::Entity>("GizmoRotate_WhiteTorus");
        if(!entity)
        {
            DestroyGizmoRotateECS(gizmo);
            return nullptr;
        }

        gizmo->white_torus_transform = entity->AddComponent<hgl::ecs::TransformComponent>();
        const math::Vector3f white_scale(13.0f, 13.0f, 13.0f);
        gizmo->white_torus_transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(white_scale));
        gizmo->white_torus_transform->SetParent(gizmo->root->GetID());

        auto prim_comp = entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        prim_comp->SetPrimitive(torus);
        prim_comp->SetOverrideMaterial(gizmo->white_mi);

        gizmo->white_torus = prim_comp;
        gizmo->entity_ids.push_back(entity->GetID());
    }

    return gizmo;
}

void DestroyGizmoRotateECS(GizmoRotateECS *gizmo)
{
    if(!gizmo)
        return;

    if(gizmo->world)
    {
        for(const auto &id : gizmo->entity_ids)
        {
            if(id.IsValid())
                gizmo->world->DestroyEntity(id);
        }
    }

    delete gizmo;
}

bool GetGizmoRotateECSState(const GizmoRotateECS *gizmo, GizmoRotateECSState &out_state)
{
    if(!gizmo)
        return false;

    out_state.cur_axis = gizmo->cur_axis;
    out_state.pick_axis = gizmo->pick_axis;
    out_state.dragging = gizmo->dragging;
    out_state.cur_angle = gizmo->cur_angle;
    out_state.pick_angle = gizmo->pick_angle;

    return true;
}

void SetGizmoRotatePosition(GizmoRotateECS *gizmo, const math::Vector3f &position)
{
    if(!gizmo || !gizmo->root_transform)
        return;

    gizmo->root_transform->SetLocalPosition(glm::vec3(position));
}

void SetGizmoRotateVisible(GizmoRotateECS *gizmo, bool visible)
{
    if(!gizmo || !gizmo->world)
        return;

    for(const auto &id : gizmo->entity_ids)
    {
        if(id.IsValid())
        {
            auto entity = gizmo->world->GetEntity(id);
            if(entity)
            {
                // Add or get VisibilityComponent
                auto vis_comp = entity->GetComponent<hgl::ecs::VisibilityComponent>();
                if(!vis_comp)
                {
                    vis_comp = entity->AddComponent<hgl::ecs::VisibilityComponent>();
                }
                if(vis_comp)
                {
                    vis_comp->SetVisible(visible);
                }
            }
        }
    }
}

void UpdateGizmoRotateECS(GizmoRotateECS *gizmo,
                          const math::Vector2i &mouse_coord,
                          const graph::CameraInfo *camera_info,
                          const graph::ViewportInfo *viewport_info,
                          hgl::ecs::InputSystem *input_system,
                          bool left_down,
                          bool left_pressed,
                          bool left_released)
{
    if(!gizmo || !gizmo->root_transform || !camera_info || !viewport_info)
        return;

    // 更新白色圆环朝向相机
    if(gizmo->white_torus_transform)
    {
        const math::Vector3f gizmo_pos = gizmo->root_transform->GetWorldPosition();
        const glm::quat facing_rotation = CalculateFacingRotation(gizmo_pos, camera_info->view);
        gizmo->white_torus_transform->SetLocalRotation(facing_rotation);
    }

    gizmo->mouse_ray.SetFromViewportPoint(mouse_coord, camera_info, viewport_info->GetViewport());

    if(gizmo->dragging)
    {
        if(gizmo->pick_axis >= 0 && gizmo->pick_axis <= 3)
        {
            // 计算鼠标射线与旋转平面的交点
            const float denom = glm::dot(gizmo->mouse_ray.direction, gizmo->pick_plane_normal);
            if(std::fabs(denom) > 1e-6)
            {
                const float t = glm::dot(gizmo->pick_center - gizmo->mouse_ray.origin, gizmo->pick_plane_normal) / denom;
                if(t >= 0.0f)
                {
                    const math::Vector3f hit_point = gizmo->mouse_ray.origin + gizmo->mouse_ray.direction * t;
                    const math::Vector3f cur_dir = glm::normalize(hit_point - gizmo->pick_center);

                    // 计算角度 (使用 atan2)
                    const float cos_angle = glm::dot(gizmo->pick_start_dir, cur_dir);
                    const math::Vector3f cross = glm::cross(gizmo->pick_start_dir, cur_dir);
                    const float sin_angle = glm::dot(cross, gizmo->pick_plane_normal);
                    gizmo->cur_angle = std::atan2(sin_angle, cos_angle);

                    // 应用旋转（这里简化为绕单轴旋转，实际应用中可能需要更复杂的逻辑）
                    // TODO: 根据实际需求实现旋转应用
                }
            }
        }

        if(left_released)
        {
            gizmo->dragging = false;
            if(input_system)
                input_system->EndMouseCapture(gizmo);
        }

        return;
    }

    const math::Matrix4f l2w = gizmo->root_transform->GetWorldMatrix();
    const math::Vector3f center = TransformPosition(l2w, math::Vector3f(0.0f, 0.0f, 0.0f));

    gizmo->cur_axis = -1;

    // 检测鼠标悬停在白色圆环上（屏幕朝向环）
    if(gizmo->white_torus_transform)
    {
        const math::Vector3f white_plane_normal = glm::normalize(math::Vector3f(camera_info->view[0][2], camera_info->view[1][2], camera_info->view[2][2]));
        const float denom = glm::dot(gizmo->mouse_ray.direction, white_plane_normal);
        if(std::fabs(denom) > 1e-6f)
        {
            const float t = glm::dot(center - gizmo->mouse_ray.origin, white_plane_normal) / denom;
            if(t >= 0.0f)
            {
                const math::Vector3f hit_point = gizmo->mouse_ray.origin + gizmo->mouse_ray.direction * t;
                const float dist_to_center = glm::distance(hit_point, center);
                const float white_radius = 13.0f;

                if(std::fabs(dist_to_center - white_radius) < WHITE_TORUS_HOVER_THRESHOLD)
                {
                    gizmo->cur_axis = 3;
                    gizmo->pick_center = center;
                    gizmo->pick_plane_normal = white_plane_normal;
                }
            }
        }
    }

    // 检测鼠标悬停在哪个轴的圆环上
    for(int i=0;i<3;i++)
    {
        if(gizmo->cur_axis == 3)
            break;

        const math::Vector3f axis_vector = math::GetAxisVector(math::AXIS(i));
        const math::Vector3f plane_normal = TransformDirection(l2w, axis_vector);

        // 计算射线与圆环平面的交点
        const float denom = glm::dot(gizmo->mouse_ray.direction, plane_normal);
        if(std::fabs(denom) > 1e-6)
        {
            const float t = glm::dot(center - gizmo->mouse_ray.origin, plane_normal) / denom;
            if(t >= 0.0f)
            {
                const math::Vector3f hit_point = gizmo->mouse_ray.origin + gizmo->mouse_ray.direction * t;
                const float dist_to_center = glm::distance(hit_point, center);
                const float torus_radius = GIZMO_ARROW_LENGTH;

                // 检查是否在圆环附近
                if(std::fabs(dist_to_center - torus_radius) < TORUS_HOVER_THRESHOLD)
                {
                    gizmo->cur_axis = i;
                    gizmo->pick_center = center;
                    gizmo->pick_plane_normal = plane_normal;
                    break;
                }
            }
        }
    }

    ApplyAxisMaterials(gizmo);

    if(left_pressed && gizmo->cur_axis >= 0 && gizmo->cur_axis <= 3)
    {
        if(input_system && !input_system->BeginMouseCapture(gizmo))
            return;

        gizmo->pick_axis = gizmo->cur_axis;

        // 计算初始方向
        const float denom = glm::dot(gizmo->mouse_ray.direction, gizmo->pick_plane_normal);
        if(std::fabs(denom) > 1e-6)
        {
            const float t = glm::dot(gizmo->pick_center - gizmo->mouse_ray.origin, gizmo->pick_plane_normal) / denom;
            if(t >= 0.0f)
            {
                const math::Vector3f hit_point = gizmo->mouse_ray.origin + gizmo->mouse_ray.direction * t;
                gizmo->pick_start_dir = glm::normalize(hit_point - gizmo->pick_center);
            }
        }

        gizmo->cur_angle = 0.0f;
        gizmo->pick_angle = 0.0f;
        gizmo->dragging = true;
    }
}

}//namespace hgl::graph

