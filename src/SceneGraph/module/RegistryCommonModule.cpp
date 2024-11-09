#include<hgl/graph/module/GraphModuleFactory.h>
#include<hgl/graph/manager/TextureManager.h>
#include<hgl/graph/manager/RenderPassManager.h>
#include<hgl/graph/module/SwapchainModule.h>

VK_NAMESPACE_BEGIN

void RegistryCommonGraphModule()
{
    REGISTRY_GRAPH_MODULE(TextureManager)
    REGISTRY_GRAPH_MODULE(RenderPassManager)
    REGISTRY_GRAPH_MODULE(SwapchainModule)
}

VK_NAMESPACE_END
