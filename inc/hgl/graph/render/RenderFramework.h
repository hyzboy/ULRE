#pragma once

/**
 * RenderFramework.h - Backward compatibility header
 *
 * DEPRECATED: RenderFramework has been refactored into separate components:
 * - AppFramework: Application lifecycle management (window, device, events)
 * - VulkanGraphicsContext: Graphics resource management (implements IGraphicsContext)
 *
 * This file provides a type alias for backward compatibility.
 * New code should use AppFramework directly.
 *
 * Migration guide:
 * 1. Replace `#include<hgl/graph/render/RenderFramework.h>` with `#include<hgl/platform/AppFramework.h>`
 * 2. Replace `graph::RenderFramework` with `AppFramework`
 * 3. Replace `RenderFramework* rf` with `AppFramework* app`
 * 4. Access graphics context: `app->GetGraphicsContext()->GetXxxManager()`
 *
 * This compatibility header will be removed in a future version.
 */

#include <hgl/platform/AppFramework.h>

namespace hgl
{
    /**
     * @deprecated Use AppFramework instead
     * Type alias for backward compatibility
     */
    using RenderFramework = AppFramework;

    namespace graph
    {
        /**
         * @deprecated Use hgl::AppFramework instead
         * Type alias for backward compatibility in graph namespace
         */
        using RenderFramework = hgl::AppFramework;
    }
}

