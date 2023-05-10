#ifndef HGL_GRAPH_VULKAN_DEVICE_INCLUDE
#define HGL_GRAPH_VULKAN_DEVICE_INCLUDE

#include<hgl/type/List.h>
#include<hgl/type/String.h>
#include<hgl/type/Map.h>
#include<hgl/type/RectScope.h>
#include<hgl/platform/Window.h>
#include<hgl/graph/Bitmap.h>
#include<hgl/graph/font/Font.h>
#include<hgl/graph/VK.h>
#include<hgl/graph/VKDeviceAttribute.h>
#include<hgl/graph/VKSwapchain.h>
#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VertexAttribData.h>
#include<hgl/graph/VKShaderModuleMap.h>
#include<hgl/graph/VKArrayBuffer.h>
#include<hgl/graph/VKDescriptorSetType.h>

VK_NAMESPACE_BEGIN

class TileData;
class TileFont;
class FontSource;
class GPUArrayBuffer;

struct VulkanHardwareRequirement
{
    uint min_1d_image_size;
    uint min_2d_image_size;
    uint min_3d_image_size;
    uint min_cube_image_size;
    uint min_array_image_layers;

    uint min_vertex_input_attribute;            ///<最小顶点输入属性数量需求
    uint min_color_attachments;                 ///<最小颜色输出成份数量需求

    uint min_push_constant_size;                ///<最小push constant大小
    uint min_ubo_range;                         ///<最小ubo range需求
    uint min_ssbo_range;                        ///<最小ssbo range需求

    uint min_draw_indirect_count;               ///<最小间接绘制次数需求

    bool geometry_shader;                       ///<要求支持几何着色器
    bool tessellation_shader;                   ///<要求支持细分着色器
//    bool compute_shader;                        ///<要求支持计算着色器

    bool multi_draw_indirect;                   ///<要求支持MultiDrawIndirect

    bool wide_lines;                            ///<要求支持宽线条
    bool line_rasterization;                    ///<要支持线条特性
    bool large_points;                          ///<要求支持绘制大点

    bool texture_cube_array;                    ///<要求支持立方体数组纹理

    bool uint32_draw_index;                     ///<要求支持32位索引(不建议使用)

    struct
    {
        bool bc,etc2,astc_ldr,astc_hdr,pvrtc;   ///<要求支持的压缩纹理格式
    }texture_compression;

    //dynamic_state VK_EXT_extended_dynamic_state
    //  cull mode
    //  front face
    //  primitive topology
    //  viewport
    //  scissor
    //  bind vbo
    //  depth test
    //  depth write
    //  depth compare op
    //  depth bounds test
    //  stencil test
    //  stencil op
    //dynamic_state[1] VK_EXT_extended_dynamic_state2
    //  patch control points
    //  rasterizer discard
    //  depth bias
    //  logic op
    //  primitive restart
    //dynamic_state[2] VK_EXT_extended_dynamic_state3
    //  tess domain origin
    //  depth clamp
    //  discard polygon mode
    //  rasterization samples
    //  sample mask
    //  alpha to coverage
    //  alpha to one
    //  logic op enable
    //  color blend
    //  color blend equation
    //  color write mask
    //  depth clamp
    //  Color blend advanced
    //  line rasterization mode
    //  line stipple
    //  depth clip -1 to 1
    //  shading rate image enable
    bool dynamic_state[3];                      ///<要求支持动态状态

    // 1.3 特性
    bool dynamic_rendering;                     ///<要求支持动态渲染
};

struct VulkanDeviceCreateInfo
{
    VulkanInstance *instance;
    Window *window;
    const GPUPhysicalDevice *physical_device;

    VulkanHardwareRequirement require;
};

/*
 * GPU设备创建信息
 */
struct GPUDeviceCreateInfo
{
    VkPhysicalDeviceType    device_type             =VK_PHYSICAL_DEVICE_TYPE_MAX_ENUM;

    uint32_t                swapchain_image_count   =0;
    VkSurfaceFormatKHR      color_format            ={PF_A2BGR10UN,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    VkFormat                depth_format            =VK_FORMAT_UNDEFINED;
};//struct GPUDeviceCreateInfo

class GPUDevice
{
    GPUDeviceAttribute *attr;

    DeviceQueue *texture_queue;
    TextureCmdBuffer *texture_cmd_buf;

private:

    DeviceRenderPassManage *render_pass_manage;
    RenderPass *device_render_pass;

    SwapchainRenderTarget *swapchainRT;

    SwapchainRenderTarget *CreateSwapchainRenderTarget();

    void InitRenderPassManage();
    void ClearRenderPassManage();

private:

    VkCommandBuffer CreateCommandBuffer();

    bool CreateSwapchainFBO(Swapchain *);

    Swapchain *CreateSwapchain(const VkExtent2D &acquire_extent);

private:

    friend GPUDevice *CreateRenderDevice(VulkanDeviceCreateInfo *,VkSurfaceKHR surface,const VkExtent2D &extent);

    GPUDevice(GPUDeviceAttribute *da);

public:

    virtual ~GPUDevice();

    operator    VkDevice                                ()      {return attr->device;}
                GPUDeviceAttribute *GetDeviceAttribute  ()      {return attr;}

                VkSurfaceKHR        GetSurface          ()      {return attr->surface;}
                VkDevice            GetDevice           ()const {return attr->device;}
    const       GPUPhysicalDevice * GetPhysicalDevice   ()const {return attr->physical_device;}

                VkDescriptorPool    GetDescriptorPool   ()      {return attr->desc_pool;}
                VkPipelineCache     GetPipelineCache    ()      {return attr->pipeline_cache;}

    const       VkFormat            GetSurfaceFormat    ()const {return attr->surface_format.format;}
    const       VkColorSpaceKHR     GetColorSpace       ()const {return attr->surface_format.colorSpace;}
                VkQueue             GetGraphicsQueue    ()      {return attr->graphics_queue;}

                RenderPass *        GetRenderPass       ()      {return device_render_pass;}

    SwapchainRenderTarget *         GetSwapchainRT      ()      {return swapchainRT;}

    const       VkExtent2D &        GetSwapchainSize    ()const {return swapchainRT->GetExtent();}

                void                WaitIdle            ()const {vkDeviceWaitIdle(attr->device);}

public:

                bool                Resize              (const VkExtent2D &);
                bool                Resize              (const uint32_t &w,const uint32_t &h)
                {
                    VkExtent2D extent={w,h};

                    return Resize(extent);
                }

public: //内存相关

    DeviceMemory *CreateMemory(const VkMemoryRequirements &,const uint32_t properties);
    DeviceMemory *CreateMemory(VkImage,const uint32 flag=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

private: //Buffer相关

    bool            CreateBuffer(DeviceBufferData *buf,VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sharing_mode);
    bool            CreateBuffer(DeviceBufferData *buf,VkBufferUsageFlags buf_usage,                   VkDeviceSize size,const void *data,SharingMode sharing_mode){return CreateBuffer(buf,buf_usage,size,size,data,sharing_mode);}

public: //Buffer相关

    DeviceBuffer *  CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,   SharingMode sm=SharingMode::Exclusive);
    DeviceBuffer *  CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,                    SharingMode sm=SharingMode::Exclusive){return CreateBuffer(buf_usage,range,size,nullptr,sm);}
    
    DeviceBuffer *  CreateBuffer(VkBufferUsageFlags buf_usage,                   VkDeviceSize size,const void *data,   SharingMode sm=SharingMode::Exclusive){return CreateBuffer(buf_usage,size,size,data,sm);}
    DeviceBuffer *  CreateBuffer(VkBufferUsageFlags buf_usage,                   VkDeviceSize size,                    SharingMode sm=SharingMode::Exclusive){return CreateBuffer(buf_usage,size,size,nullptr,sm);}

    VBO *           CreateVBO   (VkFormat format, uint32_t count,const void *data,    SharingMode sm=SharingMode::Exclusive);
    VBO *           CreateVBO   (VkFormat format, uint32_t count,                     SharingMode sm=SharingMode::Exclusive){return CreateVBO(format,count,nullptr,sm);}
    VBO *           CreateVBO   (const VAD *vad,                                      SharingMode sm=SharingMode::Exclusive){return CreateVBO(vad->GetFormat(),vad->GetCount(),vad->GetData(),sm);}

    IndexBuffer *   CreateIBO   (IndexType type,  uint32_t count,const void *  data,  SharingMode sm=SharingMode::Exclusive);
    IndexBuffer *   CreateIBO16 (                 uint32_t count,const uint16 *data,  SharingMode sm=SharingMode::Exclusive){return CreateIBO(IndexType::U16, count,(void *)data,sm);}
    IndexBuffer *   CreateIBO32 (                 uint32_t count,const uint32 *data,  SharingMode sm=SharingMode::Exclusive){return CreateIBO(IndexType::U32, count,(void *)data,sm);}

    IndexBuffer *   CreateIBO   (IndexType type,  uint32_t count,                     SharingMode sm=SharingMode::Exclusive){return CreateIBO(type,           count,nullptr,sm);}
    IndexBuffer *   CreateIBO16 (                 uint32_t count,                     SharingMode sm=SharingMode::Exclusive){return CreateIBO(IndexType::U16, count,nullptr,sm);}
    IndexBuffer *   CreateIBO32 (                 uint32_t count,                     SharingMode sm=SharingMode::Exclusive){return CreateIBO(IndexType::U32, count,nullptr,sm);}

    const VkDeviceSize GetUBOAlign();
    const VkDeviceSize GetSSBOAlign();
    const VkDeviceSize GetUBORange();
    const VkDeviceSize GetSSBORange();

#define CREATE_BUFFER_OBJECT(LargeName,type)    DeviceBuffer *Create##LargeName(                   VkDeviceSize size,void *data,  SharingMode sm=SharingMode::Exclusive)  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,size ,size,data,      sm);} \
                                                DeviceBuffer *Create##LargeName(                   VkDeviceSize size,             SharingMode sm=SharingMode::Exclusive)  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,size ,size,nullptr,   sm);} \
                                                DeviceBuffer *Create##LargeName(VkDeviceSize range,VkDeviceSize size,void *data,  SharingMode sm=SharingMode::Exclusive)  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,range,size,data,      sm);} \
                                                DeviceBuffer *Create##LargeName(VkDeviceSize range,VkDeviceSize size,             SharingMode sm=SharingMode::Exclusive)  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,range,size,nullptr,   sm);}

    CREATE_BUFFER_OBJECT(UBO,UNIFORM)
    CREATE_BUFFER_OBJECT(SSBO,STORAGE)
    CREATE_BUFFER_OBJECT(INBO,INDIRECT)

#undef CREATE_BUFFER_OBJECT

    GPUArrayBuffer *CreateUBO(const VkDeviceSize &uint_size);
    GPUArrayBuffer *CreateSSBO(const VkDeviceSize &uint_size);

public: //Image

    VkImage CreateImage         (VkImageCreateInfo *);
    void    DestroyImage        (VkImage);

private:    //texture

    bool CommitTexture          (Texture *,DeviceBuffer *buf,const VkBufferImageCopy *,const int count,const uint32_t layer_count,VkPipelineStageFlags);//=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    bool CommitTexture2D        (Texture2D *,DeviceBuffer *buf,VkPipelineStageFlags stage);
    bool CommitTexture2DMipmaps (Texture2D *,DeviceBuffer *buf,const VkExtent3D &,uint32_t);

    bool CommitTextureCube          (TextureCube *,DeviceBuffer *buf,const uint32_t mipmaps_zero_bytes,VkPipelineStageFlags stage);
    bool CommitTextureCubeMipmaps   (TextureCube *,DeviceBuffer *buf,const VkExtent3D &,uint32_t);

    bool SubmitTexture          (const VkCommandBuffer *cmd_bufs,const uint32_t count=1);           ///<提交纹理处理到队列

public: //Texture

    bool CheckFormatSupport(const VkFormat,const uint32_t bits,ImageTiling tiling=ImageTiling::Optimal)const;

    bool CheckTextureFormatSupport(const VkFormat fmt,ImageTiling tiling=ImageTiling::Optimal)const{return CheckFormatSupport(fmt,VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,tiling);}

    Texture2D *CreateTexture2D(TextureData *);
    Texture2D *CreateTexture2D(TextureCreateInfo *ci);

    TextureCube *CreateTextureCube(TextureData *);
    TextureCube *CreateTextureCube(TextureCreateInfo *ci);

    void Clear(TextureCreateInfo *);

    bool ChangeTexture2D(Texture2D *,DeviceBuffer *buf, const List<Image2DRegion> &,                                            VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    bool ChangeTexture2D(Texture2D *,DeviceBuffer *buf, uint32_t left,uint32_t top,uint32_t width,uint32_t height,              VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    bool ChangeTexture2D(Texture2D *,void *data,        uint32_t left,uint32_t top,uint32_t width,uint32_t height,uint32_t size,VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    template<typename T>
    bool ChangeTexture2D(Texture2D *tex,DeviceBuffer *buf,const RectScope2<T> &rs)
    {
        return ChangeTexture2D( tex,
                                buf,
                                rs.GetLeft(),
                                rs.GetTop(),
                                rs.GetWidth(),
                                rs.GetHeight());
    }
    
    template<typename T>
    bool ChangeTexture2D(Texture2D *tex,void *data,const RectScope2<T> &rs,uint32_t size)
    {
        return ChangeTexture2D( tex,
                                data,
                                rs.GetLeft(),
                                rs.GetTop(),
                                rs.GetWidth(),
                                rs.GetHeight(),
                                size);
    }

public: //

    Sampler *CreateSampler(VkSamplerCreateInfo *sci=nullptr);
    Sampler *CreateSampler(Texture *);

public: //shader & material

    PipelineLayoutData *CreatePipelineLayoutData(const MaterialDescriptorManager *);
    void Destroy(PipelineLayoutData *);

    DescriptorSet *     CreateDS(const PipelineLayoutData *,const DescriptorSetType &type)const;
    MaterialParameters *CreateMP(const MaterialDescriptorManager *,const PipelineLayoutData *,const DescriptorSetType &);
    MaterialParameters *CreateMP(Material *,const DescriptorSetType &);
    
    ShaderModule *CreateShaderModule(VkShaderStageFlagBits,const uint32_t *,const size_t);
    
    Material *CreateMaterial(const UTF8String &mtl_name,ShaderModuleMap *shader_maps,MaterialDescriptorManager *,VertexInput *);

    MaterialInstance *CreateMI(Material *,const VILConfig *vil_cfg=nullptr);

public: //Command Buffer 相关

    RenderCmdBuffer * CreateRenderCommandBuffer();
    TextureCmdBuffer *CreateTextureCommandBuffer();
    
public:

    RenderPass * AcquireRenderPass(   const RenderbufferInfo *,const uint subpass_count=2);

    Fence *      CreateFence(bool);
    Semaphore *  CreateGPUSemaphore();

    DeviceQueue *CreateQueue(const uint32_t fence_count=1,const bool create_signaled=false);

public: //FrameBuffer相关

    Framebuffer *CreateFBO(RenderPass *rp,ImageView **color_list,const uint color_count,ImageView *depth);
//    Framebuffer *CreateFBO(RenderPass *,List<ImageView *> &color,ImageView *depth);
    Framebuffer *CreateFBO(RenderPass *,ImageView *color,ImageView *depth);
    Framebuffer *CreateFBO(RenderPass *,ImageView *);

public:
    
    RenderTarget *CreateRT(   const FramebufferInfo *fbi,RenderPass *,const uint32_t fence_count=1);
    RenderTarget *CreateRT(   const FramebufferInfo *fbi,const uint32_t fence_count=1);

public:

    TileData *CreateTileData(const VkFormat video_format,const uint width,const uint height,const uint count);          ///<创建一个Tile数据集
    
    TileFont *CreateTileFont(FontSource *fs,int limit_count=-1);                                                        ///<创建一个Tile字体
};//class GPUDevice

GPUDevice *CreateRenderDevice(VulkanDeviceCreateInfo *);
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DEVICE_INCLUDE
