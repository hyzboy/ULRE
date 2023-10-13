#ifndef HGL_GRAPH_VULKAN_DEVICE_INCLUDE
#define HGL_GRAPH_VULKAN_DEVICE_INCLUDE

#include<hgl/type/List.h>
#include<hgl/type/String.h>
#include<hgl/type/Map.h>
#include<hgl/type/RectScope.h>
#include<hgl/graph/ImageRegion.h>
#include<hgl/platform/Window.h>
#include<hgl/graph/BitmapData.h>
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

struct CopyBufferToImageInfo;

class GPUDevice
{
    GPUDeviceAttribute *attr;

    DeviceQueue *texture_queue;
    TextureCmdBuffer *texture_cmd_buf;

private:

    DeviceRenderPassManage *render_pass_manage;
    RenderPass *device_render_pass;

    RTSwapchain *sc_rt;

    RTSwapchain *CreateSwapchainRenderTarget();

    void InitRenderPassManage();
    void ClearRenderPassManage();

private:

    VkCommandBuffer CreateCommandBuffer(const AnsiString &);

    bool CreateSwapchainFBO(Swapchain *);

    Swapchain *CreateSwapchain(const VkExtent2D &acquire_extent);

private:

    friend class VulkanDeviceCreater;

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

                RTSwapchain *       GetSwapchainRT      ()      {return sc_rt;}

    const       VkExtent2D &        GetSwapchainSize    ()const {return sc_rt->GetExtent();}

                void                WaitIdle            ()const {vkDeviceWaitIdle(attr->device);}

                DebugUtils *        GetDebugUtils       (){return attr->debug_utils;}

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
    IndexBuffer *   CreateIBO8  (                 uint32_t count,const void *  data,  SharingMode sm=SharingMode::Exclusive){return CreateIBO(IndexType::U8,  count,(void *)data,sm);}
    IndexBuffer *   CreateIBO16 (                 uint32_t count,const uint16 *data,  SharingMode sm=SharingMode::Exclusive){return CreateIBO(IndexType::U16, count,(void *)data,sm);}
    IndexBuffer *   CreateIBO32 (                 uint32_t count,const uint32 *data,  SharingMode sm=SharingMode::Exclusive){return CreateIBO(IndexType::U32, count,(void *)data,sm);}

    IndexBuffer *   CreateIBO   (IndexType type,  uint32_t count,                     SharingMode sm=SharingMode::Exclusive){return CreateIBO(type,           count,nullptr,sm);}
    IndexBuffer *   CreateIBO8  (                 uint32_t count,                     SharingMode sm=SharingMode::Exclusive){return CreateIBO(IndexType::U8,  count,nullptr,sm);}
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

    GPUArrayBuffer *CreateArrayInUBO(const VkDeviceSize &uint_size);
    GPUArrayBuffer *CreateArrayInSSBO(const VkDeviceSize &uint_size);

public: //Image

    VkImage CreateImage         (VkImageCreateInfo *);
    void    DestroyImage        (VkImage);

private:    //texture

    bool CopyBufferToImage      (const CopyBufferToImageInfo *info,VkPipelineStageFlags destinationStage);

    bool CopyBufferToImage      (Texture *,DeviceBuffer *buf,const VkBufferImageCopy *,const int count,const uint32_t base_layer,const uint32_t layer_count,VkPipelineStageFlags);//=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    bool CopyBufferToImage2D    (Texture *tex,DeviceBuffer *buf,const VkBufferImageCopy *bic_list,const int bic_count,  VkPipelineStageFlags dstStage){return CopyBufferToImage(tex,buf,bic_list,   bic_count,  0,1,dstStage);}
    bool CopyBufferToImage2D    (Texture *tex,DeviceBuffer *buf,const VkBufferImageCopy *bic,                           VkPipelineStageFlags dstStage){return CopyBufferToImage(tex,buf,bic,        1,          0,1,dstStage);}

    bool CopyBufferToImageCube  (Texture *tex,DeviceBuffer *buf,const VkBufferImageCopy *bic_list,const int bic_count,  VkPipelineStageFlags dstStage){return CopyBufferToImage(tex,buf,bic_list,   bic_count,  0,6,dstStage);}
    bool CopyBufferToImageCube  (Texture *tex,DeviceBuffer *buf,const VkBufferImageCopy *bic,                           VkPipelineStageFlags dstStage){return CopyBufferToImage(tex,buf,bic,        1,          0,6,dstStage);}

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

    Texture2DArray *CreateTexture2DArray(TextureData *);
    Texture2DArray *CreateTexture2DArray(TextureCreateInfo *ci);
    Texture2DArray *CreateTexture2DArray(const uint32_t w,const uint32_t h,const uint32 l,const VkFormat fmt,const bool mipmaps);

    TextureCube *CreateTextureCube(TextureData *);
    TextureCube *CreateTextureCube(TextureCreateInfo *ci);

    void Clear(TextureCreateInfo *);

    bool ChangeTexture2D(Texture2D *,DeviceBuffer *buf,             const List<Image2DRegion> &,VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    bool ChangeTexture2D(Texture2D *,DeviceBuffer *buf,             const RectScope2ui &,       VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    bool ChangeTexture2D(Texture2D *,void *data,const uint32_t size,const RectScope2ui &,       VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

//    bool ChangeTexture2DArray(Texture2DArray *,DeviceBuffer *buf,             const List<Image2DRegion> &,  const uint32_t base_layer,const uint32_t layer_count,VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    bool ChangeTexture2DArray(Texture2DArray *,DeviceBuffer *buf,             const RectScope2ui &,         const uint32_t base_layer,const uint32_t layer_count,VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    bool ChangeTexture2DArray(Texture2DArray *,void *data,const uint32_t size,const RectScope2ui &,         const uint32_t base_layer,const uint32_t layer_count,VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

public: //

    Sampler *CreateSampler(VkSamplerCreateInfo *sci=nullptr);
    Sampler *CreateSampler(Texture *);

public: //shader & material

    ShaderModule *CreateShaderModule(VkShaderStageFlagBits,const uint32_t *,const size_t);

    PipelineLayoutData *CreatePipelineLayoutData(const MaterialDescriptorManager *desc_manager);

    MaterialParameters *CreateMP(const MaterialDescriptorManager *desc_manager,const PipelineLayoutData *pld,const DescriptorSetType &desc_set_type);

public: //Command Buffer 相关

    RenderCmdBuffer * CreateRenderCommandBuffer(const AnsiString &);
    TextureCmdBuffer *CreateTextureCommandBuffer(const AnsiString &);
    
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
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DEVICE_INCLUDE
