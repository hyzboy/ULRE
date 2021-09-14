#ifndef HGL_GRAPH_VULKAN_INCLUDE
#define HGL_GRAPH_VULKAN_INCLUDE

#include<hgl/math/Math.h>
#include<hgl/type/List.h>
#include<hgl/type/String.h>
#include<hgl/type/Map.h>
#include<iostream>
#include<hgl/graph/VKNamespace.h>
#include<hgl/graph/VKFormat.h>
#include<hgl/graph/VKPrimivate.h>
#include<hgl/graph/VKStruct.h>
#include<hgl/graph/VKRenderbufferInfo.h>

VK_NAMESPACE_BEGIN

#ifndef VK_DESCRIPTOR_TYPE_BEGIN_RANGE
constexpr size_t VK_DESCRIPTOR_TYPE_BEGIN_RANGE=VK_DESCRIPTOR_TYPE_SAMPLER;
#endif//VK_DESCRIPTOR_TYPE_BEGIN_RANGE

#ifndef VK_DESCRIPTOR_TYPE_END_RANGE
constexpr size_t VK_DESCRIPTOR_TYPE_END_RANGE=VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
#endif//VK_DESCRIPTOR_TYPE_END_RANGE
    
#ifndef VK_DESCRIPTOR_TYPE_RANGE_SIZE
constexpr size_t VK_DESCRIPTOR_TYPE_RANGE_SIZE=VK_DESCRIPTOR_TYPE_END_RANGE-VK_DESCRIPTOR_TYPE_BEGIN_RANGE+1;
#endif//VK_DESCRIPTOR_TYPE_RANGE_SIZE

using CharPointerList=hgl::List<const char *>;
using BindingMapping=Map<uint32_t,int>;

class VulkanInstance;
class GPUPhysicalDevice;
class GPUDevice;
struct GPUDeviceAttribute;
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

class GPUMemory;
class GPUBuffer;
struct GPUBufferData;

class VertexAttribBuffer;
using VAB=VertexAttribBuffer;

class IndexBuffer;

class GPUCmdBuffer;
class RenderCmdBuffer;
class TextureCmdBuffer;
class RenderPass;
class GPUFence;
class GPUSemaphore;

enum class DescriptorSetType
{
    //设计使其对应shader中的set
    
    Global=0,   ///<全局参数(如太阳光等)
    Material,   ///<材质中永远不变的参数
//    Texture,    ///<材质中的纹理参数
    Value,      ///<材质中的变量参数
    Renderable, ///<渲染实例参数(如Local2World matrix)

    ENUM_CLASS_RANGE(Global,Renderable)
};//

const DescriptorSetType CheckDescriptorSetType(const char *str);

struct PipelineLayoutData;
class DescriptorSets;

struct ShaderStage;

class ShaderResource;
class ShaderModule;
class VertexShaderModule;
class ShaderModuleMap;
class MaterialDescriptorSets;

class Material;
class MaterialParameters;
class MaterialInstance;
struct PipelineData;
enum class InlinePipeline;
class Pipeline;
class VertexAttributeBinding;

class Renderable;
class RenderableInstance;

class RenderResource;

enum class SharingMode
{
    Exclusive = 0,
    Concurrent
};//

enum class Filter
{
    Nearest=0,
    Linear,
};//

enum ImageTiling
{
    Optimal=0,
    Linear
};//

inline const uint32_t GetMipLevel(const VkExtent2D &ext)
{
    return hgl::GetMipLevel(ext.width,ext.height);
}

inline const uint32_t GetMipLevel(const VkExtent3D &ext)
{
    return hgl::GetMipLevel(ext.width,ext.height,ext.depth);
}

enum IndexType
{
    U16=0,
    U32
};

enum class ShaderStageBit
{
    Vertex      =VK_SHADER_STAGE_VERTEX_BIT,
    TessControl =VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
    TessEval    =VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
    Geometry    =VK_SHADER_STAGE_GEOMETRY_BIT,
    Fragment    =VK_SHADER_STAGE_FRAGMENT_BIT,
    Compute     =VK_SHADER_STAGE_COMPUTE_BIT
};//enum class ShaderStageBit

inline const uint GetShaderCountByBits(const uint32_t bits)
{
    uint comp=(uint)VK_SHADER_STAGE_VERTEX_BIT;
    uint result=0;

    for(uint i=0;i<6;i++)
    {
        if(bits&comp)
            ++result;

        comp<<=1;
    }

    return result;
}

/**
 * max-lengths:
 *
        256 bytes:  nvidia,arm
        128 bytes:  amd,intel,powervr,adreno
 */
struct PushConstant
{
    Matrix4f local_to_world;
    Matrix4f normal;
};

constexpr uint32_t MAX_PUSH_CONSTANT_BYTES=sizeof(PushConstant);

inline void copy(VkExtent3D &e3d,const VkExtent2D &e2d,const uint32 depth=1)
{
    e3d.width   =e2d.width;
    e3d.height  =e2d.height;
    e3d.depth   =depth;
}

inline void debug_out_vk_version(const uint32_t version)
{
    std::cout<<VK_VERSION_MAJOR(version)<<"."
             <<VK_VERSION_MINOR(version)<<"."
             <<VK_VERSION_PATCH(version);
}

template<typename T>
inline hgl::String<T> VkUUID2String(const uint8_t *pipelineCacheUUID)
{
    T *hstr=new T[VK_UUID_SIZE*2+1];
    
    DataToLowerHexStr(hstr,pipelineCacheUUID,VK_UUID_SIZE);

    return hgl::String<T>::newOf(hstr,VK_UUID_SIZE*2);
}

inline void debug_out(const char *front,const hgl::List<VkLayerProperties> &layer_properties)
{
    const int property_count=layer_properties.GetCount();

    if(property_count<=0)return;

    const VkLayerProperties *lp=layer_properties.GetData();

    for(int i=0;i<property_count;i++)
    {
        std::cout<<front<<" Layer Propertyes ["<<i<<"] : "<<lp->layerName<<" [spec: ";        
        debug_out_vk_version(lp->specVersion);
        
        std::cout<<", impl: ";
        debug_out_vk_version(lp->implementationVersion);

        std::cout<<"] desc: "<<lp->description<<std::endl;
        ++lp;
    }
}

inline void debug_out(const char *front,const hgl::List<VkExtensionProperties> &extension_properties)
{
    const int extension_count=extension_properties.GetCount();

    if(extension_count<=0)return;

    VkExtensionProperties *ep=extension_properties.GetData();
    for(int i=0;i<extension_count;i++)
    {
        std::cout<<front<<" Extension Propertyes ["<<i<<"] : "<<ep->extensionName<<" ver: ";
        
        debug_out_vk_version(ep->specVersion);
        
        std::cout<<std::endl;
        ++ep;
    }
}
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_INCLUDE
