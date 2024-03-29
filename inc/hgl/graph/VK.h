﻿#ifndef HGL_GRAPH_VULKAN_INCLUDE
#define HGL_GRAPH_VULKAN_INCLUDE

#include<hgl/math/Math.h>
#include<hgl/type/String.h>
#include<hgl/type/Map.h>
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
class DeviceQueue;
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

struct PipelineLayoutData;
class DescriptorSet;

struct ShaderAttribute;

class ShaderResource;
class ShaderModule;
class ShaderModuleMap;
class MaterialDescriptorManager;

class Material;
class MaterialParameters;
class MaterialInstance;
struct PipelineData;
enum class InlinePipeline;
class Pipeline;

struct VAConfig;
class VILConfig;
class VertexInput;

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
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_INCLUDE
