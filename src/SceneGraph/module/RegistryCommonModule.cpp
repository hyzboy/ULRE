#include<hgl/graph/module/GraphModuleFactory.h>
#include<hgl/graph/module/SwapchainModule.h>

VK_NAMESPACE_BEGIN

void RegistryCommonGraphModule()
{
    REGISTRY_GRAPH_MODULE(Swapchain)
}

VK_NAMESPACE_END
