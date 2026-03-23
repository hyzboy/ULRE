#pragma once

#include<hgl/graph/camera/ViewportInfo.h>
#include<hgl/vk/UBOAccessor.h>

namespace hgl::graph
{
using UBOViewportInfo=UBOAccessor<ViewportInfo,mtl::UBODescriptorSemantic::ViewportInfo>;
}
