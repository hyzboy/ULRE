#pragma once

#include<hgl/type/ValueArray.h>
#include<hgl/type/String.h>
#include<hgl/type/RectScope.h>
#include<hgl/graph/ImageRegion.h>
#include<hgl/platform/Window.h>
#include<hgl/graph/BitmapData.h>
#include<hgl/graph/font/Font.h>
#include<hgl/graph/VK.h>
#include<hgl/graph/VKMemory.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKDeviceAttribute.h>
#include<hgl/graph/VKSwapchain.h>
#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKShaderModuleMap.h>
#include<hgl/graph/VKArrayBuffer.h>
#include<hgl/graph/VKDescriptorSetType.h>
#include<hgl/log/Log.h>
#include<typeinfo>
#include<type_traits>
#include<utility>

VK_NAMESPACE_BEGIN
class TileData;
class TileFont;
class FontDataSource;
class VulkanArrayBuffer;
class IndirectDrawBuffer;
class IndirectDrawIndexedBuffer;
class IndirectDispatchBuffer;
class BufferUpdateQueue;
class BufferCommitQueue;
class StagedBuffer;
class ComputePipeline;
class Material;

struct CopyBufferToImageInfo;

class VulkanDevice
{
    OBJECT_LOGGER

    VulkanDevAttr *attr;
    BufferUpdateQueue *buffer_update_queue;
    BufferCommitQueue *buffer_commit_queue;

private:

    VkCommandBuffer CreateCommandBuffer(const AnsiString &);

private:

    friend class VulkanDeviceCreater;

    VulkanDevice(VulkanDevAttr *da);

public:

    virtual ~VulkanDevice();

    operator    VkDevice                                ()      {return attr->device;}
                VulkanDevAttr *     GetDevAttr          ()      {return attr;}

    const       VulkanSurface *     GetSurface          ()const {return attr->surface;}
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

    DeviceMemory *  CreateMemory(const VkMemoryRequirements &,const uint32_t properties);
    DeviceMemory *  CreateMemory(VkImage,const uint32 flag=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    DeviceMemory *  CreateMemory(const VkMemoryRequirements &req, MemoryUsage usage);

    BufferUpdateQueue * GetBufferUpdateQueue() { return buffer_update_queue; }
    BufferCommitQueue * GetBufferCommitQueue() { return buffer_commit_queue; }

private: //Buffer相关

    bool            CreateBuffer(DeviceBufferData *buf,VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sharing_mode);
    bool            CreateBuffer(DeviceBufferData *buf,VkBufferUsageFlags buf_usage,                   VkDeviceSize size,const void *data,SharingMode sharing_mode){return CreateBuffer(buf,buf_usage,size,size,data,sharing_mode);}
    bool            CreateBuffer(DeviceBufferData *buf,VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sharing_mode,MemoryUsage mem_usage);

public: //Buffer相关

    DeviceBuffer *  CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,   SharingMode sm=SharingMode::Exclusive);
    DeviceBuffer *  CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,                    SharingMode sm=SharingMode::Exclusive){return CreateBuffer(buf_usage,range,size,nullptr,sm);}

    DeviceBuffer *  CreateBuffer(VkBufferUsageFlags buf_usage,                   VkDeviceSize size,const void *data,   SharingMode sm=SharingMode::Exclusive){return CreateBuffer(buf_usage,size,size,data,sm);}
    DeviceBuffer *  CreateBuffer(VkBufferUsageFlags buf_usage,                   VkDeviceSize size,                    SharingMode sm=SharingMode::Exclusive){return CreateBuffer(buf_usage,size,size,nullptr,sm);}

    DeviceBuffer *  CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,BufferAllocPolicy policy,SharingMode sm=SharingMode::Exclusive);
    DeviceBuffer *  CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,BufferAllocPolicy policy,SharingMode sm,BufferUpdateClass update_class);
    DeviceBuffer *  CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sm,BufferUpdateClass update_class);
    DeviceBuffer *  CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,BufferAllocPolicy policy,SharingMode sm=SharingMode::Exclusive){return CreateBuffer(buf_usage,range,size,nullptr,policy,sm);}

    DeviceBuffer *  CreateBuffer(VkBufferUsageFlags buf_usage,                   VkDeviceSize size,const void *data,BufferAllocPolicy policy,SharingMode sm=SharingMode::Exclusive){return CreateBuffer(buf_usage,size,size,data,policy,sm);}
    DeviceBuffer *  CreateBuffer(VkBufferUsageFlags buf_usage,                   VkDeviceSize size,BufferAllocPolicy policy,SharingMode sm=SharingMode::Exclusive){return CreateBuffer(buf_usage,size,size,nullptr,policy,sm);}

    StagedBuffer *  CreateStagedBuffer(VkBufferUsageFlags usage, VkDeviceSize size, const void *data = nullptr, SharingMode sm = SharingMode::Exclusive);

    VAB *           CreateVAB   (VkFormat format, uint32_t count,const void *data,BufferAllocPolicy policy,SharingMode sm=SharingMode::Exclusive,BufferUpdateClass update_class=BufferUpdateClass::Default);
    VAB *           CreateVAB   (VkFormat format, uint32_t count,const void *data,    SharingMode sm=SharingMode::Exclusive){return CreateVAB(format,count,data,BufferAllocPolicy::Auto,sm);}
    VAB *           CreateVAB   (VkFormat format, uint32_t count,                     SharingMode sm=SharingMode::Exclusive){return CreateVAB(format,count,nullptr,BufferAllocPolicy::Auto,sm);}

    const bool      IsSupport       (const IndexType &type)const; ///<检测是否支持某种索引类型
    const IndexType ChooseIndexType (const VkDeviceSize &vertex_count)const;                    ///<求一个合适的索引类型
    const bool      CheckIndexType  (const IndexType,const VkDeviceSize &vertex_count)const;    ///<检测一个索引类型是否合适

    IndexBuffer *   CreateIBO   (IndexType type,  uint32_t count,const void *  data,BufferAllocPolicy policy,SharingMode sm=SharingMode::Exclusive,BufferUpdateClass update_class=BufferUpdateClass::Default);
    IndexBuffer *   CreateIBO   (IndexType type,  uint32_t count,const void *  data,  SharingMode sm=SharingMode::Exclusive){return CreateIBO(type,count,data,BufferAllocPolicy::Auto,sm);}
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

    VkDeviceSize AlignStructuredBufferSize(VkDeviceSize size,VkBufferUsageFlags usage) const
    {
        if(!(usage & (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)))
            return size;

        const VkDeviceSize atom_size = attr->physical_device->GetLimits().nonCoherentAtomSize;
        if(atom_size == 0)
            return size;

        return hgl_align(size, atom_size);
    }

    template<typename T>
    static void LogCreateBufferBegin(const char *name,const ShaderBufferDesc *desc,BufferUpdateClass update_class)
    {
#ifdef _DEBUG
        GLogWarning("[Create%s] type=%s size=%llu desc=%p update_class=%u",
                    name,
                    typeid(T).name(),
                    static_cast<unsigned long long>(T::GetSize()),
                    (void *)desc,
                    static_cast<unsigned>(update_class));
#endif//_DEBUG
    }

    template<typename T>
    static void LogCreateBufferEnd(const char *name,DeviceBuffer *buf)
    {
#ifdef _DEBUG
        GLogWarning("[Create%s] type=%s buffer=%p mem=%p memSize=%llu bufRange=%llu",
                    name,
                    typeid(T).name(),
                    (void *)buf,
                    buf ? (void *)static_cast<VkDeviceMemory>(buf->GetVkMemory()) : nullptr,
                    buf && buf->GetMemory() ? static_cast<unsigned long long>(buf->GetMemory()->GetSize()) : 0ULL,
                    buf ? static_cast<unsigned long long>(buf->GetSize()) : 0ULL);
#endif//_DEBUG
    }

    template<typename T,typename... Args>
    static std::enable_if_t<std::is_constructible<T, Args..., VkDeviceSize>::value, T *>
    CreateBufferObjectWithAligned(VkDeviceSize aligned_size, Args&&... args)
    {
        return new T(std::forward<Args>(args)..., aligned_size);
    }

    template<typename T,typename... Args>
    static std::enable_if_t<!std::is_constructible<T, Args..., VkDeviceSize>::value, T *>
    CreateBufferObjectWithAligned(VkDeviceSize, Args&&... args)
    {
        return new T(std::forward<Args>(args)...);
    }

#define CREATE_BUFFER_OBJECT(LargeName,type)    DeviceBuffer *Create##LargeName(                   VkDeviceSize size,void *data,BufferAllocPolicy policy,SharingMode sm=SharingMode::Exclusive)  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,size ,size,data,      policy,sm);} \
                                                DeviceBuffer *Create##LargeName(                   VkDeviceSize size,             SharingMode sm=SharingMode::Exclusive)  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,size ,size,nullptr,   BufferAllocPolicy::Auto,sm);} \
                                                DeviceBuffer *Create##LargeName(                   VkDeviceSize size,void *data,  SharingMode sm=SharingMode::Exclusive)  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,size ,size,data,      BufferAllocPolicy::Auto,sm);} \
                                                DeviceBuffer *Create##LargeName(VkDeviceSize range,VkDeviceSize size,void *data,BufferAllocPolicy policy,SharingMode sm=SharingMode::Exclusive)  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,range,size,data,      policy,sm);} \
                                                DeviceBuffer *Create##LargeName(VkDeviceSize range,VkDeviceSize size,             SharingMode sm=SharingMode::Exclusive)  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,range,size,nullptr,   BufferAllocPolicy::Auto,sm);} \
                                                DeviceBuffer *Create##LargeName(VkDeviceSize range,VkDeviceSize size,void *data,  SharingMode sm=SharingMode::Exclusive)  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,range,size,data,      BufferAllocPolicy::Auto,sm);} \
\
    DeviceBuffer *Create##LargeName(                   VkDeviceSize size,void *data,BufferAllocPolicy policy,SharingMode sm,BufferUpdateClass update_class)  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,size ,size,data,      policy,sm,update_class);} \
    DeviceBuffer *Create##LargeName(VkDeviceSize range,VkDeviceSize size,void *data,BufferAllocPolicy policy,SharingMode sm,BufferUpdateClass update_class)  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,range,size,data,      policy,sm,update_class);} \
\
    template<typename T> T *Create##LargeName()  \
    {   \
        const VkDeviceSize range_size = T::GetSize();    \
        const VkDeviceSize alloc_size = AlignStructuredBufferSize(range_size, VK_BUFFER_USAGE_##type##_BUFFER_BIT);    \
        DeviceBuffer *buf=Create##LargeName(range_size, alloc_size);    \
        return(buf?CreateBufferObjectWithAligned<T>(alloc_size, buf):nullptr);  \
    } \
\
    template<typename T> T *Create##LargeName(const ShaderBufferDesc *desc)  \
    {   \
        const VkDeviceSize range_size = T::GetSize();    \
        const VkDeviceSize alloc_size = AlignStructuredBufferSize(range_size, VK_BUFFER_USAGE_##type##_BUFFER_BIT);    \
        DeviceBuffer *buf=Create##LargeName(range_size, alloc_size);    \
        return(buf?CreateBufferObjectWithAligned<T>(alloc_size, buf, desc):nullptr);  \
    }   \
\
    template<typename T> T *Create##LargeName(const ShaderBufferDesc *desc,BufferUpdateClass update_class)  \
    {   \
        LogCreateBufferBegin<T>(#LargeName, desc, update_class);\
        const VkDeviceSize range_size = T::GetSize();    \
        const VkDeviceSize alloc_size = AlignStructuredBufferSize(range_size, VK_BUFFER_USAGE_##type##_BUFFER_BIT);    \
        DeviceBuffer *buf=Create##LargeName(range_size, alloc_size, nullptr, BufferAllocPolicy::Auto, SharingMode::Exclusive, update_class);    \
        LogCreateBufferEnd<T>(#LargeName, buf);\
        return(buf?CreateBufferObjectWithAligned<T>(alloc_size, buf, desc):nullptr);  \
    }   \
\
    template<typename T> T *Create##LargeName(const DescriptorSetType &set_type,const AnsiString &name)  \
    {   \
        const VkDeviceSize range_size = T::GetSize();    \
        const VkDeviceSize alloc_size = AlignStructuredBufferSize(range_size, VK_BUFFER_USAGE_##type##_BUFFER_BIT);    \
        DeviceBuffer *buf=Create##LargeName(range_size, alloc_size);    \
        return(buf?CreateBufferObjectWithAligned<T>(alloc_size, buf, set_type, name):nullptr);  \
    }

    CREATE_BUFFER_OBJECT(UBO,UNIFORM)
    CREATE_BUFFER_OBJECT(SSBO,STORAGE)
    CREATE_BUFFER_OBJECT(INBO,INDIRECT)

#undef CREATE_BUFFER_OBJECT

    VulkanArrayBuffer *CreateArrayInUBO(const VkDeviceSize &uint_size);
    VulkanArrayBuffer *CreateArrayInSSBO(const VkDeviceSize &uint_size);

public: //间接绘制

    bool CreateIndirectCommandBuffer(DeviceBufferData *,const uint32_t cmd_count,const uint32_t cmd_size,SharingMode sm=SharingMode::Exclusive);
    bool CreateIndirectCommandBuffer(DeviceBufferData *,const uint32_t cmd_count,const uint32_t cmd_size,BufferAllocPolicy policy,StagedBuffer **staged_out,SharingMode sm=SharingMode::Exclusive);

    IndirectDrawBuffer *        CreateIndirectDrawBuffer(       const uint32_t cmd_count,SharingMode sm=SharingMode::Exclusive);
    IndirectDrawBuffer *        CreateIndirectDrawBuffer(       const uint32_t cmd_count,BufferAllocPolicy policy,SharingMode sm=SharingMode::Exclusive);
    IndirectDrawIndexedBuffer * CreateIndirectDrawIndexedBuffer(const uint32_t cmd_count,SharingMode sm=SharingMode::Exclusive);
    IndirectDrawIndexedBuffer * CreateIndirectDrawIndexedBuffer(const uint32_t cmd_count,BufferAllocPolicy policy,SharingMode sm=SharingMode::Exclusive);
    IndirectDispatchBuffer *    CreateIndirectDispatchBuffer(   const uint32_t cmd_count,SharingMode sm=SharingMode::Exclusive);
    IndirectDispatchBuffer *    CreateIndirectDispatchBuffer(   const uint32_t cmd_count,BufferAllocPolicy policy,SharingMode sm=SharingMode::Exclusive);

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

public: // Compute Pipeline相关

    /**
     * 创建计算管线
     * @param name 管线名称
     * @param shader_module 计算着色器模块
     * @param pipeline_layout 管线布局
     * @return 计算管线指针，失败返回nullptr
     */
    ComputePipeline *CreateComputePipeline(const AnsiString &name, VkShaderModule shader_module, VkPipelineLayout pipeline_layout);

public:

    TileData *CreateTileData(const VkFormat video_format,const uint width,const uint height,const uint count);          ///<创建一个Tile数据集

    TileFont *CreateTileFont(FontDataSource *fs,int limit_count=-1);                                                        ///<创建一个Tile字体
};//class VulkanDevice
VK_NAMESPACE_END
