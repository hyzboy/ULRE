/*
 Gizmo scale (ECS)

 ref: Blender 4

        0                 9-10
        *-----------------[][
        |
        |
        |         5+
        |         +6
        |
        |
        v

        假设轴尺寸为10
        立方体边长为1，直径为1
        双轴调节正方形，长宽为1，位置为5,5

        中心立方体边长为2
*/

#include"GizmoResource.h"
#include"GizmoInternal.h"

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

struct GizmoScaleECS
{
    hgl::ecs::World *world = nullptr;
    hgl::ecs::Entity *root = nullptr;
    std::shared_ptr<hgl::ecs::TransformComponent> root_transform;

    MaterialInstance *pick_mi = nullptr;

    struct Axis
    {
        MaterialInstance *mi = nullptr;
        std::shared_ptr<hgl::ecs::PrimitiveComponent> cylinder;
        std::shared_ptr<hgl::ecs::PrimitiveComponent> cube;
        std::shared_ptr<hgl::ecs::PrimitiveComponent> plane;
    };

    Axis axis[3]{};

    math::Ray mouse_ray;

    int cur_axis = -1;
    int pick_axis = -1;
    float cur_scale = 1.0f;
    float pick_scale = 1.0f;

    math::Matrix4f pick_l2w = math::Identity4f;
    math::Vector3f pick_center = math::Vector3f(0.0f, 0.0f, 0.0f);
    math::Vector3f pick_plane_center = math::Vector3f(0.0f, 0.0f, 0.0f);
    math::Vector3f pick_plane_normal = math::Vector3f(0.0f, 0.0f, 1.0f);
    math::Vector3f pick_plane_u = math::Vector3f(1.0f, 0.0f, 0.0f);
    math::Vector3f pick_plane_v = math::Vector3f(0.0f, 1.0f, 0.0f);
    float pick_plane_u_dist = 0.0f;
    float pick_plane_v_dist = 0.0f;
    float pick_plane_u_ref = 1.0f;
    float pick_plane_v_ref = 1.0f;
    math::Vector3f pick_base_scale = math::Vector3f(1.0f, 1.0f, 1.0f);
    float pick_dist = 0.0f;
    float cur_scale_u = 1.0f;
    float cur_scale_v = 1.0f;
    float pick_scale_u = 1.0f;
    float pick_scale_v = 1.0f;

    bool dragging = false;

    std::vector<hgl::ecs::EntityID> entity_ids;
};

namespace
{
    const math::Vector3f one_scale(1.0f, 1.0f, 1.0f);
    const math::Vector3f plane_scale(2.0f, 2.0f, 2.0f);
    const math::Vector3f cylinder_scale(GIZMO_CYLINDER_RADIUS, GIZMO_CYLINDER_RADIUS, GIZMO_CYLINDER_HALF_LENGTH);

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

    float ClampSignedScaleRatio(float value)
    {
        const float clamped = glm::clamp(value, -10.0f, 10.0f);

        if (std::fabs(clamped) < 0.05f)
            return (clamped < 0.0f) ? -0.05f : 0.05f;

        return clamped;
    }

    constexpr float PLANE_SCALE_SENSITIVITY = 0.35f;
    constexpr float PLANE_SCALE_MIN_REF = 4.0f;

    void ApplyAxisMaterials(GizmoScaleECS *gizmo)
    {
        if(!gizmo)
            return;

        for(int i=0;i<3;i++)
        {
            const bool axis_selected = (gizmo->cur_axis == i);
            const bool plane_selected = (gizmo->cur_axis == (i + 3));
            MaterialInstance *axis_mi = axis_selected ? gizmo->pick_mi : gizmo->axis[i].mi;
            MaterialInstance *plane_mi = plane_selected ? gizmo->pick_mi : gizmo->axis[i].mi;

            if(gizmo->axis[i].cylinder)
                gizmo->axis[i].cylinder->SetOverrideMaterial(axis_mi);
            if(gizmo->axis[i].cube)
                gizmo->axis[i].cube->SetOverrideMaterial(axis_mi);
            if(gizmo->axis[i].plane)
                gizmo->axis[i].plane->SetOverrideMaterial(plane_mi);
        }
    }

    bool CreateAxisEntities(GizmoScaleECS *gizmo, Primitive *cylinder, Primitive *cube, Primitive *square)
    {
        if(!gizmo || !gizmo->world || !gizmo->root)
            return false;

        struct AxisConfig
        {
            math::Vector3f rotation_axis;
            float rotation_angle;
            GizmoColor color;
            math::Vector3f plane_position;
        };

        const AxisConfig axis_config[3]=
        {
            {math::Vector3f(0.0f, 1.0f, 0.0f),  90.0f, GizmoColor::Red,   math::Vector3f(0.0f, GIZMO_TWO_AXIS_OFFSET, GIZMO_TWO_AXIS_OFFSET)},
            {math::Vector3f(1.0f, 0.0f, 0.0f), -90.0f, GizmoColor::Green, math::Vector3f(GIZMO_TWO_AXIS_OFFSET, 0.0f, GIZMO_TWO_AXIS_OFFSET)},
            {math::Vector3f(0.0f, 0.0f, 0.0f),   0.0f, GizmoColor::Blue,  math::Vector3f(GIZMO_TWO_AXIS_OFFSET, GIZMO_TWO_AXIS_OFFSET, 0.0f)}
        };

        for(int i=0;i<3;i++)
        {
            const math::Vector3f axis_vector = math::GetAxisVector(math::AXIS(i));
            const AxisConfig &cfg = axis_config[i];

            glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
            if(cfg.rotation_angle != 0.0f)
            {
                rotation = glm::angleAxis(glm::radians(cfg.rotation_angle),
                                          glm::vec3(cfg.rotation_axis.x, cfg.rotation_axis.y, cfg.rotation_axis.z));
            }

            gizmo->axis[i].mi = GetGizmoMI3D(cfg.color);

            // 圆柱
            {
                auto entity = gizmo->world->CreateEntity<hgl::ecs::Entity>(
                    (i==0) ? "GizmoScale_X_Cylinder" : (i==1) ? "GizmoScale_Y_Cylinder" : "GizmoScale_Z_Cylinder");
                if(!entity)
                    return false;

                auto transform = entity->AddComponent<hgl::ecs::TransformComponent>();
                transform->SetLocalTRS(glm::vec3(axis_vector * GIZMO_CYLINDER_OFFSET), rotation, glm::vec3(cylinder_scale));
                transform->SetParent(gizmo->root->GetID());

                auto prim_comp = entity->AddComponent<hgl::ecs::PrimitiveComponent>();
                prim_comp->SetPrimitive(cylinder);
                prim_comp->SetOverrideMaterial(gizmo->axis[i].mi);

                gizmo->axis[i].cylinder = prim_comp;
                gizmo->entity_ids.push_back(entity->GetID());
            }

            // 末端立方体
            {
                auto entity = gizmo->world->CreateEntity<hgl::ecs::Entity>(
                    (i==0) ? "GizmoScale_X_Cube" : (i==1) ? "GizmoScale_Y_Cube" : "GizmoScale_Z_Cube");
                if(!entity)
                    return false;

                auto transform = entity->AddComponent<hgl::ecs::TransformComponent>();
                transform->SetLocalTRS(glm::vec3(axis_vector * GIZMO_CONE_OFFSET), rotation, glm::vec3(one_scale));
                transform->SetParent(gizmo->root->GetID());

                auto prim_comp = entity->AddComponent<hgl::ecs::PrimitiveComponent>();
                prim_comp->SetPrimitive(cube);
                prim_comp->SetOverrideMaterial(gizmo->axis[i].mi);

                gizmo->axis[i].cube = prim_comp;
                gizmo->entity_ids.push_back(entity->GetID());
            }

            // 双轴调节平面
            {
                auto entity = gizmo->world->CreateEntity<hgl::ecs::Entity>(
                    (i==0) ? "GizmoScale_X_Plane" : (i==1) ? "GizmoScale_Y_Plane" : "GizmoScale_Z_Plane");
                if(!entity)
                    return false;

                auto transform = entity->AddComponent<hgl::ecs::TransformComponent>();
                transform->SetLocalTRS(glm::vec3(cfg.plane_position), rotation, glm::vec3(plane_scale));
                transform->SetParent(gizmo->root->GetID());

                auto prim_comp = entity->AddComponent<hgl::ecs::PrimitiveComponent>();
                prim_comp->SetPrimitive(square);
                prim_comp->SetOverrideMaterial(gizmo->axis[i].mi);

                gizmo->axis[i].plane = prim_comp;
                gizmo->entity_ids.push_back(entity->GetID());
            }
        }

        return true;
    }
}

GizmoScaleECS *CreateGizmoScaleECS(hgl::ecs::World *world,
                                    const char *name,
                                    const math::Vector3f &position)
{
    if(!world)
        return nullptr;

    auto *gizmo = new GizmoScaleECS;
    gizmo->world = world;
    gizmo->pick_mi = GetGizmoMI3D(GizmoColor::Yellow);

    gizmo->root = world->CreateEntity<hgl::ecs::Entity>(name ? name : "GizmoScale");
    if(!gizmo->root)
    {
        delete gizmo;
        return nullptr;
    }

    gizmo->root_transform = gizmo->root->AddComponent<hgl::ecs::TransformComponent>();
    gizmo->root_transform->SetLocalTRS(glm::vec3(position), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
    gizmo->root_transform->SetMovable(true);
    gizmo->root_transform->SetFixedPixelSizingParameters(GIZMO_FIXED_PIXEL_DIAMETER,
                                                         GIZMO_ARROW_LENGTH * 2.0f,
                                                         0.01f);
    gizmo->root_transform->SetFixedPixelSizingEnabled(true);
    gizmo->entity_ids.push_back(gizmo->root->GetID());

    Primitive *center_cube = GetGizmoPrimitive(GizmoShape::Cube);
    Primitive *cylinder = GetGizmoPrimitive(GizmoShape::Cylinder);
    Primitive *cube = GetGizmoPrimitive(GizmoShape::Cube);
    Primitive *square = GetGizmoPrimitive(GizmoShape::Square);

    if(!center_cube || !cylinder || !cube || !square)
    {
        DestroyGizmoScaleECS(gizmo);
        return nullptr;
    }

    // 中心立方体
    {
        auto entity = world->CreateEntity<hgl::ecs::Entity>("GizmoScale_CenterCube");
        if(!entity)
        {
            DestroyGizmoScaleECS(gizmo);
            return nullptr;
        }

        auto transform = entity->AddComponent<hgl::ecs::TransformComponent>();
        const math::Vector3f center_scale(GIZMO_CENTER_SPHERE_RADIUS * 2.0f,
                                          GIZMO_CENTER_SPHERE_RADIUS * 2.0f,
                                          GIZMO_CENTER_SPHERE_RADIUS * 2.0f);
        transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(center_scale));
        transform->SetParent(gizmo->root->GetID());

        auto prim_comp = entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        prim_comp->SetPrimitive(center_cube);
        prim_comp->SetOverrideMaterial(GetGizmoMI3D(GizmoColor::White));

        gizmo->entity_ids.push_back(entity->GetID());
    }

    if(!CreateAxisEntities(gizmo, cylinder, cube, square))
    {
        DestroyGizmoScaleECS(gizmo);
        return nullptr;
    }

    return gizmo;
}

void DestroyGizmoScaleECS(GizmoScaleECS *gizmo)
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

bool GetGizmoScaleECSState(const GizmoScaleECS *gizmo, GizmoScaleECSState &out_state)
{
    if(!gizmo)
        return false;

    out_state.cur_axis = gizmo->cur_axis;
    out_state.pick_axis = gizmo->pick_axis;
    out_state.dragging = gizmo->dragging;
    out_state.cur_scale = gizmo->cur_scale;
    out_state.pick_scale = gizmo->pick_scale;
    out_state.cur_scale_u = gizmo->cur_scale_u;
    out_state.cur_scale_v = gizmo->cur_scale_v;
    out_state.pick_scale_u = gizmo->pick_scale_u;
    out_state.pick_scale_v = gizmo->pick_scale_v;

    return true;
}

void SetGizmoScalePosition(GizmoScaleECS *gizmo, const math::Vector3f &position)
{
    if(!gizmo || !gizmo->root_transform)
        return;

    gizmo->root_transform->SetLocalPosition(glm::vec3(position));
}

void SetGizmoScaleRotation(GizmoScaleECS *gizmo, const glm::quat &rotation)
{
    if(!gizmo || !gizmo->root_transform)
        return;

    gizmo->root_transform->SetLocalRotation(rotation);
}

void SetGizmoScaleVisible(GizmoScaleECS *gizmo, bool visible)
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

void UpdateGizmoScaleECS(GizmoScaleECS *gizmo,
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

    gizmo->root_transform->SetFixedPixelSizingContext(camera_info, viewport_info);

    gizmo->mouse_ray.SetFromViewportPoint(mouse_coord, camera_info, viewport_info->GetViewport());

    if(gizmo->dragging)
    {
        if(gizmo->pick_axis >= 0 && gizmo->pick_axis < 3)
        {
            const math::Vector3f axis_vector = math::GetAxisVector(math::AXIS(gizmo->pick_axis));

            math::Vector3f p1 = axis_vector * std::fabs(camera_info->zfar - camera_info->znear);
            math::Vector3f p2 = -p1;

            p1 = TransformPosition(gizmo->pick_l2w, p1);
            p2 = TransformPosition(gizmo->pick_l2w, p2);

            math::Vector3f p_ray, p_ls;
            gizmo->mouse_ray.ClosestPoint(p_ray, p_ls, p1, p2);

            const float cur_dist = glm::dot(p_ls - gizmo->pick_center, axis_vector);

            // 计算缩放比例
            if(std::fabs(gizmo->pick_dist) > 1e-6)
            {
                gizmo->cur_scale = cur_dist / gizmo->pick_dist;
                gizmo->cur_scale = ClampSignedScaleRatio(gizmo->cur_scale);
                    gizmo->cur_scale_u = gizmo->cur_scale;
                    gizmo->cur_scale_v = gizmo->cur_scale;

                // 应用缩放（这里简化为均匀缩放，实际应用中可能需要更复杂的逻辑）
                // TODO: 根据实际需求实现缩放应用
            }
        }
        else if(gizmo->pick_axis >= 3 && gizmo->pick_axis < 6)
        {
            const float denom = glm::dot(gizmo->mouse_ray.direction, gizmo->pick_plane_normal);
            if(std::fabs(denom) > 1e-6f)
            {
                const float t = glm::dot(gizmo->pick_plane_center - gizmo->mouse_ray.origin, gizmo->pick_plane_normal) / denom;
                if(t >= 0.0f)
                {
                    const math::Vector3f hit_point = gizmo->mouse_ray.origin + gizmo->mouse_ray.direction * t;
                    const math::Vector3f delta = hit_point - gizmo->pick_plane_center;

                    const float du = glm::dot(delta, gizmo->pick_plane_u);
                    const float dv = glm::dot(delta, gizmo->pick_plane_v);

                    if(gizmo->pick_plane_u_ref > 1e-6f)
                        gizmo->cur_scale_u = ClampSignedScaleRatio(1.0f + ((du - gizmo->pick_plane_u_dist) / gizmo->pick_plane_u_ref) * PLANE_SCALE_SENSITIVITY);

                    if(gizmo->pick_plane_v_ref > 1e-6f)
                        gizmo->cur_scale_v = ClampSignedScaleRatio(1.0f + ((dv - gizmo->pick_plane_v_dist) / gizmo->pick_plane_v_ref) * PLANE_SCALE_SENSITIVITY);

                    gizmo->cur_scale = (gizmo->cur_scale_u + gizmo->cur_scale_v) * 0.5f;
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
    const glm::vec3 gizmo_scale = gizmo->root_transform->GetLocalScale();
    const float scale_factor = std::max(0.01f, std::fabs(gizmo_scale.x));

    const float axis_sphere_radius = glm::length(math::AxisVector::X * (GIZMO_CENTER_SPHERE_RADIUS / 2.0f));
    const float axis_length = glm::length(math::AxisVector::X * GIZMO_ARROW_LENGTH);

    gizmo->cur_axis = -1;

    // 先检测双轴平面方片（优先级高于轴）
    {
        struct PlaneMapping
        {
            int u_axis;
            int v_axis;
            int n_axis;
        };

        constexpr PlaneMapping mapping[3] =
        {
            {1, 2, 0}, // YZ plane (X normal)
            {0, 2, 1}, // XZ plane (Y normal)
            {0, 1, 2}  // XY plane (Z normal)
        };

        const float half_extent = 1.0f * scale_factor;
        const float plane_offset = GIZMO_TWO_AXIS_OFFSET * scale_factor;

        for(int i=0;i<3;i++)
        {
            const auto &m = mapping[i];

            const math::Vector3f u_dir = TransformDirection(l2w, math::GetAxisVector(math::AXIS(m.u_axis)));
            const math::Vector3f v_dir = TransformDirection(l2w, math::GetAxisVector(math::AXIS(m.v_axis)));
            const math::Vector3f n_dir = TransformDirection(l2w, math::GetAxisVector(math::AXIS(m.n_axis)));

            const math::Vector3f plane_center = center + u_dir * plane_offset + v_dir * plane_offset;

            const float denom = glm::dot(gizmo->mouse_ray.direction, n_dir);
            if(std::fabs(denom) <= 1e-6f)
                continue;

            const float t = glm::dot(plane_center - gizmo->mouse_ray.origin, n_dir) / denom;
            if(t < 0.0f)
                continue;

            const math::Vector3f hit = gizmo->mouse_ray.origin + gizmo->mouse_ray.direction * t;
            const math::Vector3f rel = hit - plane_center;

            const float du = glm::dot(rel, u_dir);
            const float dv = glm::dot(rel, v_dir);

            if(std::fabs(du) <= half_extent && std::fabs(dv) <= half_extent)
            {
                gizmo->cur_axis = i + 3;
                gizmo->pick_center = center;
                gizmo->pick_l2w = l2w;
                gizmo->pick_plane_center = plane_center;
                gizmo->pick_plane_normal = n_dir;
                gizmo->pick_plane_u = u_dir;
                gizmo->pick_plane_v = v_dir;
                break;
            }
        }
    }

    for(int i=0;i<3 && gizmo->cur_axis < 0;i++)
    {
        const math::Vector3f axis_vector = math::GetAxisVector(math::AXIS(i));
        const math::Vector3f axis_endpoint = TransformPosition(l2w, axis_vector * axis_length);

        math::Vector3f p_ray, p_ls;

        gizmo->mouse_ray.ClosestPoint(p_ray, p_ls, center, axis_endpoint);

        const float to_center_dist = glm::distance(p_ls, center);
        const float dist = glm::distance(p_ls, p_ray);

        if(to_center_dist > axis_sphere_radius &&
           to_center_dist < axis_length &&
           dist < GIZMO_CYLINDER_RADIUS)
        {
            gizmo->cur_axis = i;
            gizmo->pick_center = center;
            gizmo->pick_l2w = l2w;
            break;
        }
    }

    ApplyAxisMaterials(gizmo);

    if(left_pressed && gizmo->cur_axis >= 0)
    {
        if(input_system && !input_system->BeginMouseCapture(gizmo))
            return;

        gizmo->pick_axis = gizmo->cur_axis;

        if(gizmo->pick_axis < 3)
        {
            const math::Vector3f axis_vector = math::GetAxisVector(math::AXIS(gizmo->pick_axis));
            const math::Vector3f axis_endpoint = TransformPosition(l2w, axis_vector * axis_length);

            math::Vector3f p_ray, p_ls;
            gizmo->mouse_ray.ClosestPoint(p_ray, p_ls, center, axis_endpoint);

            gizmo->pick_dist = glm::dot(p_ls - center, axis_vector);
        }
        else
        {
            const float denom = glm::dot(gizmo->mouse_ray.direction, gizmo->pick_plane_normal);
            if(std::fabs(denom) > 1e-6f)
            {
                const float t = glm::dot(gizmo->pick_plane_center - gizmo->mouse_ray.origin, gizmo->pick_plane_normal) / denom;
                if(t >= 0.0f)
                {
                    const math::Vector3f hit = gizmo->mouse_ray.origin + gizmo->mouse_ray.direction * t;
                    const math::Vector3f rel = hit - gizmo->pick_plane_center;

                    const float du = glm::dot(rel, gizmo->pick_plane_u);
                    const float dv = glm::dot(rel, gizmo->pick_plane_v);

                    gizmo->pick_plane_u_dist = du;
                    gizmo->pick_plane_v_dist = dv;
                    gizmo->pick_plane_u_ref = std::max(std::fabs(du), PLANE_SCALE_MIN_REF);
                    gizmo->pick_plane_v_ref = std::max(std::fabs(dv), PLANE_SCALE_MIN_REF);
                }
                else
                {
                    gizmo->pick_plane_u_dist = 0.0f;
                    gizmo->pick_plane_v_dist = 0.0f;
                    gizmo->pick_plane_u_ref = 1.0f;
                    gizmo->pick_plane_v_ref = 1.0f;
                }
            }
            else
            {
                gizmo->pick_plane_u_dist = 0.0f;
                gizmo->pick_plane_v_dist = 0.0f;
                gizmo->pick_plane_u_ref = 1.0f;
                gizmo->pick_plane_v_ref = 1.0f;
            }

            gizmo->pick_scale_u = gizmo->cur_scale_u;
            gizmo->pick_scale_v = gizmo->cur_scale_v;
        }

        gizmo->cur_scale = 1.0f;
        gizmo->pick_scale = 1.0f;
        gizmo->cur_scale_u = 1.0f;
        gizmo->cur_scale_v = 1.0f;
        gizmo->pick_scale_u = 1.0f;
        gizmo->pick_scale_v = 1.0f;
        gizmo->dragging = true;
    }
}

}//namespace hgl::graph

