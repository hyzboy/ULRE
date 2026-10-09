#include<hgl/vk/pipeline/VKPipelineResolver.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineBackend.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/thread/ThreadMutex.h>
#include<hgl/log/Log.h>

namespace hgl::graph
{
    namespace
    {
        struct FinalPipelineCacheEntry
        {
            VulkanDevice *device = nullptr;
            VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
            FinalPipelineKey key{};
            VkPipeline pipeline = VK_NULL_HANDLE;
            uint32_t references = 0;

            bool operator == (const FinalPipelineCacheEntry &rhs) const
            {
                return device == rhs.device
                    && pipeline_layout == rhs.pipeline_layout
                    && key == rhs.key
                    && pipeline == rhs.pipeline
                    && references == rhs.references;
            }
        };

        ValueArray<FinalPipelineCacheEntry> g_pipeline_cache;

        struct PipelineResolveStats
        {
            uint64_t requests = 0;
            uint64_t invalid_request = 0;
            uint64_t incomplete_key = 0;
            uint64_t final_cache_hit = 0;
            uint64_t final_cache_miss = 0;
            uint64_t materialize_success = 0;
            uint64_t materialize_failed = 0;
        };

        enum class ResolveError : uint8
        {
            None = 0,
            MissingDevice,
            MissingShaderStages,
            MissingPipelineLayout,
            MissingColorAttachmentConfig,
            InvalidBackendMode
        };

        PipelineResolveStats g_resolve_stats;
        ThreadMutex g_resolver_mutex;

        const char *ResolveErrorText(const ResolveError error)
        {
            switch(error)
            {
            case ResolveError::MissingDevice: return "missing device";
            case ResolveError::MissingShaderStages: return "missing shader_stages";
            case ResolveError::MissingPipelineLayout: return "missing pipeline_layout";
            case ResolveError::MissingColorAttachmentConfig: return "missing frame_output color attachments";
            case ResolveError::InvalidBackendMode: return "invalid graphics pipeline backend mode";
            default: return "none";
            }
        }

        const char *GraphicsPipelineModeName(const GraphicsPipelineMode mode)
        {
            switch(mode)
            {
            case GraphicsPipelineMode::Monolithic: return "Monolithic";
            case GraphicsPipelineMode::GraphicsPipelineLibrary: return "GraphicsPipelineLibrary";
            case GraphicsPipelineMode::ShaderObject: return "ShaderObject";
            default: return "Unknown";
            }
        }

        ResolveError ValidateResolveRequest(const FinalPipelineResolveRequest &request)
        {
            if(!request.device || !request.device->GetDevAttr())
                return ResolveError::MissingDevice;

            if(!GraphicsPipelineBackend::Get(request.device->GetDevAttr()->graphics_pipeline_mode))
                return ResolveError::InvalidBackendMode;

            if(!request.shader_stages || request.shader_stages->IsEmpty())
                return ResolveError::MissingShaderStages;

            if(request.pipeline_layout == VK_NULL_HANDLE)
                return ResolveError::MissingPipelineLayout;

            if(request.frame_output.color_attachment_count == 0)
            {
                if(request.frame_output.depth_stencil_format == VK_FORMAT_UNDEFINED)
                    return ResolveError::MissingColorAttachmentConfig;
            }
            else
            if(!request.frame_output.color_formats)
                return ResolveError::MissingColorAttachmentConfig;

            return ResolveError::None;
        }

        void LogResolveStatsIfNeeded()
        {
            if((g_resolve_stats.requests % 64u) != 0u)
                return;

            GLogInfo("[PipelineResolver] req=%llu invalid=%llu incomplete_key=%llu final_cache(h/m)=%llu/%llu materialize(ok/fail)=%llu/%llu",
                     (unsigned long long)g_resolve_stats.requests,
                     (unsigned long long)g_resolve_stats.invalid_request,
                     (unsigned long long)g_resolve_stats.incomplete_key,
                     (unsigned long long)g_resolve_stats.final_cache_hit,
                     (unsigned long long)g_resolve_stats.final_cache_miss,
                     (unsigned long long)g_resolve_stats.materialize_success,
                     (unsigned long long)g_resolve_stats.materialize_failed);
        }

        bool TryGetCachedFinalPipeline(const FinalPipelineResolveRequest &request,
                                       const FinalPipelineKey &key,
                                       VkPipeline &out_pipeline)
        {
            if(!request.device || request.pipeline_layout == VK_NULL_HANDLE)
                return false;

            for(FinalPipelineCacheEntry &entry : g_pipeline_cache)
            {
                if(entry.device != request.device
                || entry.pipeline_layout != request.pipeline_layout
                || !(entry.key == key)
                || entry.pipeline == VK_NULL_HANDLE)
                    continue;

                out_pipeline = entry.pipeline;
                ++entry.references;
                return true;
            }

            return false;
        }

        void CacheFinalPipeline(const FinalPipelineResolveRequest &request,
                                const FinalPipelineKey &key,
                                const VkPipeline pipeline)
        {
            if(!request.device || request.pipeline_layout == VK_NULL_HANDLE || pipeline == VK_NULL_HANDLE)
                return;

            FinalPipelineCacheEntry entry{};
            entry.device = request.device;
            entry.pipeline_layout = request.pipeline_layout;
            entry.key = key;
            entry.pipeline = pipeline;
            entry.references = 1;
            g_pipeline_cache.Add(entry);
        }
    }

    bool PipelineResolver::BuildFinalPipelineKey(const FinalPipelineResolveRequest &request, FinalPipelineKey &out_key)
    {
        out_key = {};

        if(request.device && request.device->GetDevAttr())
            out_key.backend_mode = request.device->GetDevAttr()->graphics_pipeline_mode;

        out_key.shader_stages_hash = GraphicsPipelineBackend::HashShaderStages(request.device, request.shader_stages);
        out_key.color_attachment_count = request.frame_output.color_attachment_count;
        out_key.color_formats_hash =
            request.frame_output.color_formats && request.frame_output.color_attachment_count > 0
                ? GraphicsPipelineBackend::HashBytes(request.frame_output.color_formats,
                                                     sizeof(VkFormat) * request.frame_output.color_attachment_count)
                : 0;
        out_key.depth_stencil_format = request.frame_output.depth_stencil_format;
        return true;
    }

    bool PipelineResolver::HasCompleteFinalKey(const FinalPipelineKey &key)
    {
        if(key.shader_stages_hash == 0)
            return false;

        if(key.color_attachment_count == 0 && key.depth_stencil_format == VK_FORMAT_UNDEFINED)
            return false;

        return true;
    }

    bool PipelineResolver::MaterializePipeline(const FinalPipelineResolveRequest &request, VkPipeline &out_pipeline)
    {
        out_pipeline = VK_NULL_HANDLE;
        if(!request.device || !request.device->GetDevAttr())
            return false;

        const GraphicsPipelineMode mode = request.device->GetDevAttr()->graphics_pipeline_mode;
        GraphicsPipelineBackend *backend = GraphicsPipelineBackend::Get(mode);
        if(!backend)
        {
            GLogError("[PipelineResolver] Invalid graphics backend mode value: %u",
                      static_cast<unsigned>(mode));
            return false;
        }

        if(backend->GetMode() != mode)
        {
            GLogError("[PipelineResolver] Backend mode mismatch for '%s'", GraphicsPipelineModeName(mode));
            return false;
        }

        return backend->Materialize(request, out_pipeline);
    }

    bool PipelineResolver::ResolveFinalPipeline(const FinalPipelineResolveRequest &request,
                                                FinalPipelineResolveResult &out_result)
    {
        ThreadMutexLock resolver_lock(&g_resolver_mutex);

        out_result = {};
        ++g_resolve_stats.requests;

        const ResolveError request_error = ValidateResolveRequest(request);
        if(request_error != ResolveError::None)
        {
            ++g_resolve_stats.invalid_request;
            if(request_error == ResolveError::InvalidBackendMode && request.device && request.device->GetDevAttr())
            {
                GLogError("[PipelineResolver] Resolve failed: %s ('%s')",
                          ResolveErrorText(request_error),
                          GraphicsPipelineModeName(request.device->GetDevAttr()->graphics_pipeline_mode));
            }
            else
            {
                GLogError("[PipelineResolver] Resolve failed: %s", ResolveErrorText(request_error));
            }
            LogResolveStatsIfNeeded();
            return false;
        }

        BuildFinalPipelineKey(request, out_result.key);
        if(!HasCompleteFinalKey(out_result.key))
        {
            ++g_resolve_stats.incomplete_key;
            GLogError("[PipelineResolver] Resolve failed: incomplete FinalPipelineKey");
            LogResolveStatsIfNeeded();
            return false;
        }

        if(TryGetCachedFinalPipeline(request, out_result.key, out_result.pipeline))
        {
            ++g_resolve_stats.final_cache_hit;
            LogResolveStatsIfNeeded();
            return true;
        }

        ++g_resolve_stats.final_cache_miss;
        if(!MaterializePipeline(request, out_result.pipeline))
        {
            ++g_resolve_stats.materialize_failed;
            GLogError("[PipelineResolver] Materialize failed");
            LogResolveStatsIfNeeded();
            return false;
        }

        ++g_resolve_stats.materialize_success;
        CacheFinalPipeline(request, out_result.key, out_result.pipeline);
        LogResolveStatsIfNeeded();
        return true;
    }

    void PipelineResolver::ReleaseFinalPipeline(VulkanDevice *device, const VkPipeline pipeline)
    {
        ThreadMutexLock resolver_lock(&g_resolver_mutex);
        if(!device || pipeline == VK_NULL_HANDLE)
            return;

        for(int i = g_pipeline_cache.GetCount() - 1; i >= 0; --i)
        {
            FinalPipelineCacheEntry &entry = g_pipeline_cache[i];
            if(entry.device != device || entry.pipeline != pipeline)
                continue;

            if(entry.references > 1)
                --entry.references;
            else
                g_pipeline_cache.Delete(i);
        }
    }

    void PipelineResolver::ClearCacheForDevice(VulkanDevice *device)
    {
        ThreadMutexLock resolver_lock(&g_resolver_mutex);
        if(!device)
            return;

        GraphicsPipelineBackend *backend = nullptr;
        if(device->GetDevAttr())
            backend = GraphicsPipelineBackend::Get(device->GetDevAttr()->graphics_pipeline_mode);
        if(backend)
            backend->ClearCacheForDevice(device);

        int write = 0;
        const int count = g_pipeline_cache.GetCount();
        for(int i = 0; i < count; ++i)
        {
            if(g_pipeline_cache[i].device != device)
            {
                if(write != i)
                    g_pipeline_cache[write] = g_pipeline_cache[i];
                ++write;
            }
        }
        while(g_pipeline_cache.GetCount() > write)
            g_pipeline_cache.Delete(g_pipeline_cache.GetCount() - 1);

        GLogInfo("[PipelineResolver] Cleared caches for device %p. Remaining: %d",
                 (void *)device,
                 g_pipeline_cache.GetCount());
    }
}
