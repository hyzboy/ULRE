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
#include"Gizmo.h"

// ECS
#include<hgl/ecs/core/Context.h>
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
    hgl::ecs::ECSContext *world = nullptr;
    hgl::ecs::Entity *root = nullptr;
    std::shared_ptr<hgl::ecs::TransformComponent> root_transform;

    MaterialInstance *pick_mi = nullptr;

    struct Axis
    {
        MaterialInstance *mi = nullptr;
        std::shared_ptr<hgl::ecs::PrimitiveComponent> cylinder;
        std::shared_ptr<hgl::ecs::PrimitiveComponent> cone;
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

    bool dragging = false;

    std::vector<hgl::ecs::EntityID> entity_ids;
};

namespace
{
    const math::Vector3f one_scale(1.0f, 1.0f, 1.0f);
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

    void ApplyAxisMaterials(GizmoMoveECS *gizmo)
    {
        if(!gizmo)
            return;

        for(int i=0;i<3;i++)
        {
            MaterialInstance *mi = (gizmo->cur_axis == i) ? gizmo->pick_mi : gizmo->axis[i].mi;

            if(gizmo->axis[i].cylinder)
                gizmo->axis[i].cylinder->SetOverrideMaterial(mi);
            if(gizmo->axis[i].cone)
                gizmo->axis[i].cone->SetOverrideMaterial(mi);
        }
    }

    bool CreateAxisEntities(GizmoMoveECS *gizmo, Primitive *cylinder, Primitive *cone)
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
            {math::Vector3f(0.0f, 1.0f, 0.0f),  90.0f, GizmoColor::Red},
            {math::Vector3f(1.0f, 0.0f, 0.0f), -90.0f, GizmoColor::Green},
            {math::Vector3f(0.0f, 0.0f, 0.0f),   0.0f, GizmoColor::Blue}
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
        }

        return true;
    }
}

GizmoMoveECS *CreateGizmoMoveECS(hgl::ecs::ECSContext *world,
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
    gizmo->entity_ids.push_back(gizmo->root->GetID());

    Primitive *sphere = GetGizmoPrimitive(GizmoShape::Sphere);
    Primitive *cylinder = GetGizmoPrimitive(GizmoShape::Cylinder);
    Primitive *cone = GetGizmoPrimitive(GizmoShape::Cone);

    if(!sphere || !cylinder || !cone)
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

    if(!CreateAxisEntities(gizmo, cylinder, cone))
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

            gizmo->cur_dist = glm::dot(p_ls - gizmo->pick_center, axis_vector);

            const math::Vector3f offset = axis_vector * (gizmo->cur_dist - gizmo->pick_dist);
            gizmo->root_transform->SetLocalPosition(glm::vec3(gizmo->pick_base_pos + offset));
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

    const float axis_sphere_radius = glm::length(math::AxisVector::X * (GIZMO_CENTER_SPHERE_RADIUS / 2.0f));
    const float axis_length = glm::length(math::AxisVector::X * GIZMO_ARROW_LENGTH);

    gizmo->cur_axis = -1;

    for(int i=0;i<3;i++)
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
            gizmo->cur_dist = to_center_dist;
            break;
        }
    }

    ApplyAxisMaterials(gizmo);

    if(left_pressed && gizmo->cur_axis >= 0 && gizmo->cur_axis < 3)
    {
        if(input_system && !input_system->BeginMouseCapture(gizmo))
            return;

        gizmo->pick_axis = gizmo->cur_axis;
        gizmo->pick_dist = gizmo->cur_dist;
        gizmo->pick_base_pos = gizmo->root_transform->GetLocalPosition();
        gizmo->dragging = true;
    }
}

}//namespace hgl::graph

