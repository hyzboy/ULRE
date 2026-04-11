#pragma once

#include <hgl/common/VertexAttribDef.h>
#include <hgl/vk/VKFormat.h>

#include <absl/container/btree_map.h>

namespace hgl::graph
{
using VertexFormatMap = absl::btree_map<VertexAttrib, VkFormat>;
}