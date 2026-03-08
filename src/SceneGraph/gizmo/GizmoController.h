#pragma once

#include <array>
#include <memory>

#include "channels/IGizmoChannel.h"
#include "channels/MoveGizmoChannel.h"
#include "channels/RotateGizmoChannel.h"
#include "channels/ScaleGizmoChannel.h"

namespace hgl::graph
{

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
