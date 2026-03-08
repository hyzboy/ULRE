#pragma once

#include <array>
#include <memory>

#include "channels/IGizmoChannel.h"
#include "channels/MoveGizmoChannel.h"
#include "channels/RotateGizmoChannel.h"
#include "channels/ScaleGizmoChannel.h"

namespace hgl::ecs
{
class InputSystem;
class TransformComponent;
}

namespace hgl::graph
{

struct GizmoECS;
struct CameraInfo;
class ViewportInfo;

struct AssetUpdateFrameState
{
    std::shared_ptr<hgl::ecs::TransformComponent> target_transform;
    bool has_view_context = false;
    math::Vector3f prev_pos{0.0f, 0.0f, 0.0f};
    glm::quat prev_rot{1.0f, 0.0f, 0.0f, 0.0f};
    math::Vector3f prev_scale{1.0f, 1.0f, 1.0f};
    math::Vector3f cur_effective_scale{1.0f, 1.0f, 1.0f};
};

class GizmoController
{
public:
    enum class ChannelSlot : uint8_t
    {
        Move = 0,
        Rotate = 1,
        Scale = 2,
    };

    static bool IsMoveMode(GizmoMode mode)
    {
        return mode == GizmoMode::MoveWorld || mode == GizmoMode::MoveLocal;
    }

    static bool IsRotateMode(GizmoMode mode)
    {
        return mode == GizmoMode::RotateWorld || mode == GizmoMode::RotateLocal;
    }

    static bool IsScaleMode(GizmoMode mode)
    {
        return mode == GizmoMode::ScaleLocal;
    }

    static bool IsLocalMode(GizmoMode mode)
    {
        return mode == GizmoMode::MoveLocal || mode == GizmoMode::RotateLocal || mode == GizmoMode::ScaleLocal;
    }

    static bool IsWorldMode(GizmoMode mode)
    {
        return mode == GizmoMode::MoveWorld || mode == GizmoMode::RotateWorld;
    }

    static ChannelSlot SlotForMode(GizmoMode mode)
    {
        if (IsMoveMode(mode))
            return ChannelSlot::Move;
        if (IsRotateMode(mode))
            return ChannelSlot::Rotate;
        return ChannelSlot::Scale;
    }

    // Phase-in APIs: move update-stage orchestration decisions into controller.
    static bool ShouldRefreshHoverBeforeDrag(const GizmoECS *gizmo);
    static bool CanRunDragDispatch(const GizmoECS *gizmo, bool left_down);
    static bool ShouldAttemptBeginDrag(const GizmoECS *gizmo, bool left_pressed);
    static bool ShouldEndDragOnRelease(const GizmoECS *gizmo, bool left_released);
    static void RecoverDragLifecycle(GizmoECS *gizmo, bool left_down, bool left_released);
    static void SyncActivePickFromChannelForDrag(GizmoECS *gizmo);

    static void StartDragCommonState(GizmoECS *gizmo,
                                     const math::Vector2i &mouse_coord,
                                     const math::Vector3f &prev_pos,
                                     const glm::quat &prev_rot,
                                     const math::Vector3f &prev_scale);
    static void StopDragCommonState(GizmoECS *gizmo);
    static bool BeginDragIfNeeded(GizmoECS *gizmo,
                                  const math::Vector2i &mouse_coord,
                                  const CameraInfo *camera_info,
                                  const ViewportInfo *viewport_info,
                                  hgl::ecs::InputSystem *input_system,
                                  bool has_view_context,
                                  const math::Vector3f &prev_pos,
                                  const glm::quat &prev_rot,
                                  const math::Vector3f &prev_scale);
    static void EndDragIfNeeded(GizmoECS *gizmo,
                                const math::Vector2i &mouse_coord,
                                const CameraInfo *camera_info,
                                const ViewportInfo *viewport_info);
    static void RecoverDragIfReleaseMissed(GizmoECS *gizmo);
    static void RefreshHoverState(GizmoECS *gizmo,
                                  const math::Vector2i &mouse_coord,
                                  const CameraInfo *camera_info,
                                  const ViewportInfo *viewport_info);
    static void UpdateRotateViewRingFacingToCamera(GizmoECS *gizmo, const CameraInfo *camera_info);
    static void SyncTargetToRootIfIdle(GizmoECS *gizmo,
                                       const std::shared_ptr<hgl::ecs::TransformComponent> &target_transform,
                                       bool has_view_context);
    static void CommitTransformChanges(GizmoECS *gizmo,
                                       const std::shared_ptr<hgl::ecs::TransformComponent> &target_transform,
                                       const math::Vector3f &prev_pos,
                                       const glm::quat &prev_rot,
                                       const math::Vector3f &prev_scale,
                                       const math::Vector3f &cur_effective_scale);

    static void PrepareUpdateFrameState(GizmoECS *gizmo,
                                        const CameraInfo *camera_info,
                                        const ViewportInfo *viewport_info,
                                        AssetUpdateFrameState &state);
    static bool RunDragUpdateStage(GizmoECS *gizmo,
                                   const math::Vector2i &mouse_coord,
                                   const CameraInfo *camera_info,
                                   const ViewportInfo *viewport_info,
                                   hgl::ecs::InputSystem *input_system,
                                   bool left_down,
                                   bool left_pressed,
                                   bool left_released,
                                   AssetUpdateFrameState &state);
    static void FinalizeUpdateFrameState(GizmoECS *gizmo,
                                         const CameraInfo *camera_info,
                                         AssetUpdateFrameState &state);

    void InitializeDefaultChannels()
    {
        channels_[0] = std::make_unique<MoveGizmoChannel>();
        channels_[1] = std::make_unique<RotateGizmoChannel>();
        channels_[2] = std::make_unique<ScaleGizmoChannel>();
    }

    IGizmoChannel *GetChannelForMode(GizmoMode mode)
    {
        for (auto &channel : channels_)
        {
            if (channel && channel->SupportsMode(mode))
                return channel.get();
        }

        return nullptr;
    }

    const IGizmoChannel *GetChannelForMode(GizmoMode mode) const
    {
        for (const auto &channel : channels_)
        {
            if (channel && channel->SupportsMode(mode))
                return channel.get();
        }

        return nullptr;
    }

private:
    std::array<std::unique_ptr<IGizmoChannel>, 3> channels_;
};

} // namespace hgl::graph
