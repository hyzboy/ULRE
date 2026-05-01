#pragma once

#include<hgl/platform/Platform.h>

namespace hgl::graph{

/**
* 可渲染对象创建描述符
*/
struct RenderableDesc
{
    enum class Backend : uint8 { VBO, Pulling };

    Backend backend = Backend::VBO;   ///< C 阶段默认 VBO；D 阶段默认 Pulling
};//struct RenderableDesc

}//namespace hgl::graph
