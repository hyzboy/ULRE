/*
 Gizmo move (ECS)

 ref: Blender 4

        0                 9-10
        *----------------->>>>
        |
        |
        |         5+
        |         +6
        |
        |
        v

        假设轴尺寸为10
        箭头长度为2，直径为2
        双轴调节正方形，长宽为1，位置为5,5

        中心球半径为1
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

struct GizmoMoveECS
{
    hgl::ecs::World *world = nullptr;
    hgl::ecs::Entity *root = nullptr;
    std::shared_ptr<hgl::ecs::TransformComponent> root_transform;

    MaterialInstance *pick_mi = nullptr;

    struct Axis
    {
        MaterialInstance *mi = nullptr;
        std::shared_ptr<hgl::ecs::PrimitiveComponent> cylinder;
        std::shared_ptr<hgl::ecs::PrimitiveComponent> cone;
        std::shared_ptr<hgl::ecs::PrimitiveComponent> plane;
    };

    Axis axis[3]{};

    math::Ray mouse_ray;

    int cur_axis = -1;
    int pick_axis = -1;
    float cur_dist = 0.0f;
    float pick_dist = 0.0f;

    math::Matrix4f pick_l2w = math::Identity4f;
    math::Vector3f pick_center = math::Vector3f(0.0f, 0.0f, 0.0f);
    math::Vector3f pick_base_pos = math::Vector3f(0.0f, 0.0f, 0.0f);
    math::Vector3f pick_plane_center = math::Vector3f(0.0f, 0.0f, 0.0f);
    math::Vector3f pick_plane_normal = math::Vector3f(0.0f, 0.0f, 1.0f);
    math::Vector3f pick_plane_u = math::Vector3f(1.0f, 0.0f, 0.0f);
    math::Vector3f pick_plane_v = math::Vector3f(0.0f, 1.0f, 0.0f);
    float pick_plane_u_dist = 0.0f;
    float pick_plane_v_dist = 0.0f;

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

    void ApplyAxisMaterials(GizmoMoveECS *gizmo)
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
            if(gizmo->axis[i].cone)
                gizmo->axis[i].cone->SetOverrideMaterial(axis_mi);
            if(gizmo->axis[i].plane)
                gizmo->axis[i].plane->SetOverrideMaterial(plane_mi);
        }
    }

    bool CreateAxisEntities(GizmoMoveECS *gizmo, Primitive *cylinder, Primitive *cone, Primitive *square)
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

            {
                auto entity = gizmo->world->CreateEntity<hgl::ecs::Entity>(
                    (i==0) ? "GizmoMove_X_Cylinder" : (i==1) ? "GizmoMove_Y_Cylinder" : "GizmoMove_Z_Cylinder");
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

            {
                auto entity = gizmo->world->CreateEntity<hgl::ecs::Entity>(
                    (i==0) ? "GizmoMove_X_Cone" : (i==1) ? "GizmoMove_Y_Cone" : "GizmoMove_Z_Cone");
                if(!entity)
                    return false;

                auto transform = entity->AddComponent<hgl::ecs::TransformComponent>();
                transform->SetLocalTRS(glm::vec3(axis_vector * GIZMO_CONE_OFFSET), rotation, glm::vec3(one_scale));
                transform->SetParent(gizmo->root->GetID());

                auto prim_comp = entity->AddComponent<hgl::ecs::PrimitiveComponent>();
                prim_comp->SetPrimitive(cone);
                prim_comp->SetOverrideMaterial(gizmo->axis[i].mi);

                gizmo->axis[i].cone = prim_comp;
                gizmo->entity_ids.push_back(entity->GetID());
            }

            {
                auto entity = gizmo->world->CreateEntity<hgl::ecs::Entity>(
                    (i==0) ? "GizmoMove_X_Plane" : (i==1) ? "GizmoMove_Y_Plane" : "GizmoMove_Z_Plane");
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

GizmoMoveECS *CreateGizmoMoveECS(hgl::ecs::World *world,
                                 const char *name,
                                 const math::Vector3f &position)
{
    if(!world)
        return nullptr;

    auto *gizmo = new GizmoMoveECS;
    gizmo->world = world;
    gizmo->pick_mi = GetGizmoMI3D(GizmoColor::Yellow);

    gizmo->root = world->CreateEntity<hgl::ecs::Entity>(name ? name : "GizmoMove");
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

    Primitive *sphere = GetGizmoPrimitive(GizmoShape::Sphere);
    Primitive *cylinder = GetGizmoPrimitive(GizmoShape::Cylinder);
    Primitive *cone = GetGizmoPrimitive(GizmoShape::Cone);
    Primitive *square = GetGizmoPrimitive(GizmoShape::Square);

    if(!sphere || !cylinder || !cone || !square)
    {
        DestroyGizmoMoveECS(gizmo);
        return nullptr;
    }

    {
        auto entity = world->CreateEntity<hgl::ecs::Entity>("GizmoMove_Sphere");
        if(!entity)
        {
            DestroyGizmoMoveECS(gizmo);
            return nullptr;
        }

        auto transform = entity->AddComponent<hgl::ecs::TransformComponent>();
        transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        transform->SetParent(gizmo->root->GetID());

        auto prim_comp = entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        prim_comp->SetPrimitive(sphere);
        prim_comp->SetOverrideMaterial(GetGizmoMI3D(GizmoColor::White));

        gizmo->entity_ids.push_back(entity->GetID());
    }

    if(!CreateAxisEntities(gizmo, cylinder, cone, square))
    {
        DestroyGizmoMoveECS(gizmo);
        return nullptr;
    }

    return gizmo;
}

void DestroyGizmoMoveECS(GizmoMoveECS *gizmo)
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

bool GetGizmoMoveECSState(const GizmoMoveECS *gizmo, GizmoMoveECSState &out_state)
{
    if(!gizmo)
        return false;

    out_state.cur_axis = gizmo->cur_axis;
    out_state.pick_axis = gizmo->pick_axis;
    out_state.dragging = gizmo->dragging;
    out_state.cur_dist = gizmo->cur_dist;
    out_state.pick_dist = gizmo->pick_dist;

    return true;
}

bool GetGizmoMovePosition(const GizmoMoveECS *gizmo, math::Vector3f &out_position)
{
    if(!gizmo || !gizmo->root_transform)
        return false;

    out_position = gizmo->root_transform->GetLocalPosition();
    return true;
}

void SetGizmoMovePosition(GizmoMoveECS *gizmo, const math::Vector3f &position)
{
    if(!gizmo || !gizmo->root_transform)
        return;

    gizmo->root_transform->SetLocalPosition(glm::vec3(position));
}

void SetGizmoMoveRotation(GizmoMoveECS *gizmo, const glm::quat &rotation)
{
    if(!gizmo || !gizmo->root_transform)
        return;

    gizmo->root_transform->SetLocalRotation(rotation);
}

void SetGizmoMoveVisible(GizmoMoveECS *gizmo, bool visible)
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

void UpdateGizmoMoveECS(GizmoMoveECS *gizmo,
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
    gizmo->root_transform->UpdateIfDirty();

    gizmo->mouse_ray.SetFromViewportPoint(mouse_coord, camera_info, viewport_info->GetViewport());

    if(gizmo->dragging)
    {
        if(gizmo->pick_axis >= 0 && gizmo->pick_axis < 3)
        {
            const math::Vector3f axis_vector = math::GetAxisVector(math::AXIS(gizmo->pick_axis));
            const math::Vector3f axis_world = TransformDirection(gizmo->pick_l2w, axis_vector);

            math::Vector3f p1 = axis_vector * std::fabs(camera_info->zfar - camera_info->znear);
            math::Vector3f p2 = -p1;

            p1 = TransformPosition(gizmo->pick_l2w, p1);
            p2 = TransformPosition(gizmo->pick_l2w, p2);

            math::Vector3f p_ray, p_ls;
            gizmo->mouse_ray.ClosestPoint(p_ray, p_ls, p1, p2);

            gizmo->cur_dist = glm::dot(p_ls - gizmo->pick_center, axis_world);

            const math::Vector3f offset = axis_world * (gizmo->cur_dist - gizmo->pick_dist);
            gizmo->root_transform->SetLocalPosition(glm::vec3(gizmo->pick_base_pos + offset));
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

                    const float cur_u = glm::dot(delta, gizmo->pick_plane_u);
                    const float cur_v = glm::dot(delta, gizmo->pick_plane_v);

                    const float du = cur_u - gizmo->pick_plane_u_dist;
                    const float dv = cur_v - gizmo->pick_plane_v_dist;

                    gizmo->cur_dist = 0.5f * (cur_u + cur_v);

                    const math::Vector3f move_delta = gizmo->pick_plane_u * du + gizmo->pick_plane_v * dv;
                    gizmo->root_transform->SetLocalPosition(glm::vec3(gizmo->pick_base_pos + move_delta));
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

    {
        struct PlaneMapping
        {
            int u_axis;
            int v_axis;
            int n_axis;
        };

        constexpr PlaneMapping mapping[3] =
        {
            {1, 2, 0},
            {0, 2, 1},
            {0, 1, 2}
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
                gizmo->cur_dist = 0.5f * (du + dv);
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
            gizmo->cur_dist = glm::dot(p_ls - center, TransformDirection(l2w, axis_vector));
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
            gizmo->pick_dist = gizmo->cur_dist;
        }
        else
        {
            const float denom = glm::dot(gizmo->mouse_ray.direction, gizmo->pick_plane_normal);
            if(std::fabs(denom) > 1e-6f)
            {
                const float t = glm::dot(gizmo->pick_plane_center - gizmo->mouse_ray.origin, gizmo->pick_plane_normal) / denom;
                if(t >= 0.0f)
                {
                    const math::Vector3f hit_point = gizmo->mouse_ray.origin + gizmo->mouse_ray.direction * t;
                    const math::Vector3f delta = hit_point - gizmo->pick_plane_center;
                    gizmo->pick_plane_u_dist = glm::dot(delta, gizmo->pick_plane_u);
                    gizmo->pick_plane_v_dist = glm::dot(delta, gizmo->pick_plane_v);
                    gizmo->pick_dist = 0.5f * (gizmo->pick_plane_u_dist + gizmo->pick_plane_v_dist);
                }
            }
        }

        gizmo->pick_base_pos = gizmo->root_transform->GetLocalPosition();
        gizmo->dragging = true;
    }
}

}//namespace hgl::graph

