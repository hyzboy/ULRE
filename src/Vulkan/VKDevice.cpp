#include<hgl/vk/VKDevice.h>
#include<hgl/vk/pipeline/VKRenderTargetFormat.h>
#include<hgl/vk/VKRenderbufferInfo.h>
#include<hgl/vk/VKSemaphore.h>
#include<hgl/vk/VKFence.h>
#include<hgl/vk/VKQueue.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/pipeline/VKComputePipeline.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineBuilder.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineBuildRequest.h>
#include<hgl/vk/pipeline/VKGraphicsRenderState.h>
#include<hgl/vk/VKObjectName.h>
#include<hgl/vk/IGPUBuffer.h>
#include<hgl/log/Log.h>
#include <algorithm>
#include <functional>
#include <thread>
#include <utility>
#include <cstdint>
#include <cstdlib>
#include <unordered_map>
#include <string>
#ifdef _WIN32
#include <Windows.h>
#endif

namespace hgl::graph{
namespace
{
    std::unordered_map<VkDevice, VulkanDevice *> g_device_map;

    bool IsRenderDocActive()
    {
#ifdef _WIN32
        if (GetModuleHandleA("renderdoc.dll") != nullptr)
            return true;
#endif

        auto has_env = [](const char *name) -> bool
        {
            const char *value = std::getenv(name);
            return value && *value;
        };

        return has_env("RENDERDOC_CAPFILE")
            || has_env("RENDERDOC_CAPTUREOPTS")
            || has_env("RENDERDOC_DEBUG_LOG_FILE")
            || has_env("RENDERDOC_LOGFILE");
    }

    AnsiString GenerateRenderFormatKey(const RenderbufferInfo *rbi)
    {
        AnsiString key;
        hgl::Sprintf(key,"RenderFormat_%d_%u_%u_%u",
                     rbi->GetDepthFormat(),
                     (uint)rbi->GetColorLayout(),
                     (uint)rbi->GetDepthLayout(),
                     rbi->GetColorCount());
        for(const VkFormat &fmt:rbi->GetColorFormatList())
        {
            key+="_";
            key+=AnsiString::numberOf((int)fmt);
        }
        return key;
    }

    void BuildDebugNames(const AnsiString &name, std::string &buffer_name, std::string &memory_name)
    {
        const std::string base = name.c_str();
        const std::string token = ":Buffer:";

        const size_t pos = base.find(token);
        if (pos != std::string::npos)
        {
            buffer_name = base;
            memory_name = base;
            memory_name.replace(pos, token.size(), ":Memory:");
            return;
        }

        const std::string buffer_suffix = ":Buffer";
        const std::string memory_suffix = ":Memory";
        if (base.size() >= buffer_suffix.size() &&
            base.compare(base.size() - buffer_suffix.size(), buffer_suffix.size(), buffer_suffix) == 0)
        {
            buffer_name = base;
            memory_name = base.substr(0, base.size() - buffer_suffix.size()) + ":Memory";
            return;
        }

        if (base.size() >= memory_suffix.size() &&
            base.compare(base.size() - memory_suffix.size(), memory_suffix.size(), memory_suffix) == 0)
        {
            memory_name = base;
            buffer_name = base.substr(0, base.size() - memory_suffix.size()) + ":Buffer";
            return;
        }

        buffer_name = base + ":Buffer";
        memory_name = base + ":Memory";
    }
}

VulkanDevice::VulkanDevice(VulkanDevAttr *da)
{
    attr=da;

    gpl_supported = (attr && attr->physical_device) ? attr->physical_device->SupportGraphicsPipelineLibrary() : false;
    gpl_enabled = gpl_supported;

    if (gpl_enabled && IsRenderDocActive())
    {
        gpl_enabled = false;
        LogWarning("[VulkanDevice] RenderDoc detected; forcing monolithic graphics pipelines (GPL disabled) for compatibility.");
    }

    link_backend_mono = std::make_unique<MonolithicGraphicsPipelineBuilder>();
    link_backend_gpl = std::make_unique<GplGraphicsPipelineBuilder>();

    LogInfo("[VulkanDevice] Graphics pipeline library support=%s runtime_enabled=%s",
            gpl_supported?"yes":"no",
            gpl_enabled?"yes":"no");

    if(attr && attr->device)
        g_device_map[attr->device] = this;
}

VulkanDevice::~VulkanDevice()
{
    if(attr && attr->device)
        g_device_map.erase(attr->device);

    // 在设备销毁前，输出所有未销毁的对象以检测泄漏
    LogInfo("\n=== VulkanDevice Destructor Called ===");

    DumpTrackedObjects();

    LogInfo("=== End VulkanDevice Destructor ===");

    {
        std::lock_guard<std::mutex> lock(linked_pipeline_cache_mutex);
        for (auto &kv : linked_pipeline_cache)
            delete kv.second;
        linked_pipeline_cache.clear();
    }

    for(auto &kv:render_format_cache)
        delete kv.second;
    render_format_cache.Clear();

    // Release GPL/monolithic link backends before vkDestroyDevice in VulkanDevAttr.
    link_backend_gpl.reset();
    link_backend_mono.reset();

    if(vma_allocator)
    {
        bool has_live_vma_related_objects = false;
        for(const auto &entry : tracked_objects)
        {
            const VkObjectType t = entry.first.type;
            if(t == VK_OBJECT_TYPE_BUFFER ||
               t == VK_OBJECT_TYPE_IMAGE ||
               t == VK_OBJECT_TYPE_DEVICE_MEMORY)
            {
                has_live_vma_related_objects = true;
                break;
            }
        }

        if(has_live_vma_related_objects)
        {
            LogWarning("[VulkanDevice] Skip vmaDestroyAllocator due to live Vulkan objects. "
                       "Resource owners outlived device shutdown order.");
        }
        else
        {
            vmaDestroyAllocator(vma_allocator);
        }

        vma_allocator = VK_NULL_HANDLE;
    }

    delete attr;
}

VulkanDevice *VulkanDevice::FromDevice(VkDevice device)
{
    auto it = g_device_map.find(device);
    return it == g_device_map.end() ? nullptr : it->second;
}

void VulkanDevice::SetGplEnabled(bool enabled)
{
    if(!gpl_supported && enabled)
    {
        LogWarning("[VulkanDevice] Graphics pipeline library requested but not supported, keeping disabled");
    }

    gpl_enabled = gpl_supported && enabled;

    LogInfo("[VulkanDevice] Graphics pipeline library runtime_enabled=%s", gpl_enabled?"yes":"no");
}

GraphicsPipeline *VulkanDevice::AcquireGraphicsPipeline(const GraphicsPipelineBuildRequest &req)
{
    auto should_log_counter = [](const uint64_t v) -> bool
    {
        // Log at 1,2,4,8... to avoid log spam while still exposing trend.
        return v != 0 && ((v & (v - 1)) == 0);
    };

    if (!IsValidGraphicsPipelineBuildRequest(req))
    {
        LogWarning("[VulkanDevice] AcquireGraphicsPipeline invalid request: material=%p vil=%p render_format=%p pipeline_data=%p",
                   static_cast<const void *>(req.material),
                   static_cast<const void *>(req.vil),
                   static_cast<const void *>(req.render_format),
                   static_cast<const void *>(req.pipeline_data));
        return nullptr;
    }

    const GraphicsRenderState state_profile =
        GraphicsRenderState::FromGraphicsPipelineData(*req.pipeline_data, req.primitive, req.primitive_restart);

    const GplLinkedPipelineKey linked_key = BuildLinkedPipelineKey(req, state_profile);
    pipeline_library_cache.Touch(linked_key);

    auto log_pipeline_signature = [&](const char *tag, GraphicsPipeline *pipeline)
    {
        if (!pipeline)
            return;

        const VkFormat req_depth = req.render_format ? req.render_format->GetDepthFormat() : VK_FORMAT_UNDEFINED;
        const uint32_t req_color_count = req.render_format ? req.render_format->GetColorCount() : 0;

        LogInfo("[VulkanDevice] %s handle=0x%llx name=%s gpl=%s depthFormat=%d(req=%d) colorCount=%u(req=%u)",
                tag ? tag : "PipelineSignature",
                (unsigned long long)(uintptr_t)(VkPipeline)(*pipeline),
                pipeline->GetName().c_str(),
                pipeline->IsDebugCreatedWithGpl() ? "yes" : "no",
                (int)pipeline->GetDebugDepthAttachmentFormat(),
                (int)req_depth,
                pipeline->GetDebugColorAttachmentCount(),
                req_color_count);

        if (pipeline->GetDebugDepthAttachmentFormat() != req_depth
         || pipeline->GetDebugColorAttachmentCount() != req_color_count)
        {
            LogWarning("[VulkanDevice] Pipeline signature mismatch on cache path: handle=0x%llx depthFormat=%d(req=%d) colorCount=%u(req=%u)",
                       (unsigned long long)(uintptr_t)(VkPipeline)(*pipeline),
                       (int)pipeline->GetDebugDepthAttachmentFormat(),
                       (int)req_depth,
                       pipeline->GetDebugColorAttachmentCount(),
                       req_color_count);
        }
    };

    {
        const GplLibraryStatsTracker::Snapshot s = pipeline_library_cache.GetSnapshot();
        const uint64_t total_inserts = s.vertex_input_inserts + s.pre_raster_inserts + s.fragment_shader_inserts + s.fragment_output_inserts;
        if (should_log_counter(total_inserts))
        {
            LogDebug("[VulkanDevice] VirtualLibraryCache VI=%zu PR=%zu FS=%zu FO=%zu inserts=%llu",
                     s.vertex_input_size,
                     s.pre_raster_size,
                     s.fragment_shader_size,
                     s.fragment_output_size,
                     static_cast<unsigned long long>(total_inserts));
        }
    }

    {
        std::lock_guard<std::mutex> lock(linked_pipeline_cache_mutex);
        auto it = linked_pipeline_cache.find(linked_key);
        if (it != linked_pipeline_cache.end() && it->second)
        {
            log_pipeline_signature("LinkedPipelineCache HIT", it->second);

            const uint64_t hits = ++linked_pipeline_cache_hits;
            if (should_log_counter(hits))
            {
                LogDebug("[VulkanDevice] LinkedPipelineCache HIT count=%llu misses=%llu inserts=%llu size=%zu",
                         static_cast<unsigned long long>(hits),
                         static_cast<unsigned long long>(linked_pipeline_cache_misses.load()),
                         static_cast<unsigned long long>(linked_pipeline_cache_inserts.load()),
                         linked_pipeline_cache.size());
            }
            return it->second;
        }
    }

    const uint64_t misses = ++linked_pipeline_cache_misses;
    if (should_log_counter(misses))
    {
        std::lock_guard<std::mutex> lock(linked_pipeline_cache_mutex);
        LogDebug("[VulkanDevice] LinkedPipelineCache MISS count=%llu hits=%llu inserts=%llu size=%zu",
                 static_cast<unsigned long long>(misses),
                 static_cast<unsigned long long>(linked_pipeline_cache_hits.load()),
                 static_cast<unsigned long long>(linked_pipeline_cache_inserts.load()),
                 linked_pipeline_cache.size());
    }

    IGraphicsPipelineBuilder *backend = nullptr;

    if (gpl_enabled && link_backend_gpl)
        backend = link_backend_gpl.get();
    else
        backend = link_backend_mono.get();

    if (!backend)
    {
        LogError("[VulkanDevice] AcquireGraphicsPipeline has no available link backend");
        return nullptr;
    }

    GraphicsPipelineBuildContext ctx;
    ctx.device = this;
    ctx.pipeline_cache = GetPipelineCache();

    GraphicsPipeline *result = backend->Build(ctx, req);

    if (!result)
    {
        static bool warned_mono_failed = false;
        static bool warned_gpl_failed = false;

        bool *warned = (backend->GetType() == GraphicsPipelineBuilderType::Gpl)
                     ? &warned_gpl_failed
                     : &warned_mono_failed;

        if (!(*warned))
        {
            LogWarning("[VulkanDevice] AcquireGraphicsPipeline backend build failed once: backend=%s",
                       backend->GetType() == GraphicsPipelineBuilderType::Gpl ? "gpl" : "mono");
            *warned = true;
        }

        if (backend->GetType() == GraphicsPipelineBuilderType::Gpl && link_backend_mono)
        {
            static bool warned_fallback_once = false;
            if (!warned_fallback_once)
            {
                LogWarning("[VulkanDevice] AcquireGraphicsPipeline fallback: GPL backend failed, retry with monolithic backend");
                warned_fallback_once = true;
            }

            result = link_backend_mono->Build(ctx, req);
        }
    }

    if (!result)
        return nullptr;

    {
        std::lock_guard<std::mutex> lock(linked_pipeline_cache_mutex);
        linked_pipeline_cache[linked_key] = result;
        log_pipeline_signature("LinkedPipelineCache INSERT", result);

        const uint64_t inserts = ++linked_pipeline_cache_inserts;
        if (should_log_counter(inserts))
        {
            LogDebug("[VulkanDevice] LinkedPipelineCache INSERT count=%llu hits=%llu misses=%llu size=%zu",
                     static_cast<unsigned long long>(inserts),
                     static_cast<unsigned long long>(linked_pipeline_cache_hits.load()),
                     static_cast<unsigned long long>(linked_pipeline_cache_misses.load()),
                     linked_pipeline_cache.size());
        }
    }

    return result;
}

VulkanDevice::LinkedPipelineCacheStats VulkanDevice::GetLinkedPipelineCacheStats() const
{
    LinkedPipelineCacheStats stats;
    stats.hits    = linked_pipeline_cache_hits.load(std::memory_order_relaxed);
    stats.misses  = linked_pipeline_cache_misses.load(std::memory_order_relaxed);
    stats.inserts = linked_pipeline_cache_inserts.load(std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(linked_pipeline_cache_mutex);
        stats.size = linked_pipeline_cache.size();
    }
    return stats;
}

void VulkanDevice::PreheatPipelines(const GraphicsPipelineBuildRequest *requests, size_t count)
{
    if (!requests || count == 0)
        return;

    const uint64_t vk_create_before = RenderTargetFormat::GetVkCreateCount();
    size_t success = 0;
    size_t failure = 0;

    for (size_t i = 0; i < count; ++i)
    {
        GraphicsPipeline *p = AcquireGraphicsPipeline(requests[i]);
        if (p)
            ++success;
        else
        {
            ++failure;
            LogWarning("[VulkanDevice::PreheatPipelines] Request[%zu] failed: material=%p vil=%p render_format=%p pipeline_data=%p",
                       i,
                       static_cast<const void *>(requests[i].material),
                       static_cast<const void *>(requests[i].vil),
                       static_cast<const void *>(requests[i].render_format),
                       static_cast<const void *>(requests[i].pipeline_data));
        }
    }

    const uint64_t new_creates = RenderTargetFormat::GetVkCreateCount() - vk_create_before;
    const LinkedPipelineCacheStats stats = GetLinkedPipelineCacheStats();

    LogInfo("[VulkanDevice::PreheatPipelines] Preheated %zu requests: success=%zu failure=%zu "
            "new_vkCreate=%llu cache_hits=%llu cache_size=%zu",
            count,
            success,
            failure,
            static_cast<unsigned long long>(new_creates),
            static_cast<unsigned long long>(stats.hits),
            stats.size);
}

void VulkanDevice::DumpPipelineCacheStats()
{
    const LinkedPipelineCacheStats lc = GetLinkedPipelineCacheStats();
    const GplLibraryStatsTracker::Snapshot lib = GetPipelineLibraryCacheSnapshot();

    LogInfo("[VulkanDevice::DumpPipelineCacheStats] "
            "LinkedCache{ hits=%llu misses=%llu inserts=%llu size=%zu } "
            "LibraryCache{ VI(size=%zu inserts=%llu) PR(size=%zu inserts=%llu) "
            "FS(size=%zu inserts=%llu) FO(size=%zu inserts=%llu) }",
            static_cast<unsigned long long>(lc.hits),
            static_cast<unsigned long long>(lc.misses),
            static_cast<unsigned long long>(lc.inserts),
            lc.size,
            lib.vertex_input_size,
            static_cast<unsigned long long>(lib.vertex_input_inserts),
            lib.pre_raster_size,
            static_cast<unsigned long long>(lib.pre_raster_inserts),
            lib.fragment_shader_size,
            static_cast<unsigned long long>(lib.fragment_shader_inserts),
            lib.fragment_output_size,
            static_cast<unsigned long long>(lib.fragment_output_inserts));
}

void VulkanDevice::TickStatsDump()
{
    if (stats_dump_period_ == 0)
        return;
    const uint64_t idx = ++stats_dump_frame_counter_;
    if ((idx % stats_dump_period_) == 0)
        DumpPipelineCacheStats();
}

void VulkanDevice::RegisterGPUBuffer(IGPUBuffer *buf)
{
    if (!buf)
        return;
    gpu_buffer_registry.push_back(buf);
}

void VulkanDevice::UnregisterGPUBuffer(IGPUBuffer *buf)
{
    if (!buf)
        return;
    auto it = std::find(gpu_buffer_registry.begin(), gpu_buffer_registry.end(), buf);
    if (it != gpu_buffer_registry.end())
        gpu_buffer_registry.erase(it);
}

void VulkanDevice::TrackObject(VkObjectType type, uint64_t handle, const ObjectNameBuilder &name, const std::source_location &loc)
{
    if (!handle)
        return;

    ObjectDebugRecord record;
    record.name = name.ToString();  // 只在需要时转换为字符串
    record.file = loc.file_name();
    record.function = loc.function_name();
    record.line = loc.line();
    record.stack_depth = hgl::utils::get_current_allocation_stack(record.stack, 64);

//#ifdef _DEBUG
//    // 详细日志：记录对象创建
//    LogDebug("[CREATE] Object tracked");
//#endif//_DEBUG

    ObjectKey key{type, handle};
    auto it = tracked_objects.find(key);
    if (it != tracked_objects.end())
    {
        if (!record.name.IsEmpty())
            it->second.name = record.name;

        if (it->second.file.empty())
            it->second = std::move(record);
    }
    else
    {
        tracked_objects.emplace(key, std::move(record));
    }

#ifdef _DEBUG
    if (attr && attr->debug_utils,type!=VK_OBJECT_TYPE_UNKNOWN)
    {
        attr->debug_utils->SetName(type, handle, name.ToString().c_str());
    }
#endif//_DEBUG
}

void VulkanDevice::TrackObjectWithoutLocation(VkObjectType type, uint64_t handle, const ObjectNameBuilder &name)
{
    if (!handle)
        return;

    ObjectKey key{type, handle};
    auto it = tracked_objects.find(key);
    if (it != tracked_objects.end())
    {
        if (it->second.name.IsEmpty())
            it->second.name = name.ToString();  // 只在需要时转换为字符串
        return;
    }

    ObjectDebugRecord record;
    record.name = name.ToString();
    record.file = "";
    record.function = "";
    record.line = 0;
    record.stack_depth = hgl::utils::get_current_allocation_stack(record.stack, 64);

    tracked_objects.emplace(key, std::move(record));
}

void VulkanDevice::UntrackObject(VkObjectType type, uint64_t handle)
{
    if (!handle)
        return;

    ObjectKey key{type, handle};
    auto it = tracked_objects.find(key);

#ifdef _DEBUG
    if (it == tracked_objects.end())
    {
        LogWarning("[DESTROY-WARNING] Object not tracked - possible double destroy");
    }
#endif//_DEBUG

    tracked_objects.erase(key);
}

void VulkanDevice::DumpTrackedObjects() const
{
    if (tracked_objects.empty())
    {
        GLogInfo("No live objects at device destruction");
        return;
    }

    GLogWarning("%lu objects still alive at device destruction", (unsigned long)tracked_objects.size());

    // 按类型分类统计
    std::map<VkObjectType, int> type_counts;
    for (const auto &entry : tracked_objects)
    {
        type_counts[entry.first.type]++;
    }

    // 输出各类型统计
    for (const auto &tc : type_counts)
    {
        const char *type_name = "UNKNOWN";
        if (tc.first == VK_OBJECT_TYPE_BUFFER) type_name = "VkBuffer";
        else if (tc.first == VK_OBJECT_TYPE_IMAGE) type_name = "VkImage";
        else if (tc.first == VK_OBJECT_TYPE_DEVICE_MEMORY) type_name = "VkDeviceMemory";
        else if (tc.first == VK_OBJECT_TYPE_IMAGE_VIEW) type_name = "VkImageView";
        else if (tc.first == VK_OBJECT_TYPE_SAMPLER) type_name = "VkSampler";
        else if (tc.first == VK_OBJECT_TYPE_FRAMEBUFFER) type_name = "VkFramebuffer";
        else if (tc.first == VK_OBJECT_TYPE_RENDER_PASS) type_name = "VkRenderPass";
        else if (tc.first == VK_OBJECT_TYPE_PIPELINE) type_name = "VkPipeline";
        else if (tc.first == VK_OBJECT_TYPE_PIPELINE_LAYOUT) type_name = "VkPipelineLayout";
        else if (tc.first == VK_OBJECT_TYPE_DESCRIPTOR_SET) type_name = "VkDescriptorSet";
        else if (tc.first == VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT) type_name = "VkDescriptorSetLayout";

        GLogWarning("  %s: %d", type_name, tc.second);
    }

    // 输出详细泄漏信息
    for (const auto &entry : tracked_objects)
    {
        const ObjectKey &key = entry.first;
        const ObjectDebugRecord &record = entry.second;

        GLogWarning("[LEAK] Type: 0x%x Handle: 0x%llx Name: %s",
                    (unsigned int)key.type,
                    (unsigned long long)key.handle,
                    record.name.c_str());
    }

}

void VulkanDevice::TrackBuffer(VkBufferOwner *buf, const ObjectNameBuilder &name, const std::source_location &loc)
{
    if (!buf)
        return;

    TrackObject(VK_OBJECT_TYPE_BUFFER, (uint64_t)(uintptr_t)buf->GetBuffer(), name.Append(ObjectTypeTag::VKBuffer), loc);

    if (buf->GetVkMemory())
        TrackObject(VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)(uintptr_t)buf->GetVkMemory(), name.Append(ObjectTypeTag::VKMemory), loc);
}

void VulkanDevice::TrackTexture(Texture *tex, const ObjectNameBuilder &name, const std::source_location &loc)
{
    if (!tex)
        return;

    TrackObject(VK_OBJECT_TYPE_IMAGE, (uint64_t)(uintptr_t)tex->GetImage(), name.Append(ObjectTypeTag::VKImage), loc);
    TrackObject(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)(uintptr_t)tex->GetVulkanImageView(), name.Append(ObjectTypeTag::VKImageView), loc);
    TrackObject(VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)(uintptr_t)tex->GetDeviceMemory(), name.Append(ObjectTypeTag::VKMemory), loc);
}

void VulkanDevice::WaitIdle() const
{
    if (!attr || attr->device == VK_NULL_HANDLE)
    {
        GLogError("VulkanDevice::WaitIdle invalid device (attr=%p device=%p)",
                  (void *)attr,
                  attr ? (void *)attr->device : nullptr);
        return;
    }

    const VkResult res = vkDeviceWaitIdle(attr->device);
    if (res != VK_SUCCESS)
    {
        const auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
        GLogError("VulkanDevice::WaitIdle failed res=%d device=%p thread=%llu",
                  static_cast<int>(res),
                  (void *)attr->device,
                  static_cast<unsigned long long>(tid));
    }
}

VkCommandBuffer VulkanDevice::CreateCommandBuffer(const AnsiString &name)
{
    if(!attr->cmd_pool)
        return(VK_NULL_HANDLE);

    CommandBufferAllocateInfo cmd;

    cmd.commandPool         =attr->cmd_pool;
    cmd.level               =VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd.commandBufferCount  =1;

    VkCommandBuffer cmd_buf;

    VkResult res=vkAllocateCommandBuffers(attr->device,&cmd,&cmd_buf);

    if(res!=VK_SUCCESS)
        return(VK_NULL_HANDLE);

#ifdef _DEBUG
    if(attr->debug_utils)
        attr->debug_utils->SetCommandBuffer(cmd_buf,name);
#endif//_DEBUG

    return cmd_buf;
}

RenderCmdBuffer *VulkanDevice::CreateRenderCommandBuffer(const ObjectNameBuilder &name, const std::source_location &loc)
{
    VkCommandBuffer cb=CreateCommandBuffer(name.ToString());

    if(cb==VK_NULL_HANDLE)return(nullptr);

    RenderCmdBuffer *result = new RenderCmdBuffer(attr,cb);
    if (result)
        TrackObject(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)(uintptr_t)cb, name.Append(ObjectTypeTag::VKRenderCommandBuffer), loc);
    return result;
}

TextureCmdBuffer *VulkanDevice::CreateTextureCommandBuffer(const ObjectNameBuilder &name, const std::source_location &loc)
{
    VkCommandBuffer cb=CreateCommandBuffer(name.ToString());

    if(cb==VK_NULL_HANDLE)return(nullptr);

    TextureCmdBuffer *result = new TextureCmdBuffer(attr,cb);
    if (result)
        TrackObject(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)(uintptr_t)cb, name.Append(ObjectTypeTag::VKTextureCommandBuffer), loc);
    return result;
}

/**
 * 创建栅栏
 * @param create_signaled 是否创建初始信号
 */
Fence *VulkanDevice::CreateFence(const ObjectNameBuilder &name, bool create_signaled, const std::source_location &loc)
{
    FenceCreateInfo fenceInfo(create_signaled?VK_FENCE_CREATE_SIGNALED_BIT:0);

    VkFence fence;

    if(vkCreateFence(attr->device, &fenceInfo, nullptr, &fence)!=VK_SUCCESS)
        return(nullptr);

    TrackObject(VK_OBJECT_TYPE_FENCE, (uint64_t)(uintptr_t)fence, name.Append(ObjectTypeTag::VKFence), loc);

    // 创建Fence对象并传入追踪信息
    std::string fence_name = name.Append(ObjectTypeTag::VKFence).ToString().c_str();
    return(new Fence(attr->device, fence, fence_name, loc));
}

Fence *VulkanDevice::CreateFence(bool create_signaled, const std::source_location &loc)
{
    return CreateFence(ObjectNameBuilder("Fence"), create_signaled, loc);
}

Semaphore *VulkanDevice::CreateGPUSemaphore(const ObjectNameBuilder &name, const std::source_location &loc)
{
    SemaphoreCreateInfo SemaphoreCreateInfo;

    VkSemaphore sem;

    if(vkCreateSemaphore(attr->device, &SemaphoreCreateInfo, nullptr, &sem)!=VK_SUCCESS)
        return(nullptr);

    Semaphore *result = new Semaphore(attr->device, sem);
    if (result)
        TrackObject(VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)(uintptr_t)sem, name.Append(ObjectTypeTag::VKSemaphore), loc);
    return result;
}

DeviceQueue *VulkanDevice::CreateQueue(const ObjectNameBuilder &name, const uint32_t fence_count, const bool create_signaled, const std::source_location &loc)
{
    if(fence_count<=0)return(nullptr);

    Fence **fence_list=new Fence *[fence_count];

    const std::string base_name = name.ToString().c_str();
    for(uint32_t i=0;i<fence_count;i++)
    {
        std::string fence_name = base_name;
        fence_name += ":Fence[";
        fence_name += std::to_string(i);
        fence_name += "]";
        fence_list[i] = CreateFence(ObjectNameBuilder(fence_name.c_str()), create_signaled, loc);
    }

    DeviceQueue *result = new DeviceQueue(attr->device, attr->graphics_queue, fence_list, fence_count);
    // Note: We don't track VkQueue because it's retrieved via vkGetDeviceQueue and
    // is implicitly destroyed when VkDevice is destroyed. Multiple DeviceQueue C++ wrappers
    // may share the same VkQueue handle.
    return result;
}

ComputePipeline *VulkanDevice::CreateComputePipeline(const AnsiString &name, VkShaderModule shader_module, VkPipelineLayout pipeline_layout)
{
    VkComputePipelineCreateInfo compute_pipeline_info = {};
    compute_pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_pipeline_info.layout = pipeline_layout;
    compute_pipeline_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compute_pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    compute_pipeline_info.stage.module = shader_module;
    compute_pipeline_info.stage.pName = "main";

    VkPipeline pipeline;
    if (vkCreateComputePipelines(attr->device, attr->pipeline_cache, 1, &compute_pipeline_info, nullptr, &pipeline) != VK_SUCCESS)
    {
        LogError(name + " create compute pipeline failed!");
        return nullptr;
    }

#ifdef _DEBUG
    if(attr->debug_utils)
        attr->debug_utils->SetPipeline(pipeline, name);
#endif//_DEBUG

    return new ComputePipeline(name, attr->device, pipeline, pipeline_layout);
}

RenderTargetFormat *VulkanDevice::AcquireRenderTargetFormat(const RenderbufferInfo *rbi)
{
    HGL_CAPTURE_SCOPE();

    {
        const auto *phy_dev=GetPhyDevice();

        for(const VkFormat &fmt:rbi->GetColorFormatList())
            if(!phy_dev->IsColorAttachmentOptimal(fmt))
                return(nullptr);

        if(rbi->HasDepthOrStencil())
            if(!phy_dev->IsDepthAttachmentOptimal(rbi->GetDepthFormat()))
                return(nullptr);
    }

    AnsiString key=GenerateRenderFormatKey(rbi);
    RenderTargetFormat *rf=nullptr;

    if(render_format_cache.Get(key,rf))
        return rf;

    rf=new RenderTargetFormat(this,key,rbi->GetColorFormatList(),rbi->GetDepthFormat());
    render_format_cache.Add(key,rf);
    return rf;
}

}//namespace hgl::graph
