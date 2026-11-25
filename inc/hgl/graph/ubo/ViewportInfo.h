#pragma once

/**
 * 兼容入口：ViewportInfo 是 GPU ABI 契约（视口/正交投影参数块），
 * 已迁到后端无关的契约层，Vulkan 渲染目标与 SceneGraph 共用。
 * 新代码请直接 include <hgl/graphics/contracts/ViewportInfo.h>。
 */
#include<hgl/graphics/contracts/ViewportInfo.h>