#ifndef HGL_GRAPH_VULKAN_INCLUDE
#define HGL_GRAPH_VULKAN_INCLUDE

#include<hgl/type/List.h>
#include<hgl/math/Math.h>
#include<iostream>
#include<hgl/graph/vulkan/VKNamespace.h>
#include<hgl/graph/vulkan/VKFormat.h>
#include<hgl/graph/vulkan/VKPrimivate.h>

VK_NAMESPACE_BEGIN

class Instance;
class PhysicalDevice;
class Device;
struct DeviceAttribute;
class ImageView;
class Framebuffer;
struct Swapchain;
class RenderTarget;
class SwapchainRenderTarget;

class Texture;
class Texture1D;
class Texture1DArray;
class Texture2D;
class Texture2DArray;
class Texture3D;
class TextureCubemap;
class TextureCubemapArray;

class Sampler;

class Memory;
class Buffer;
class VertexBuffer;
class IndexBuffer;

class CommandBuffer;
class RenderPass;
class Fence;
class Semaphore;

class ShaderModule;
class ShaderModuleManage;
class VertexShaderModule;
class Material;
class PipelineLayout;
class Pipeline;
class DescriptorSets;
class VertexAttributeBinding;

class Renderable;

enum class ShaderStage
{
    Vertex      =VK_SHADER_STAGE_VERTEX_BIT,
    TessControl =VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
    TessEval    =VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
    Geometry    =VK_SHADER_STAGE_GEOMETRY_BIT,
    Fragment    =VK_SHADER_STAGE_FRAGMENT_BIT,
    Compute     =VK_SHADER_STAGE_COMPUTE_BIT
};//enum class ShaderStage

struct PushConstant
{
    Matrix4f local_to_world;
};

inline void copy(VkExtent3D &e3d,const VkExtent2D &e2d)
{
    e3d.width   =e2d.width;
    e3d.height  =e2d.height;
    e3d.depth   =1;
}

inline void debug_out(const hgl::List<VkLayerProperties> &layer_properties)
{
    const int property_count=layer_properties.GetCount();

    if(property_count<=0)return;

    const VkLayerProperties *lp=layer_properties.GetData();

    for(int i=0;i<property_count;i++)
    {
        std::cout<<"Layer Propertyes ["<<i<<"] : "<<lp->layerName<<" desc: "<<lp->description<<std::endl;
        ++lp;
    }
}

inline void debug_out(const hgl::List<VkExtensionProperties> &extension_properties)
{
    const int extension_count=extension_properties.GetCount();

    if(extension_count<=0)return;

    VkExtensionProperties *ep=extension_properties.GetData();
    for(int i=0;i<extension_count;i++)
    {
        std::cout<<"Extension Propertyes ["<<i<<"] : "<<ep->extensionName<<" ver: "<<ep->specVersion<<std::endl;
        ++ep;
    }
}
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_INCLUDE
