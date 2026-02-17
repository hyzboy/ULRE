#pragma once

/**
 * VulkanGraphicsContext.h - Deprecated header
 *
 * This file has been merged into GraphicsContext.h
 * Use #include <hgl/graph/core/GraphicsContext.h> instead
 *
 * Backward compatibility: VulkanGraphicsContext is now an alias for GraphicsContext
 */

#include <hgl/graph/core/GraphicsContext.h>

namespace hgl::graph
{
    // 向后兼容别名
    using VulkanGraphicsContext = GraphicsContext;
    using IGraphicsContext = GraphicsContext;  // 增加该别名以兼换旧代码
}
