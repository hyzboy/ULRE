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
