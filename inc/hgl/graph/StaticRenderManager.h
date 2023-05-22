#ifndef HGL_GRAPH_STATIC_RENDER_MANAGER_INCLUDE
#define HGL_GRAPH_STATIC_RENDER_MANAGER_INCLUDE

#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN

class RawMesh
{
};

/**
* 静态渲染管理器<br>
* 静态渲染指的是不会产生资源变动的内容，而不是指不会动的内容。
*/
class StaticRenderManager
{
    
public:

    virtual ~StaticRenderManager()=default;
    
};//class StaticRenderManager
VK_NAMESPACE_END
#endif//HGL_GRAPH_STATIC_RENDER_MANAGER_INCLUDE
