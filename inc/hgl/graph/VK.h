#pragma once

#include<hgl/type/ArrayList.h>
#include<hgl/math/Math.h>
#include<hgl/type/String.h>
#include<hgl/type/Map.h>
#include<hgl/graph/VKNamespace.h>
#include<hgl/graph/VKFormat.h>
#include<hgl/graph/VKPrimitiveType.h>
#include<hgl/graph/VKStruct.h>
#include<hgl/graph/ViewportInfo.h>
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

struct VertexAttribDataPtr
{
    const char *    name;
    const VkFormat  format;
    const void *    data;
};

using BindingMap        =Map<AnsiString,int>;
using BindingMapArray   =BindingMap[VK_DESCRIPTOR_TYPE_RANGE_SIZE];

class DescriptorBinding;

class GraphModule;
class RenderFramework;

class VulkanInstance;
class VulkanPhyDevice;
class VulkanSurface;
class VulkanDevice;
struct VulkanDevAttr;
class DeviceQueue;
class ImageView;
class Framebuffer;
struct Swapchain;
class IRenderTarget;
class RenderTarget;
class MultiFrameRenderTarget;
class SwapchainRenderTarget;

struct CopyBufferToImageInfo;

class Texture;
class Texture1D;
class Texture1DArray;
class Texture2D;
class Texture2DArray;
class Texture3D;
class TextureCube;
class TextureCubeArray;

class Sampler;

class TileData;
class TextRender;

class DeviceMemory;
class DeviceBuffer;
struct DeviceBufferData;
template<typename T> class DeviceBufferMap;

struct MeshDataBuffer;
struct MeshRenderData;

class VertexAttribBuffer;
using VAB=VertexAttribBuffer;

class IndexBuffer;

class VABMap;
class IBMap;

class VulkanCmdBuffer;
class RenderCmdBuffer;
class TextureCmdBuffer;

class RenderPass;
class DeviceRenderPassManage;

class Fence;
class Semaphore;

struct PipelineLayoutData;
class DescriptorSet;
enum class DescriptorSetType;

enum class DescriptorType:uint32
{
    //对应glsl中的绑定的texture1D/2D/Cube等
    //如
    //  layout(set = 0, binding = 0) uniform texture2D myImage;
    //
    //  vec4 color = texelFetch(myImage, ivec2(x, y), lod);
    Texture             =VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,

    //对应glsl中的sampler类型
    // 如
    //  layout(set = 0, binding = 1) uniform sampler mySampler;
    Sampler             =VK_DESCRIPTOR_TYPE_SAMPLER,

    //以上两项要组合使用
    // 如
    //  vec4 color=texture(sampler2D(myImage,mySampler),texCoord);


    //对应glsl中的sampler2D/3D/Cube等
    // 如
    //  layout(set = 0, binding = 2) uniform sampler2D mySampler2D;
    //
    //  vec4 color=texture(mySampler2D,texCoord);
    TextureSampler      =VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,

    // 对应Image2D/3D/Cube类型，可读可写。
    // 如
    //  layout(set = 0, binding = 3, r32f) uniform image2D myImage;
    //
    // 
    //  vec4 value = imageLoad(myStorageImage, ivec2(x, y));                  //读取像素    
    //  imageStore(myStorageImage, ivec2(x, y), vec4(1.0, 0.0, 0.0, 1.0));    //写入像素
    RWImage             =VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,

    // 只读数组，对应glsl中的samplerBuffer
    // 如
    //  layout(set = 0, binding = 4) uniform samplerBuffer myTexelBuffer;
    //
    //  vec4 color=texelFetch(myTexelBuffer,texelIndex);
    Array               =VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,

    // 可读写数组，对应glsl中的imageBuffer
    // 如
    //  layout(set = 0, binding = 5) uniform imageBuffer myStorageBuffer;
    //
    //  float val = imageLoad(myStorageBuffer, index).x;  // 读取
    //  imageStore(myStorageBuffer, index, vec4(val * 2.0));  // 写入
    RWArray             =VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,

    UBO                 =VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    SSBO                =VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,

    DynamicUBO          =VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
    DynamicSSBO         =VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,

    InputAttachment     =VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,

    ENUM_CLASS_RANGE(Sampler,InputAttachment)
};

enum class ShaderStage:uint32_t
{
    Vertex                      = VK_SHADER_STAGE_VERTEX_BIT,
    TessControl                 = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
    TessEval                    = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
    Geometry                    = VK_SHADER_STAGE_GEOMETRY_BIT,
    Fragment                    = VK_SHADER_STAGE_FRAGMENT_BIT,
    Compute                     = VK_SHADER_STAGE_COMPUTE_BIT,
    Task                        = VK_SHADER_STAGE_TASK_BIT_EXT,
    Mesh                        = VK_SHADER_STAGE_MESH_BIT_EXT,
    ClusterCulling              = VK_SHADER_STAGE_CLUSTER_CULLING_BIT_HUAWEI,

    VertexFragment              = Vertex | Fragment,
    VertexGeometryFragment      = Vertex | Geometry | Fragment,
    Tessellation                = TessControl | TessEval,

    TaskMesh                    = Task | Mesh,
    TaskMeshFragment            = Task | Mesh | Fragment,

    AllGraphics                 = VK_SHADER_STAGE_ALL_GRAPHICS,

    ENUM_CLASS_RANGE(Vertex,ClusterCulling)
};

struct VertexInputAttribute;

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

struct VertexInputFormat;
using VIF=VertexInputFormat;

class VertexInputLayout;
using VIL=VertexInputLayout;

class PrimitiveData;
class Primitive;
class Mesh;

class VertexDataManager;
using VDM=VertexDataManager;

class IndirectDrawBuffer;
class IndirectDrawIndexedBuffer;
class IndirectDispatchBuffer;

class MeshComponent;

class SceneNode;
class Scene;
class RenderCollector;

struct CameraInfo;
struct Camera;

class Renderer;

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
enum IndexType:int
{
    AUTO=-1,
    U16=0,
    U32,
    U8=VK_INDEX_TYPE_UINT8_EXT,

    ERR=VK_INDEX_TYPE_MAX_ENUM
};

inline const bool IsIndexType(const IndexType it)
{
    return it>=U16&&it<=U8;
}

//Push Constant max-lengths:
//
//    256 bytes:  nvidia,arm
//    128 bytes:  amd,intel,powervr,adreno

inline void copy(VkExtent3D &e3d,const VkExtent2D &e2d,const uint32 depth=1)
{
    e3d.width   =e2d.width;
    e3d.height  =e2d.height;
    e3d.depth   =depth;
}

VK_NAMESPACE_END
