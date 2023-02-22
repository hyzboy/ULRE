#ifndef HGL_GRAPH_VULKAN_INCLUDE
#define HGL_GRAPH_VULKAN_INCLUDE

#include<hgl/math/Math.h>
#include<hgl/type/List.h>
#include<hgl/type/String.h>
#include<hgl/type/Map.h>
#include<iostream>
#include<hgl/graph/VKNamespace.h>
#include<hgl/graph/VKFormat.h>
#include<hgl/graph/VKPrimitiveType.h>
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
class Queue;
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
class TextureCube;
class TextureCubeArray;

class Sampler;

class DeviceMemory;
class DeviceBuffer;
struct DeviceBufferData;

class VertexAttribBuffer;
using VBO=VertexAttribBuffer;

class IndexBuffer;

class GPUCmdBuffer;
class RenderCmdBuffer;
class TextureCmdBuffer;

class RenderPass;
class DeviceRenderPassManage;

class Fence;
class Semaphore;

enum class DescriptorSetsType
{
    //设计使其对应shader中的set
    
    Global=0,   ///<全局参数(如太阳光等)
    PerFrame,   ///<帧参数(如摄像机等)
    PerMaterial,///<材质参数(如纹理等)
    PerObject,  ///<对象参数(如模型矩阵等)

    Instance,
    Skeleton,

    ENUM_CLASS_RANGE(Global,Skeleton)
};//

constexpr char *DescriptSetsTypeName[]=
{
    "Global","PerFrame","PerMaterial","PerObject","Instance","Skeleton"
};

inline const char *GetDescriptorSetsTypeName(const enum class DescriptorSetsType &type)
{
    RANGE_CHECK_RETURN_NULLPTR(type);

    return DescriptSetsTypeName[(size_t)type];
}

struct PipelineLayoutData;
class DescriptorSet;

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

struct VAConfig
{
    VkFormat format;
    bool instance;

public:

    VAConfig()
    {
        format=PF_UNDEFINED;
        instance=false;
    }

    VAConfig(const VkFormat fmt,bool inst=false)
    {
        format=fmt;
        instance=inst;
    }

    CompOperatorMemcmp(const VAConfig &);
};

class VILConfig:public Map<AnsiString,VAConfig>
{
public:

    using Map<AnsiString,VAConfig>::Map;

    bool Add(const AnsiString &name,const VkFormat fmt,const bool inst=false)
    {
        return Map<AnsiString,VAConfig>::Add(name,VAConfig(fmt,inst));
    }
};

class VertexInputLayout;
using VIL=VertexInputLayout;

class Primitive;
class Renderable;

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

/**
 * 索引类型,等同于VkIndexType
 */
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
