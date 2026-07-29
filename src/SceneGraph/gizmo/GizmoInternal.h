#pragma once

#include "Gizmo.h"

namespace hgl::graph
{
    class PrimitiveAsset;
    namespace mtl
    {
        struct MaterialRecipe;
    }
    // Global resident state for gizmo system
    struct GizmoSystemResidentState
    {
        bool resources_ready = false;
        bool standby = false;
        uint32_t active_system_count = 0;
    };

    extern GizmoSystemResidentState g_gizmo_resident_state;

    using TransformGizmo = GizmoECS;

    TransformGizmo *CreateTransformGizmo(::hgl::ecs::ECSContext *world,
                                         const char *name,
                                         const math::Vector3f &position);
    TransformGizmo *CreateDefaultTransformGizmo(::hgl::ecs::ECSContext *world,
                                                const char *name,
                                                const math::Vector3f &position,
                                                GizmoMode default_mode = GizmoMode::MoveWorld);
    void DestroyTransformGizmo(TransformGizmo *gizmo);

    void SetTransformGizmoMode(TransformGizmo *gizmo, GizmoMode mode);
    GizmoMode GetTransformGizmoMode(const TransformGizmo *gizmo);

    void SetTransformGizmoVisible(TransformGizmo *gizmo, bool visible);
    bool BindTransformGizmoTargetEntity(TransformGizmo *gizmo, hgl::ecs::Entity *target_entity);
    hgl::ecs::Entity *GetTransformGizmoTargetEntity(const TransformGizmo *gizmo);
    void SetTransformGizmoChangedCallback(TransformGizmo *gizmo, GizmoChangedCallback callback);
    void SetTransformGizmoAllowNegativeScale(TransformGizmo *gizmo, bool enabled);
    bool IsTransformGizmoAllowNegativeScale(const TransformGizmo *gizmo);
    void SetTransformGizmoFixedPixelDiameter(TransformGizmo *gizmo, float pixel_diameter);
    float GetTransformGizmoFixedPixelDiameter(const TransformGizmo *gizmo);

    struct GizmoFrameInput
    {
        math::Vector2i           mouse_coord;
        const CameraInfo        *camera_info   = nullptr;
        const ViewportInfo      *viewport_info = nullptr;
        ::hgl::ecs::InputSystem *input_system  = nullptr;
        bool                     left_down     = false;
        bool                     left_pressed  = false;
        bool                     left_released = false;
    };

    void UpdateTransformGizmo(TransformGizmo *gizmo, const GizmoFrameInput &input);

    enum class GizmoColor:uint
    {
        Black=0,
        White,

        Red,
        Green,
        Blue,

        Yellow,

        ENUM_CLASS_RANGE(Black,Yellow)
    };

    enum class GizmoShape:uint
    {
        Square=0,
        Circle,
        Cube,
        Sphere,
        Cone,
        Cylinder,
        Torus,

        ENUM_CLASS_RANGE(Square,Torus)
    };

    bool InitGizmoResource(GraphicsContext *, RenderPass *);
    void FreeGizmoResource();
    bool EnsureGizmoSystemResources(::hgl::ecs::ECSContext *world);
    void ForceReleaseGizmoSystemResources();
    bool IsGizmoSystemResourcesResident();

    MaterialInstance *GetGizmoMI3D(const GizmoColor &);          ///< Legacy alias; returns nullptr in new path
    const mtl::MaterialRecipe *GetGizmoRecipe3D(const GizmoColor &color);
    const PrimitiveAsset *GetGizmoMeshAsset(const GizmoShape &shape);

    // 辅助函数
    hgl::ecs::Entity *GetGizmoRootEntity(const GizmoECS *gizmo);
    }//namespace hgl::graph
