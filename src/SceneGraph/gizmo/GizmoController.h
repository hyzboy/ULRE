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

    static ChannelSlot SlotForMode(GizmoMode mode)
    {
        if (mode == GizmoMode::MoveWorld || mode == GizmoMode::MoveLocal)
            return ChannelSlot::Move;
        if (mode == GizmoMode::RotateWorld || mode == GizmoMode::RotateLocal)
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
