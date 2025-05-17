#pragma once

#include<hgl/type/ArrayList.h>
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
#include<hgl/graph/VKShaderModuleMap.h>
#include<hgl/graph/VKArrayBuffer.h>
#include<hgl/graph/VKDescriptorSetType.h>

VK_NAMESPACE_BEGIN
class TileData;
class TileFont;
class FontSource;
class VulkanArrayBuffer;
class IndirectDrawBuffer;
class IndirectDrawIndexedBuffer;
class IndirectDispatchBuffer;

struct CopyBufferToImageInfo;

class VulkanDevice
{
    VulkanDevAttr *attr;

private:

    VkCommandBuffer CreateCommandBuffer(const AnsiString &);

private:

    friend class VulkanDeviceCreater;

    VulkanDevice(VulkanDevAttr *da);

public:

    virtual ~VulkanDevice();

    operator    VkDevice                                ()      {return attr->device;}
                VulkanDevAttr *     GetDevAttr          ()      {return attr;}

                VkSurfaceKHR        GetSurface          ()      {return attr->surface;}
                VkDevice            GetDevice           ()const {return attr->device;}
    const       VulkanPhyDevice *   GetPhyDevice        ()const {return attr->physical_device;}

                VkDescriptorPool    GetDescriptorPool   ()      {return attr->desc_pool;}
                VkPipelineCache     GetPipelineCache    ()      {return attr->pipeline_cache;}

    const       VkFormat            GetSurfaceFormat    ()const {return attr->surface_format.format;}
    const       VkColorSpaceKHR     GetColorSpace       ()const {return attr->surface_format.colorSpace;}
                VkQueue             GetGraphicsQueue    ()      {return attr->graphics_queue;}

                void                WaitIdle            ()const {vkDeviceWaitIdle(attr->device);}
                
#ifdef _DEBUG
                DebugUtils *        GetDebugUtils       (){return attr->debug_utils;}
#endif//_DEBUG

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

    VAB *           CreateVAB   (VkFormat format, uint32_t count,const void *data,    SharingMode sm=SharingMode::Exclusive);
    VAB *           CreateVAB   (VkFormat format, uint32_t count,                     SharingMode sm=SharingMode::Exclusive){return CreateVAB(format,count,nullptr,sm);}

    const IndexType ChooseIndexType (const VkDeviceSize &vertex_count)const;                    ///<求一个合适的索引类型
    const bool      CheckIndexType  (const IndexType,const VkDeviceSize &vertex_count)const;    ///<检测一个索引类型是否合适

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
                                                DeviceBuffer *Create##LargeName(VkDeviceSize range,VkDeviceSize size,             SharingMode sm=SharingMode::Exclusive)  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,range,size,nullptr,   sm);} \
\
    template<typename T> T *Create##LargeName()  \
    {   \
        DeviceBuffer *buf=Create##LargeName(T::GetSize());    \
        return(buf?new T(buf):nullptr);  \
    }

    CREATE_BUFFER_OBJECT(UBO,UNIFORM)
    CREATE_BUFFER_OBJECT(SSBO,STORAGE)
    CREATE_BUFFER_OBJECT(INBO,INDIRECT)

#undef CREATE_BUFFER_OBJECT

    VulkanArrayBuffer *CreateArrayInUBO(const VkDeviceSize &uint_size);
    VulkanArrayBuffer *CreateArrayInSSBO(const VkDeviceSize &uint_size);

public: //间接绘制

    bool CreateIndirectCommandBuffer(DeviceBufferData *,const uint32_t cmd_count,const uint32_t cmd_size,SharingMode sm=SharingMode::Exclusive);

    IndirectDrawBuffer *        CreateIndirectDrawBuffer(       const uint32_t cmd_count,SharingMode sm=SharingMode::Exclusive);
    IndirectDrawIndexedBuffer * CreateIndirectDrawIndexedBuffer(const uint32_t cmd_count,SharingMode sm=SharingMode::Exclusive);
    IndirectDispatchBuffer *    CreateIndirectDispatchBuffer(   const uint32_t cmd_count,SharingMode sm=SharingMode::Exclusive);

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

    Fence *      CreateFence(bool);
    Semaphore *  CreateGPUSemaphore();

    DeviceQueue *CreateQueue(const uint32_t fence_count=1,const bool create_signaled=false);

public:

    TileData *CreateTileData(const VkFormat video_format,const uint width,const uint height,const uint count);          ///<创建一个Tile数据集
    
    TileFont *CreateTileFont(FontSource *fs,int limit_count=-1);                                                        ///<创建一个Tile字体
};//class VulkanDevice
VK_NAMESPACE_END
