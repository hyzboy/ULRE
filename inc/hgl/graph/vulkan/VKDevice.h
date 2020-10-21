#ifndef HGL_GRAPH_RENDER_SURFACE_INCLUDE
#define HGL_GRAPH_RENDER_SURFACE_INCLUDE

#include<hgl/type/List.h>
#include<hgl/type/String.h>
#include<hgl/type/Map.h>
#include<hgl/type/RectScope.h>
#include<hgl/platform/Window.h>
#include<hgl/graph/Bitmap.h>
#include<hgl/graph/font/Font.h>
#include<hgl/graph/vulkan/VK.h>
#include<hgl/graph/vulkan/VKDeviceAttribute.h>
#include<hgl/graph/vulkan/VKSwapchain.h>
#include<hgl/graph/vulkan/VKRenderTarget.h>
#include<hgl/graph/VertexAttribData.h>
#include<hgl/graph/vulkan/VKShaderModuleMap.h>

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
class Device
{
    DeviceAttribute *attr;

    SubmitQueue *textureSQ;
    CommandBuffer *texture_cmd_buf;

    Swapchain *swapchain;
    SwapchainRenderTarget *swapchainRT;

    bool CreateSwapchainColorTexture();
    bool CreateSwapchainDepthTexture();

    bool CreateSwapchain(const VkExtent2D &acquire_extent);

private:

    friend Device *CreateRenderDevice(VkInstance inst,const PhysicalDevice *physical_device,VkSurfaceKHR surface,const VkExtent2D &extent);

    Device(DeviceAttribute *da);

public:

    virtual ~Device();

    operator    VkDevice                                ()      {return attr->device;}
                DeviceAttribute *   GetDeviceAttribute  ()      {return attr;}

                VkSurfaceKHR        GetSurface          ()      {return attr->surface;}
                VkDevice            GetDevice           ()const {return attr->device;}
    const       PhysicalDevice *    GetPhysicalDevice   ()const {return attr->physical_device;}

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

public: //内存相关

    Memory *CreateMemory(const VkMemoryRequirements &,const uint32_t properties);
    Memory *CreateMemory(VkImage,const uint32 flag=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

private: //Buffer相关

    bool            CreateBuffer(BufferData *buf,VkBufferUsageFlags buf_usage,VkDeviceSize size,const void *data,SharingMode sharing_mode);

public: //Buffer相关

    Buffer *        CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize size,const void *data,   SharingMode sharing_mode=SharingMode::Exclusive);
    Buffer *        CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize size,                    SharingMode sharing_mode=SharingMode::Exclusive){return CreateBuffer(buf_usage,size,nullptr,sharing_mode);}

    VAB *           CreateVAB   (VkFormat format,uint32_t count,const void *data,   SharingMode sharing_mode=SharingMode::Exclusive);
    VAB *           CreateVAB   (VkFormat format,uint32_t count,                    SharingMode sharing_mode=SharingMode::Exclusive){return CreateVAB(format,count,nullptr,sharing_mode);}
    VAB *           CreateVAB   (const VAD *vad,                                    SharingMode sharing_mode=SharingMode::Exclusive){return CreateVAB(vad->GetVulkanFormat(),vad->GetCount(),vad->GetData(),sharing_mode);}

    IndexBuffer *   CreateIBO   (IndexType index_type,uint32_t count,const void *  data,SharingMode sharing_mode=SharingMode::Exclusive);
    IndexBuffer *   CreateIBO16 (                     uint32_t count,const uint16 *data,SharingMode sharing_mode=SharingMode::Exclusive){return CreateIBO(IndexType::U16,count,(void *)data,sharing_mode);}
    IndexBuffer *   CreateIBO32 (                     uint32_t count,const uint32 *data,SharingMode sharing_mode=SharingMode::Exclusive){return CreateIBO(IndexType::U32,count,(void *)data,sharing_mode);}

    IndexBuffer *   CreateIBO   (IndexType index_type,uint32_t count,SharingMode sharing_mode=SharingMode::Exclusive){return CreateIBO(index_type,count,nullptr,sharing_mode);}
    IndexBuffer *   CreateIBO16 (                     uint32_t count,SharingMode sharing_mode=SharingMode::Exclusive){return CreateIBO(IndexType::U16,count,nullptr,sharing_mode);}
    IndexBuffer *   CreateIBO32 (                     uint32_t count,SharingMode sharing_mode=SharingMode::Exclusive){return CreateIBO(IndexType::U32,count,nullptr,sharing_mode);}

#define CREATE_BUFFER_OBJECT(LargeName,type)    Buffer *Create##LargeName(VkDeviceSize size,void *data,SharingMode sharing_mode=SharingMode::Exclusive){return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,size,data,sharing_mode);}  \
                                                Buffer *Create##LargeName(VkDeviceSize size,SharingMode sharing_mode=SharingMode::Exclusive){return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,size,nullptr,sharing_mode);}

    CREATE_BUFFER_OBJECT(UBO,UNIFORM)
    CREATE_BUFFER_OBJECT(SSBO,STORAGE)
    CREATE_BUFFER_OBJECT(INBO,INDIRECT)

#undef CREATE_BUFFER_OBJECT

public: //Image

    VkImage CreateImage         (VkImageCreateInfo *);
    void    DestoryImage        (VkImage);

public: //Texture

    bool CheckFormatSupport(const VkFormat,const uint32_t bits,ImageTiling tiling=ImageTiling::Optimal)const;

    bool CheckTextureFormatSupport(const VkFormat fmt,ImageTiling tiling=ImageTiling::Optimal)const{return CheckFormatSupport(fmt,VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,tiling);}

    Texture2D *CreateTexture2D(TextureData *);
    Texture2D *CreateTexture2D(TextureCreateInfo *ci);

    void Clear(TextureCreateInfo *);

    Texture2D *CreateTexture2D(Memory *mem,VkImage image,ImageView *image_view,VkImageLayout image_layout,ImageTiling tiling);
    Texture2D *CreateTexture2D(VkFormat format,uint32_t width,uint32_t height,VkImageAspectFlags aspectMask,VkImage image,VkImageLayout image_layout,ImageTiling tiling=ImageTiling::Optimal);

    Texture2D *CreateTexture2D(const VkFormat format,uint32_t width,uint32_t height,const VkImageAspectFlags aspectMask,const uint usage,const VkImageLayout image_layout,ImageTiling tiling=ImageTiling::Optimal);

    Texture2D *CreateTexture2DColor(const VkFormat video_format,uint32_t width,uint32_t height,const ImageTiling tiling=ImageTiling::Optimal)
    {
        return CreateTexture2D(video_format,width,height,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
                (tiling==ImageTiling::Optimal?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_GENERAL),
                tiling);
    }

    Texture2D *CreateTexture2DDepth(const VkFormat video_format,uint32_t width,uint32_t height,const ImageTiling tiling=ImageTiling::Optimal)
    {
        return CreateTexture2D(video_format,width,height,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
                (tiling==ImageTiling::Optimal?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_GENERAL),
                tiling);
    }

    Texture2D *CreateAttachmentTexture(const VkFormat video_format,uint32_t width,uint32_t height,const VkImageAspectFlags aspectMask,const uint usage,const VkImageLayout image_layout)
    {
        return CreateTexture2D(video_format,width,height,aspectMask,usage|VK_IMAGE_USAGE_SAMPLED_BIT,image_layout);
    }

    Texture2D *CreateAttachmentTextureColor(const VkFormat video_format,uint32_t width,uint32_t height)
    {
        return CreateAttachmentTexture( video_format,width,height,
                                        VK_IMAGE_ASPECT_COLOR_BIT,
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }

    Texture2D *CreateAttachmentTextureDepth(const VkFormat video_format,uint32_t width,uint32_t height)
    {
        return CreateAttachmentTexture( video_format,width,height,
                                        VK_IMAGE_ASPECT_DEPTH_BIT,
                                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }

    Texture2D *CreateAttachmentTextureDepthStencil(const VkFormat video_format,uint32_t width,uint32_t height)
    {
        return CreateAttachmentTexture( video_format,width,height,
                                        VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,
                                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }

    Texture2D *CreateTexture2D( const VkFormat video_format,Buffer *buf,uint32_t width,uint32_t height,
                                const VkImageAspectFlags    aspectMask  =VK_IMAGE_ASPECT_COLOR_BIT,
                                const uint                  usage       =VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
                                const VkImageLayout         image_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                const ImageTiling           tiling      =ImageTiling::Optimal);

    Texture2D *CreateTexture2D( const VkFormat video_format,void *data,uint32_t width,uint32_t height,uint32_t size,
                                const VkImageAspectFlags    aspectMask  =VK_IMAGE_ASPECT_COLOR_BIT,
                                const uint                  usage       =VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
                                const VkImageLayout         image_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                const ImageTiling           tiling      =ImageTiling::Optimal);

    bool ChangeTexture2D(Texture2D *,Buffer *buf,const VkBufferImageCopy *,const int count);
    bool ChangeTexture2D(Texture2D *,Buffer *buf,const List<ImageRegion> &);

    bool ChangeTexture2D(Texture2D *,Buffer *buf,uint32_t left,uint32_t top,uint32_t width,uint32_t height);
    bool ChangeTexture2D(Texture2D *,void *data,uint32_t left,uint32_t top,uint32_t width,uint32_t height,uint32_t size);

    template<typename T>
    bool ChangeTexture2D(Texture2D *tex,Buffer *buf,const RectScope2<T> &rs)
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
    
    Material *CreateMaterial(ShaderModuleMap *shader_maps);    
    Material *CreateMaterial(const VertexShaderModule *vertex_shader_module,const ShaderModule *fragment_shader_module);
    Material *CreateMaterial(const VertexShaderModule *vertex_shader_module,const ShaderModule *geometry_shader_module,const ShaderModule *fragment_shader_module);

public: //Command Buffer 相关

    CommandBuffer * CreateCommandBuffer(const VkExtent2D &extent,const uint32_t atta_count);
    
    RenderPass *    CreateRenderPass(   const List<VkAttachmentDescription> &desc_list,
                                        const List<VkSubpassDescription> &subpass,
                                        const List<VkSubpassDependency> &dependency,
                                        const List<VkFormat> &color_format,
                                        const VkFormat depth_format,
                                        const VkImageLayout color_final_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        const VkImageLayout depth_final_layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    RenderPass *    CreateRenderPass(   const List<VkFormat> &color_format,
                                        const VkFormat depth_format,
                                        const VkImageLayout color_final_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        const VkImageLayout depth_final_layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    RenderPass *    CreateRenderPass(   const VkFormat color_format,
                                        const VkFormat depth_format,
                                        const VkImageLayout color_final_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        const VkImageLayout depth_final_layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    Fence *             CreateFence(bool);
    vulkan::GPUSemaphore * CreateSemaphore();

public: //FrameBuffer相关

    Framebuffer *CreateFramebuffer(RenderPass *rp,ImageView **color_list,const uint color_count,ImageView *depth);
//    Framebuffer *CreateFramebuffer(RenderPass *,List<ImageView *> &color,ImageView *depth);
    Framebuffer *CreateFramebuffer(RenderPass *,ImageView *color,ImageView *depth);
    Framebuffer *CreateFramebuffer(RenderPass *,ImageView *);

public:

    bool SubmitTexture      (const VkCommandBuffer *cmd_bufs,const uint32_t count=1);           ///<提交纹理处理到队列
    
    RenderTarget *CreateRenderTarget(   Framebuffer *,const uint32_t fence_count=1);

    RenderTarget *CreateRenderTarget(   const uint,const uint,
                                        const List<VkFormat> &color_format_list,
                                        const VkFormat depth_format,
                                        const VkImageLayout color_final_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        const VkImageLayout depth_final_layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                        const uint32_t fence_count=1);

    RenderTarget *CreateRenderTarget(   const uint,const uint,
                                        const VkFormat color_format,
                                        const VkFormat depth_format,
                                        const VkImageLayout color_final_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        const VkImageLayout depth_final_layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                        const uint32_t fence_count=1);

    RenderTarget *CreateRenderTarget(   const uint,const uint,const VkFormat,const VkImageLayout final_layout,const uint32_t fence_count=1);

    RenderTarget *CreateColorRenderTarget(   const uint w,const uint h,const VkFormat format,const uint32_t fence_count=1)
    {
        return CreateRenderTarget(w,h,format,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,fence_count);
    }

    RenderTarget *CreateDepthRenderTarget(   const uint w,const uint h,const VkFormat format,const uint32_t fence_count=1)
    {
        return CreateRenderTarget(w,h,format,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,fence_count);
    }
    
public:

    Pipeline *CreatePipeline(const InlinePipeline &,const Material *,const RenderTarget *);
    Pipeline *CreatePipeline(      PipelineData *,  const Material *,const RenderTarget *);

public:

    TileData *CreateTileData(const VkFormat video_format,const uint width,const uint height,const uint count);          ///<创建一个Tile数据集
    
    TileFont *CreateTileFont(FontSource *fs,int limit_count=-1);                                                  ///<创建一个Tile字体
};//class Device

//void CreateSubpassDependency(VkSubpassDependency *);
void CreateSubpassDependency(List<VkSubpassDependency> &dependency,const uint32_t count);

void CreateAttachmentReference(VkAttachmentReference *ref_list,uint start,uint count,VkImageLayout layout);

inline void CreateColorAttachmentReference(VkAttachmentReference *ref_list, uint start,uint count)  {CreateAttachmentReference(ref_list,    start,count,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);}
inline void CreateDepthAttachmentReference(VkAttachmentReference *depth_ref,uint index)             {CreateAttachmentReference(depth_ref,   index,1    ,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);}
inline void CreateInputAttachmentReference(VkAttachmentReference *ref_list, uint start,uint count)  {CreateAttachmentReference(ref_list,    start,count,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);}

    
bool CreateColorAttachment( List<VkAttachmentReference> &ref_list,List<VkAttachmentDescription> &desc_list,const List<VkFormat> &color_format,const VkImageLayout color_final_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
bool CreateDepthAttachment( List<VkAttachmentReference> &ref_list,List<VkAttachmentDescription> &desc_list,const VkFormat &depth_format,const VkImageLayout depth_final_layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

bool CreateAttachmentDescription(      List<VkAttachmentDescription> &color_output_desc_list,
                            const List<VkFormat> &color_format,
                            const VkFormat depth_format,
                            const VkImageLayout color_final_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            const VkImageLayout depth_final_layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

Device *CreateRenderDevice(Instance *inst,Window *win,const PhysicalDevice *physical_device=nullptr);
VK_NAMESPACE_END
#endif//HGL_GRAPH_RENDER_SURFACE_INCLUDE
