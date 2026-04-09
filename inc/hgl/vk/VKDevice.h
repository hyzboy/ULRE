#pragma once

#include<hgl/type/ValueArray.h>
#include<hgl/type/String.h>
#include<hgl/type/RectScope.h>
#include<hgl/graph/data/ImageRegion.h>
#include<hgl/platform/Window.h>
#include<hgl/graph/data/BitmapData.h>
#include<hgl/graph/font/Font.h>
#include<hgl/vk/VK.h>
#include<hgl/vk/VKMemory.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/vk/VKDeviceAttribute.h>
#include<hgl/vk/VKSwapchain.h>
#include<hgl/vk/VKShaderModuleMap.h>
#include<hgl/common/DescriptorSetTypeDef.h>
#include<hgl/vk/VKObjectNameBuilder.h>
#include<hgl/log/Log.h>
#include<hgl/object/ObjectTracker.h>
#include<typeinfo>
#include<type_traits>
#include<utility>
#include<ankerl/unordered_dense.h>
#include<mutex>
#include<atomic>
#include<source_location>
#include<ostream>
#include<memory>
#include<string>
#include<vector>
#include<hgl/vk/pipeline/VKGplPipelineKey.h>
#include<hgl/vk/pipeline/VKGplLibraryStatsTracker.h>

namespace hgl::graph{
class RenderTargetFormat;
class RenderbufferInfo;
class TileData;
class TileFont;
class FontDataSource;
class IndirectDrawBuffer;
class IndirectDrawIndexedBuffer;
class IndirectDispatchBuffer;
class StagedBuffer;
class IGPUBuffer;
class ComputePipeline;
class MaterialTemplate;
class Texture;
class Fence;
class DeviceQueue;
class Semaphore;
class MaterialParameters;
struct GraphicsPipelineBuildRequest;
class IGraphicsPipelineBuilder;

struct CopyBufferToImageInfo;

class VulkanDevice
{
    OBJECT_LOGGER

    VulkanDevAttr *attr;
    bool draw_phase_active = false;
    std::vector<IGPUBuffer*> gpu_buffer_registry;  // All Layer2 buffers, iterated by ECS UploadSystem
    ankerl::unordered_dense::map<std::string, RenderTargetFormat *> render_format_cache;
    bool gpl_supported = false;
    bool gpl_enabled = false;
    std::unique_ptr<IGraphicsPipelineBuilder> link_backend_mono;
    std::unique_ptr<IGraphicsPipelineBuilder> link_backend_gpl;
    GplLibraryStatsTracker pipeline_library_cache;
    ankerl::unordered_dense::map<GplLinkedPipelineKey, GraphicsPipeline *> linked_pipeline_cache;
    mutable std::mutex linked_pipeline_cache_mutex;
    std::atomic<uint64_t> linked_pipeline_cache_hits{0};
    std::atomic<uint64_t> linked_pipeline_cache_misses{0};
    std::atomic<uint64_t> linked_pipeline_cache_inserts{0};

    uint32_t stats_dump_period_{0};         ///< 定期 dump 间隔帧数；0 = 禁用
    uint64_t stats_dump_frame_counter_{0};  ///< TickStatsDump 调用次数计数器

    struct ObjectDebugRecord
    {
        std::string name;
        std::string file;
        std::string function;
        uint32_t line = 0;
        uint32_t stack_depth = 0;
        hgl::utils::SourceLocation stack[64];
    };

    struct ObjectKey
    {
        VkObjectType type = VK_OBJECT_TYPE_UNKNOWN;
        uint64_t handle = 0;

        bool operator==(const ObjectKey &other) const
        {
            return type == other.type && handle == other.handle;
        }
    };

    struct ObjectKeyHash
    {
        size_t operator()(const ObjectKey &key) const
        {
            return std::hash<uint64_t>{}(key.handle) ^ (static_cast<size_t>(key.type) << 1);
        }
    };

    ankerl::unordered_dense::map<ObjectKey, ObjectDebugRecord, ObjectKeyHash> tracked_objects;

private:

    VkCommandBuffer CreateCommandBuffer(const std::string &);

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

                void                WaitIdle            ()const;

#ifdef _DEBUG
                DebugUtils *        GetDebugUtils       (){return attr->debug_utils;}
#endif//_DEBUG

                static VulkanDevice *FromDevice         (VkDevice device);

                void                TrackObject         (VkObjectType type, uint64_t handle, const ObjectNameBuilder &name, const std::source_location &loc = std::source_location::current());
                void                TrackObjectWithoutLocation(VkObjectType type, uint64_t handle, const ObjectNameBuilder &name);
                void                UntrackObject       (VkObjectType type, uint64_t handle);
                size_t              GetTrackedObjectCount()const{return tracked_objects.size();}
                void                DumpTrackedObjects  ()const;

                void                TrackBuffer         (VkBufferOwner *buf, const ObjectNameBuilder &name, const std::source_location &loc = std::source_location::current());
                void                TrackTexture        (Texture *tex, const ObjectNameBuilder &name, const std::source_location &loc = std::source_location::current());

public:

                bool                Resize              (const VkExtent2D &);
                bool                Resize              (const uint32_t &w,const uint32_t &h)
                {
                    VkExtent2D extent={w,h};

                    return Resize(extent);
                }

public: //内存相关

    DeviceMemory *  CreateMemory(const VkMemoryRequirements &,const uint32_t properties, const ObjectNameBuilder &name, const std::source_location &loc = std::source_location::current());
    DeviceMemory *  CreateMemory(VkImage,const uint32 flag=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, const ObjectNameBuilder &name = ObjectNameBuilder("ImageMemory"), const std::source_location &loc = std::source_location::current());
    DeviceMemory *  CreateMemory(const VkMemoryRequirements &req, MemoryUsage usage, const ObjectNameBuilder &name, const std::source_location &loc = std::source_location::current());

    void SetDrawPhaseActive(bool active) { draw_phase_active = active; }
    bool IsDrawPhaseActive() const { return draw_phase_active; }
    bool IsGplSupported() const { return gpl_supported; }
    /// Runtime switch for graphics pipeline library usage.
    /// If the device does not support GPL, this always keeps GPL disabled.
    void SetGplEnabled(bool enabled);
    bool IsGplEnabled() const { return gpl_enabled; }
    GraphicsPipeline *AcquireGraphicsPipeline(const GraphicsPipelineBuildRequest &req);
    GplLibraryStatsTracker::Snapshot GetPipelineLibraryCacheSnapshot() const { return pipeline_library_cache.GetSnapshot(); }

    struct LinkedPipelineCacheStats
    {
        uint64_t hits    = 0;   ///< 缓存命中次数（无 vkCreateGraphicsPipelines）
        uint64_t misses  = 0;   ///< 缓存未命中次数（触发实际创建）
        uint64_t inserts = 0;   ///< 写入缓存次数
        size_t   size    = 0;   ///< 当前缓存条目数
    };
    LinkedPipelineCacheStats GetLinkedPipelineCacheStats() const;

    /**
     * 场景级 pipeline 预热：在场景加载阶段（Init）批量驱动 AcquireGraphicsPipeline，
     * 将首帧 vkCreateGraphicsPipelines 挪至加载期。
     * 失败请求仅 LogWarning，不中断后续请求。
     *
     * @param requests  GraphicsPipelineBuildRequest 数组指针
     * @param count     请求数量
     */
    void PreheatPipelines(const GraphicsPipelineBuildRequest *requests, size_t count);

    /// 将 LinkedPipelineCacheStats + GplLibraryStatsTracker::Snapshot 合并输出一条 LogInfo 日志。
    /// 用于析构前或调试断点处一次性查看缓存统计全貌。
    void DumpPipelineCacheStats();

    /**
     * 设置定期 dump 间隔帧数。每调用 TickStatsDump() 一次算一帧。
     * period == 0 时禁用定期 dump（默认）。
     */
    void SetStatsDumpPeriod(uint32_t period) { stats_dump_period_ = period; }

    /**
     * 每帧调用一次；当内部帧计数达到 period 的整数倍时自动调用 DumpPipelineCacheStats()。
     * 在 WorkManager::Run() 循环尾部调用。
     */
    void TickStatsDump();

    // ---- IGPUBuffer Registry (used by RenderBufferUploadSystem) ----
    void RegisterGPUBuffer  (IGPUBuffer *buf);
    void UnregisterGPUBuffer(IGPUBuffer *buf);
    const std::vector<IGPUBuffer*> &GetGPUBufferRegistry() const { return gpu_buffer_registry; }

private: //Buffer相关

    bool            CreateBuffer(DeviceBufferData *buf,VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sharing_mode,const ObjectNameBuilder &name, const std::source_location &loc = std::source_location::current());
    bool            CreateBuffer(DeviceBufferData *buf,VkBufferUsageFlags buf_usage,                   VkDeviceSize size,const void *data,SharingMode sharing_mode,const ObjectNameBuilder &name, const std::source_location &loc = std::source_location::current()){return CreateBuffer(buf,buf_usage,size,size,data,sharing_mode,name,loc);}
    bool            CreateBuffer(DeviceBufferData *buf,VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sharing_mode,MemoryUsage mem_usage,const ObjectNameBuilder &name, const std::source_location &loc = std::source_location::current());

public: //Buffer相关

    DeviceBuffer *  CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,   SharingMode sm=SharingMode::Exclusive, const std::source_location &loc = std::source_location::current());
    DeviceBuffer *  CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,                    SharingMode sm=SharingMode::Exclusive, const std::source_location &loc = std::source_location::current()){return CreateBuffer(buf_usage,range,size,nullptr,sm,loc);}

    DeviceBuffer *  CreateBuffer(VkBufferUsageFlags buf_usage,                   VkDeviceSize size,const void *data,   SharingMode sm=SharingMode::Exclusive, const std::source_location &loc = std::source_location::current()){return CreateBuffer(buf_usage,size,size,data,sm,loc);}
    DeviceBuffer *  CreateBuffer(VkBufferUsageFlags buf_usage,                   VkDeviceSize size,                    SharingMode sm=SharingMode::Exclusive, const std::source_location &loc = std::source_location::current()){return CreateBuffer(buf_usage,size,size,nullptr,sm,loc);}

    DeviceBuffer *  CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,BufferAllocPolicy policy,SharingMode sm=SharingMode::Exclusive, const std::source_location &loc = std::source_location::current());
    DeviceBuffer *  CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,BufferAllocPolicy policy,SharingMode sm,BufferUpdateClass update_class, const std::source_location &loc = std::source_location::current());
    DeviceBuffer *  CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sm,BufferUpdateClass update_class, const std::source_location &loc = std::source_location::current());
    DeviceBuffer *  CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,BufferAllocPolicy policy,SharingMode sm=SharingMode::Exclusive, const std::source_location &loc = std::source_location::current()){return CreateBuffer(buf_usage,range,size,nullptr,policy,sm,loc);}

    DeviceBuffer *  CreateBuffer(VkBufferUsageFlags buf_usage,                   VkDeviceSize size,const void *data,BufferAllocPolicy policy,SharingMode sm=SharingMode::Exclusive, const std::source_location &loc = std::source_location::current()){return CreateBuffer(buf_usage,size,size,data,policy,sm,loc);}
    DeviceBuffer *  CreateBuffer(VkBufferUsageFlags buf_usage,                   VkDeviceSize size,BufferAllocPolicy policy,SharingMode sm=SharingMode::Exclusive, const std::source_location &loc = std::source_location::current()){return CreateBuffer(buf_usage,size,size,nullptr,policy,sm,loc);}

    DeviceBuffer *  CreateBuffer(const ObjectNameBuilder &name,
                                 VkBufferUsageFlags buf_usage,
                                 VkDeviceSize range,
                                 VkDeviceSize size,
                                 const void *data,
                                 BufferAllocPolicy policy,
                                 SharingMode sm = SharingMode::Exclusive,
                                 BufferUpdateClass update_class = BufferUpdateClass::Default,
                                 const std::source_location &loc = std::source_location::current());

    StagedBuffer *  CreateStagedBuffer(const ObjectNameBuilder &name, VkBufferUsageFlags usage, VkDeviceSize size, const void *data = nullptr, SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current());
    StagedBuffer *  CreateStagedBuffer(VkBufferUsageFlags usage, VkDeviceSize size, const void *data = nullptr, SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current());

    VAB *           CreateVAB   (VkFormat format, uint32_t count,const void *data,BufferAllocPolicy policy,SharingMode sm=SharingMode::Exclusive,BufferUpdateClass update_class=BufferUpdateClass::Default, const std::source_location &loc = std::source_location::current());
    VAB *           CreateVAB   (const ObjectNameBuilder &name, VkFormat format, uint32_t count, const void *data, BufferAllocPolicy policy, SharingMode sm=SharingMode::Exclusive, BufferUpdateClass update_class=BufferUpdateClass::Default, const std::source_location &loc = std::source_location::current());
    VAB *           CreateVAB   (VkFormat format, uint32_t count,const void *data,    SharingMode sm=SharingMode::Exclusive, const std::source_location &loc = std::source_location::current()){return CreateVAB(format,count,data,BufferAllocPolicy::Auto,sm,BufferUpdateClass::Default,loc);}
    VAB *           CreateVAB   (VkFormat format, uint32_t count,                     SharingMode sm=SharingMode::Exclusive, const std::source_location &loc = std::source_location::current()){return CreateVAB(format,count,nullptr,BufferAllocPolicy::Auto,sm,BufferUpdateClass::Default,loc);}

    const bool      IsSupport       (const IndexType &type)const; ///<检测是否支持某种索引类型
    const IndexType ChooseIndexType (const VkDeviceSize &vertex_count)const;                    ///<求一个合适的索引类型
    const bool      CheckIndexType  (const IndexType,const VkDeviceSize &vertex_count)const;    ///<检测一个索引类型是否合适

    IndexBuffer *   CreateIBO   (const ObjectNameBuilder &name, IndexType type, uint32_t count, const void *data, BufferAllocPolicy policy, SharingMode sm=SharingMode::Exclusive, BufferUpdateClass update_class=BufferUpdateClass::Default, const std::source_location &loc = std::source_location::current());
    IndexBuffer *   CreateIBO   (IndexType type,  uint32_t count,const void *  data,BufferAllocPolicy policy,SharingMode sm=SharingMode::Exclusive,BufferUpdateClass update_class=BufferUpdateClass::Default, const std::source_location &loc = std::source_location::current());
    IndexBuffer *   CreateIBO   (IndexType type,  uint32_t count,const void *  data,  SharingMode sm=SharingMode::Exclusive, BufferUpdateClass update_class=BufferUpdateClass::Default, const std::source_location &loc = std::source_location::current()){return CreateIBO(type,count,data,BufferAllocPolicy::Auto,sm,update_class,loc);}
    IndexBuffer *   CreateIBO8  (                 uint32_t count,const void *  data,  SharingMode sm=SharingMode::Exclusive, BufferUpdateClass update_class=BufferUpdateClass::Default, const std::source_location &loc = std::source_location::current()){return CreateIBO(IndexType::U8,  count,(void *)data,sm,update_class,loc);}
    IndexBuffer *   CreateIBO16 (                 uint32_t count,const uint16 *data,  SharingMode sm=SharingMode::Exclusive, BufferUpdateClass update_class=BufferUpdateClass::Default, const std::source_location &loc = std::source_location::current()){return CreateIBO(IndexType::U16, count,(void *)data,sm,update_class,loc);}
    IndexBuffer *   CreateIBO32 (                 uint32_t count,const uint32 *data,  SharingMode sm=SharingMode::Exclusive, BufferUpdateClass update_class=BufferUpdateClass::Default, const std::source_location &loc = std::source_location::current()){return CreateIBO(IndexType::U32, count,(void *)data,sm,update_class,loc);}

    IndexBuffer *   CreateIBO   (IndexType type,  uint32_t count,                     SharingMode sm=SharingMode::Exclusive, BufferUpdateClass update_class=BufferUpdateClass::Default, const std::source_location &loc = std::source_location::current()){return CreateIBO(type,           count,nullptr,sm,update_class,loc);}
    IndexBuffer *   CreateIBO8  (                 uint32_t count,                     SharingMode sm=SharingMode::Exclusive, BufferUpdateClass update_class=BufferUpdateClass::Default, const std::source_location &loc = std::source_location::current()){return CreateIBO(IndexType::U8,  count,nullptr,sm,update_class,loc);}
    IndexBuffer *   CreateIBO16 (                 uint32_t count,                     SharingMode sm=SharingMode::Exclusive, BufferUpdateClass update_class=BufferUpdateClass::Default, const std::source_location &loc = std::source_location::current()){return CreateIBO(IndexType::U16, count,nullptr,sm,update_class,loc);}
    IndexBuffer *   CreateIBO32 (                 uint32_t count,                     SharingMode sm=SharingMode::Exclusive, BufferUpdateClass update_class=BufferUpdateClass::Default, const std::source_location &loc = std::source_location::current()){return CreateIBO(IndexType::U32, count,nullptr,sm,update_class,loc);}

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

#define CREATE_BUFFER_OBJECT(LargeName,type)    DeviceBuffer *Create##LargeName(                   VkDeviceSize size,void *data,BufferAllocPolicy policy,SharingMode sm=SharingMode::Exclusive, const std::source_location &loc = std::source_location::current())  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,size ,size,data,      policy,sm,loc);} \
                                                DeviceBuffer *Create##LargeName(                   VkDeviceSize size,             SharingMode sm=SharingMode::Exclusive, const std::source_location &loc = std::source_location::current())  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,size ,size,nullptr,   BufferAllocPolicy::Auto,sm,loc);} \
                                                DeviceBuffer *Create##LargeName(                   VkDeviceSize size,void *data,  SharingMode sm=SharingMode::Exclusive, const std::source_location &loc = std::source_location::current())  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,size ,size,data,      BufferAllocPolicy::Auto,sm,loc);} \
                                                DeviceBuffer *Create##LargeName(VkDeviceSize range,VkDeviceSize size,void *data,BufferAllocPolicy policy,SharingMode sm=SharingMode::Exclusive, const std::source_location &loc = std::source_location::current())  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,range,size,data,      policy,sm,loc);} \
                                                DeviceBuffer *Create##LargeName(VkDeviceSize range,VkDeviceSize size,             SharingMode sm=SharingMode::Exclusive, const std::source_location &loc = std::source_location::current())  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,range,size,nullptr,   BufferAllocPolicy::Auto,sm,loc);} \
                                                DeviceBuffer *Create##LargeName(VkDeviceSize range,VkDeviceSize size,void *data,  SharingMode sm=SharingMode::Exclusive, const std::source_location &loc = std::source_location::current())  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,range,size,data,      BufferAllocPolicy::Auto,sm,loc);} \
\
    DeviceBuffer *Create##LargeName(                   VkDeviceSize size,void *data,BufferAllocPolicy policy,SharingMode sm,BufferUpdateClass update_class, const std::source_location &loc = std::source_location::current())  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,size ,size,data,      policy,sm,update_class,loc);} \
    DeviceBuffer *Create##LargeName(VkDeviceSize range,VkDeviceSize size,void *data,BufferAllocPolicy policy,SharingMode sm,BufferUpdateClass update_class, const std::source_location &loc = std::source_location::current())  {return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,range,size,data,      policy,sm,update_class,loc);} \
\
    template<typename T> T *Create##LargeName(const DescriptorSetType &set_type,const std::string &name, const std::source_location &loc = std::source_location::current())  \
    {   \
        const VkDeviceSize range_size = T::GetSize();    \
        const VkDeviceSize alloc_size = AlignStructuredBufferSize(range_size, VK_BUFFER_USAGE_##type##_BUFFER_BIT);    \
        DeviceBuffer *buf=Create##LargeName(range_size, alloc_size, nullptr, BufferAllocPolicy::Auto, SharingMode::Exclusive, BufferUpdateClass::Default, loc);    \
        return(buf?CreateBufferObjectWithAligned<T>(alloc_size, buf, set_type, name, true):nullptr);  \
    }   \
\
    template<typename T> T *Create##LargeName(const ObjectNameBuilder &name, const std::source_location &loc = std::source_location::current())  \
    {   \
        const VkDeviceSize range_size = T::GetSize();    \
        const VkDeviceSize alloc_size = AlignStructuredBufferSize(range_size, VK_BUFFER_USAGE_##type##_BUFFER_BIT);    \
        DeviceBuffer *buf=CreateBuffer(name, VK_BUFFER_USAGE_##type##_BUFFER_BIT, range_size, alloc_size, nullptr, BufferAllocPolicy::Auto, SharingMode::Exclusive, BufferUpdateClass::Default, loc);    \
        return(buf?CreateBufferObjectWithAligned<T>(alloc_size, buf, true):nullptr);  \
    }

    CREATE_BUFFER_OBJECT(UBO,UNIFORM)
    CREATE_BUFFER_OBJECT(SSBO,STORAGE)
    CREATE_BUFFER_OBJECT(INBO,INDIRECT)

#undef CREATE_BUFFER_OBJECT

    DeviceBuffer *CreateUBO(const std::string &name, VkDeviceSize size, void *data, BufferAllocPolicy policy, SharingMode sm, BufferUpdateClass update_class, const std::source_location &loc = std::source_location::current())
    {
        DeviceBuffer *buf = CreateUBO(size, data, policy, sm, update_class);
        TrackBuffer(buf, name, loc);
        return buf;
    }

    DeviceBuffer *CreateUBO(const std::string &name, VkDeviceSize size, void *data, SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current())
    {
        return CreateUBO(name, size, data, BufferAllocPolicy::Auto, sm, BufferUpdateClass::Default, loc);
    }

    DeviceBuffer *CreateUBO(const std::string &name, VkDeviceSize size, SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current())
    {
        return CreateUBO(name, size, nullptr, BufferAllocPolicy::Auto, sm, BufferUpdateClass::Default, loc);
    }

    DeviceBuffer *CreateSSBO(const std::string &name, VkDeviceSize size, void *data, BufferAllocPolicy policy, SharingMode sm, BufferUpdateClass update_class, const std::source_location &loc = std::source_location::current())
    {
        DeviceBuffer *buf = CreateSSBO(size, data, policy, sm, update_class);
        TrackBuffer(buf, name, loc);
        return buf;
    }

    DeviceBuffer *CreateSSBO(const std::string &name, VkDeviceSize size, void *data, SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current())
    {
        return CreateSSBO(name, size, data, BufferAllocPolicy::Auto, sm, BufferUpdateClass::Default, loc);
    }

    DeviceBuffer *CreateSSBO(const std::string &name, VkDeviceSize size, SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current())
    {
        return CreateSSBO(name, size, nullptr, BufferAllocPolicy::Auto, sm, BufferUpdateClass::Default, loc);
    }

    DeviceBuffer *CreateINBO(const std::string &name, VkDeviceSize size, void *data, BufferAllocPolicy policy, SharingMode sm, BufferUpdateClass update_class, const std::source_location &loc = std::source_location::current())
    {
        DeviceBuffer *buf = CreateINBO(size, data, policy, sm, update_class);
        TrackBuffer(buf, name, loc);
        return buf;
    }

    DeviceBuffer *CreateINBO(const std::string &name, VkDeviceSize size, void *data, SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current())
    {
        return CreateINBO(name, size, data, BufferAllocPolicy::Auto, sm, BufferUpdateClass::Default, loc);
    }

    DeviceBuffer *CreateINBO(const std::string &name, VkDeviceSize size, SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current())
    {
        return CreateINBO(name, size, nullptr, BufferAllocPolicy::Auto, sm, BufferUpdateClass::Default, loc);
    }

public: //间接绘制

    bool CreateIndirectCommandBuffer(DeviceBufferData *,const uint32_t cmd_count,const uint32_t cmd_size,const ObjectNameBuilder &name,SharingMode sm=SharingMode::Exclusive);
    bool CreateIndirectCommandBuffer(DeviceBufferData *,const uint32_t cmd_count,const uint32_t cmd_size,BufferAllocPolicy policy,IGPUBuffer **staged_out,const ObjectNameBuilder &name,SharingMode sm=SharingMode::Exclusive);

    // 间接绘制缓冲创建（必须传入名字用于追踪）
    IndirectDrawBuffer *        CreateIndirectDrawBuffer(       const uint32_t cmd_count,const ObjectNameBuilder &name,SharingMode sm=SharingMode::Exclusive);
    IndirectDrawBuffer *        CreateIndirectDrawBuffer(       const uint32_t cmd_count,BufferAllocPolicy policy,const ObjectNameBuilder &name,SharingMode sm=SharingMode::Exclusive);
    IndirectDrawIndexedBuffer * CreateIndirectDrawIndexedBuffer(const uint32_t cmd_count,const ObjectNameBuilder &name,SharingMode sm=SharingMode::Exclusive);
    IndirectDrawIndexedBuffer * CreateIndirectDrawIndexedBuffer(const uint32_t cmd_count,BufferAllocPolicy policy,const ObjectNameBuilder &name,SharingMode sm=SharingMode::Exclusive);
    IndirectDispatchBuffer *    CreateIndirectDispatchBuffer(   const uint32_t cmd_count,const ObjectNameBuilder &name,SharingMode sm=SharingMode::Exclusive);
    IndirectDispatchBuffer *    CreateIndirectDispatchBuffer(   const uint32_t cmd_count,BufferAllocPolicy policy,const ObjectNameBuilder &name,SharingMode sm=SharingMode::Exclusive);

public: //

    Sampler *CreateSampler(VkSamplerCreateInfo *sci=nullptr);
    Sampler *CreateSampler(Texture *);

public: //shader & material

    ShaderModule *CreateShaderModule(VkShaderStageFlagBits,const uint32_t *,const size_t);

    GraphicsPipelineLayoutData *CreateGraphicsPipelineLayoutData(const MaterialDescriptorManager *desc_manager);

    MaterialParameters *CreateMP(const MaterialDescriptorManager *desc_manager,const GraphicsPipelineLayoutData *pld,const DescriptorSetType &desc_set_type);

public: //Command Buffer 相关

    RenderCmdBuffer * CreateRenderCommandBuffer(const ObjectNameBuilder &name, const std::source_location &loc = std::source_location::current());
    TextureCmdBuffer *CreateTextureCommandBuffer(const ObjectNameBuilder &name, const std::source_location &loc = std::source_location::current());

public:

    Fence *      CreateFence(const ObjectNameBuilder &name, bool create_signaled = false, const std::source_location &loc = std::source_location::current());
    Fence *      CreateFence(bool create_signaled, const std::source_location &loc = std::source_location::current());
    Semaphore *  CreateGPUSemaphore(const ObjectNameBuilder &name, const std::source_location &loc = std::source_location::current());

    DeviceQueue *CreateQueue(const ObjectNameBuilder &name, const uint32_t fence_count=1, const bool create_signaled=false, const std::source_location &loc = std::source_location::current());

public: // Compute Pipeline相关

    /**
     * 创建计算管线
     * @param name 管线名称
     * @param shader_module 计算着色器模块
     * @param pipeline_layout 管线布局
     * @return 计算管线指针，失败返回nullptr
     */
    ComputePipeline *CreateComputePipeline(const std::string &name, VkShaderModule shader_module, VkPipelineLayout pipeline_layout);

public:

    TileData *CreateTileData(const VkFormat video_format,const uint width,const uint height,const uint count);          ///<创建一个Tile数据集

    TileFont *CreateTileFont(FontDataSource *fs,int limit_count=-1);                                                        ///<创建一个Tile字体

public: // RenderTargetFormat Cache

    RenderTargetFormat *AcquireRenderTargetFormat(const RenderbufferInfo *rbi);
};//class VulkanDevice
}//namespace hgl::graph
