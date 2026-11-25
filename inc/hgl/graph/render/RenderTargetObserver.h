#pragma once

#include <stdint.h>

namespace hgl::graph
{
    class IRenderTarget;

    class RenderTargetObserver
    {
    public:
        virtual void OnSwapchainTargetChanged(IRenderTarget *target) = 0;
        virtual void OnRenderTargetResized(IRenderTarget *target, uint32_t width, uint32_t height) = 0;

    protected:
        ~RenderTargetObserver() = default;
    };
}
