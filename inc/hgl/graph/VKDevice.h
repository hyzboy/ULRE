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

namespace hgl
{
    namespace graph
    {
        class TileData;
        class TileFont;
        class FontSource;
    }//namespace graph
}//namespace hgl

VK_NAMESPACE_BEGIN
class GPUDevice
{
    GPUDeviceAttribute *attr;

    GPUQueue *texture_queue;
    TextureCmdBuffer *texture_cmd_buf;

    Swapchain *swapchain;
    SwapchainRenderTarget *swapchainRT;

private:

    VkCommandBuffer CreateCommandBuffer();

    bool CreateSwapchainColorTexture();
    bool CreateSwapchainDepthTexture();

    bool CreateSwapchain(const VkExtent2D &acquire_extent);

private:

    friend GPUDevice *CreateRenderDevice(VkInstance inst,const GPUPhysicalDevice *physical_device,VkSurfaceKHR surface,const VkExtent2D &extent);

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

    const       VkFormat            GetSurfaceFormat    ()const {return attr->format;}
                VkQueue             GetGraphicsQueue    ()      {return attr->graphics_queue;}

                Swapchain *         GetSwapchain        ()      {return swapchain;}

    SwapchainRenderTarget *         GetSwapchainRT      ()      {return swapchainRT;}

public:

                const VkExtent2D &  GetSwapchainSize    ()const {return swapchain->extent;}

                bool                Resize              (const VkExtent2D &);
                bool                Resize              (const uint32_t &w,const uint32_t &h)
                {
                    VkExtent2D extent={w,h};

                    return Resize(extent);
                }

public: //

    template<typename T>
    T *GetProc(const char *name)
    {
        return reinterpret_cast<T>(vkGetDeviceProcAddr(attr->device,name));
    }

public: //内存相关

    GPUMemory *CreateMemory(const VkMemoryRequirements &,const uint32_t properties);
    GPUMemory *CreateMemory(VkImage,const uint32 flag=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

private: //Buffer相关

    bool            CreateBuffer(GPUBufferData *buf,VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sharing_mode);
    bool            CreateBuffer(GPUBufferData *buf,VkBufferUsageFlags buf_usage,                   VkDeviceSize size,const void *data,SharingMode sharing_mode){return CreateBuffer(buf,buf_usage,size,size,data,sharing_mode);}

public: //Buffer相关

    GPUBuffer *     CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,   SharingMode sm=SharingMode::Exclusive);
    GPUBuffer *     CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,                    SharingMode sm=SharingMode::Exclusive){return CreateBuffer(buf_usage,range,size,nullptr,sm);}

    GPUBuffer *     CreateBuffer(VkBufferUsageFlags buf_usage,                   VkDeviceSize size,const void *data,   SharingMode sm=SharingMode::Exclusive){return CreateBuffer(buf_usage,size,size,data,sm);}
    GPUBuffer *     CreateBuffer(VkBufferUsageFlags buf_usage,                   VkDeviceSize size,                    SharingMode sm=SharingMode::Exclusive){return CreateBuffer(buf_usage,size,size,nullptr,sm);}

    VAB *           CreateVAB   (VkFormat format, uint32_t count,const void *data,    SharingMode sm=SharingMode::Exclusive);
    VAB *           CreateVAB   (VkFormat format, uint32_t count,                     SharingMode sm=SharingMode::Exclusive){return CreateVAB(format,count,nullptr,sm);}
    VAB *           CreateVAB   (const VAD *vad,                                      SharingMode sm=SharingMode::Exclusive){return CreateVAB(vad->GetVulkanFormat(),vad->GetCount(),vad->GetData(),sm);}

    IndexBuffer *   CreateIBO   (IndexType type,  uint32_t count,const void *  data,  SharingMode sm=SharingMode::Exclusive);
    IndexBuffer *   CreateIBO16 (                 uint32_t count,const uint16 *data,  SharingMode sm=SharingMode::Exclusive){return CreateIBO(IndexType::U16, count,(void *)data,sm);}
    IndexBuffer *   CreateIBO32 (                 uint32_t count,const uint32 *data,  SharingMode sm=SharingMode::Exclusive){return CreateIBO(IndexType::U32, count,(void *)data,sm);}

    IndexBuffer *   CreateIBO   (IndexType type,  uint32_t count,                     SharingMode sm=SharingMode::Exclusive){return CreateIBO(type,           count,nullptr,sm);}
    IndexBuffer *   CreateIBO16 (                 uint32_t count,                     SharingMode sm=SharingMode::Exclusive){return CreateIBO(IndexType::U16, count,nullptr,sm);}
    IndexBuffer *   CreateIBO32 (                 uint32_t count,                     SharingMode sm=SharingMode::Exclusive){return CreateIBO(IndexType::U32, count,nullptr,sm);}

    const VkDeviceSize GetUBOAlign();

#define CREATE_BUFFER_OBJECT(LargeName,type)    GPUBuffer *Create##LargeName(                   VkDeviceSize size,void *data,  SharingMode sm=SharingMode::Exclusive)  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,size ,size,data,      sm);} \
                                                GPUBuffer *Create##LargeName(                   VkDeviceSize size,             SharingMode sm=SharingMode::Exclusive)  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,size ,size,nullptr,   sm);} \
                                                GPUBuffer *Create##LargeName(VkDeviceSize range,VkDeviceSize size,void *data,  SharingMode sm=SharingMode::Exclusive)  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,range,size,data,      sm);} \
                                                GPUBuffer *Create##LargeName(VkDeviceSize range,VkDeviceSize size,             SharingMode sm=SharingMode::Exclusive)  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,range,size,nullptr,   sm);}

    CREATE_BUFFER_OBJECT(UBO,UNIFORM)
    CREATE_BUFFER_OBJECT(SSBO,STORAGE)
    CREATE_BUFFER_OBJECT(INBO,INDIRECT)

#undef CREATE_BUFFER_OBJECT

public: //Image

    VkImage CreateImage         (VkImageCreateInfo *);
    void    DestoryImage        (VkImage);

private:    //texture

    bool CommitTexture2D        (Texture2D *,GPUBuffer *buf,uint32_t width,uint32_t height,const uint32_t miplevel,VkPipelineStageFlags stage);
    bool CommitTexture2DMipmaps (Texture2D *,GPUBuffer *buf,uint32_t width,uint32_t height,uint32_t miplevel,uint32_t);
    bool CommitTexture2D        (Texture2D *,GPUBuffer *buf,const VkBufferImageCopy *,const int count,const uint32_t miplevel=1,VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

public: //Texture

    bool CheckFormatSupport(const VkFormat,const uint32_t bits,ImageTiling tiling=ImageTiling::Optimal)const;

    bool CheckTextureFormatSupport(const VkFormat fmt,ImageTiling tiling=ImageTiling::Optimal)const{return CheckFormatSupport(fmt,VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,tiling);}

    Texture2D *CreateTexture2D(TextureData *);
    Texture2D *CreateTexture2D(TextureCreateInfo *ci);

    void Clear(TextureCreateInfo *);

    bool ChangeTexture2D(Texture2D *,GPUBuffer *buf,const List<ImageRegion> &,                                              const uint32_t miplevel=1,VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    bool ChangeTexture2D(Texture2D *,GPUBuffer *buf,uint32_t left,uint32_t top,uint32_t width,uint32_t height,              const uint32_t miplevel=1,VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    bool ChangeTexture2D(Texture2D *,void *data,    uint32_t left,uint32_t top,uint32_t width,uint32_t height,uint32_t size,const uint32_t miplevel=1,VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    template<typename T>
    bool ChangeTexture2D(Texture2D *tex,GPUBuffer *buf,const RectScope2<T> &rs)
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

public: //shader & material

    DescriptorSetLayoutCreater *CreateDescriptorSetLayoutCreater();
    
    ShaderModule *CreateShaderModule(ShaderResource *);
    
    Material *CreateMaterial(const UTF8String &mtl_name,ShaderModuleMap *shader_maps);    
    Material *CreateMaterial(const UTF8String &mtl_name,const VertexShaderModule *vertex_shader_module,const ShaderModule *fragment_shader_module);
    Material *CreateMaterial(const UTF8String &mtl_name,const VertexShaderModule *vertex_shader_module,const ShaderModule *geometry_shader_module,const ShaderModule *fragment_shader_module);

public: //Command Buffer 相关

    RenderCmdBuffer * CreateRenderCommandBuffer();
    TextureCmdBuffer *CreateTextureCommandBuffer();
    
    RenderPass *    CreateRenderPass(   const List<VkAttachmentDescription> &desc_list,
                                        const List<VkSubpassDescription> &subpass,
                                        const List<VkSubpassDependency> &dependency,
                                        const RenderbufferInfo *);

    RenderPass *    CreateRenderPass(   const RenderbufferInfo *);

    GPUFence *      CreateFence(bool);
    GPUSemaphore *  CreateSemaphore();

public: //FrameBuffer相关

    Framebuffer *CreateFramebuffer(RenderPass *rp,ImageView **color_list,const uint color_count,ImageView *depth);
//    Framebuffer *CreateFramebuffer(RenderPass *,List<ImageView *> &color,ImageView *depth);
    Framebuffer *CreateFramebuffer(RenderPass *,ImageView *color,ImageView *depth);
    Framebuffer *CreateFramebuffer(RenderPass *,ImageView *);

public:

    bool SubmitTexture      (const VkCommandBuffer *cmd_bufs,const uint32_t count=1);           ///<提交纹理处理到队列
    
    RenderTarget *CreateRenderTarget(   Framebuffer *,const uint32_t fence_count=1);

    RenderTarget *CreateRenderTarget(   const FramebufferInfo *fbi,const uint32_t fence_count=1);
    
public:

    Pipeline *CreatePipeline(const InlinePipeline &,const Material *,const RenderTarget *);
    Pipeline *CreatePipeline(      PipelineData *,  const Material *,const RenderTarget *);

public:

    TileData *CreateTileData(const VkFormat video_format,const uint width,const uint height,const uint count);          ///<创建一个Tile数据集
    
    TileFont *CreateTileFont(FontSource *fs,int limit_count=-1);                                                  ///<创建一个Tile字体
};//class GPUDevice

//void CreateSubpassDependency(VkSubpassDependency *);
void CreateSubpassDependency(List<VkSubpassDependency> &dependency,const uint32_t count);

void CreateAttachmentReference(VkAttachmentReference *ref_list,uint start,uint count,VkImageLayout layout);

inline void CreateColorAttachmentReference(VkAttachmentReference *ref_list, uint start,uint count)  {CreateAttachmentReference(ref_list,    start,count,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);}
inline void CreateDepthAttachmentReference(VkAttachmentReference *depth_ref,uint index)             {CreateAttachmentReference(depth_ref,   index,1    ,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);}
inline void CreateInputAttachmentReference(VkAttachmentReference *ref_list, uint start,uint count)  {CreateAttachmentReference(ref_list,    start,count,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);}

    
bool CreateColorAttachment( List<VkAttachmentReference> &ref_list,List<VkAttachmentDescription> &desc_list,const List<VkFormat> &color_format,const VkImageLayout color_final_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
bool CreateDepthAttachment( List<VkAttachmentReference> &ref_list,List<VkAttachmentDescription> &desc_list,const VkFormat &depth_format,const VkImageLayout depth_final_layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

bool CreateAttachmentDescription( List<VkAttachmentDescription> &color_output_desc_list,
                            const List<VkFormat> &color_format,
                            const VkFormat depth_format,
                            const VkImageLayout color_final_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            const VkImageLayout depth_final_layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

GPUDevice *CreateRenderDevice(VulkanInstance *inst,Window *win,const GPUPhysicalDevice *physical_device=nullptr);
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DEVICE_INCLUDE
