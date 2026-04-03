#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineBuildRequest.h>
#include<mutex>
#include<unordered_map>
#include<memory>

namespace hgl::graph{
struct GraphicsPipelineBuildRequest;

/**
 * GPL 四段库缓存池
 *
 * 以四个 XxxKey 为 key 分别缓存 VkPipeline 库 handle。
 * 同一 key 多次请求只创建一次，缓存命中时直接返回已有 handle。
 * 库 handle 在 GplLibraryHandleCache 析构时统一销毁（保留供后续 variant 快速 link）。
 */
class GplLibraryHandleCache
{
    VkDevice        device_         = VK_NULL_HANDLE;
    VkPipelineCache pipeline_cache_ = VK_NULL_HANDLE;

    mutable std::mutex lib_mutex_;

    std::unordered_map<GplVertexInputKey,   VkPipeline> vi_lib_;
    std::unordered_map<GplPreRasterKey,     VkPipeline> pr_lib_;
    std::unordered_map<GplFragmentShaderKey,VkPipeline> fs_lib_;
    std::unordered_map<GplFragmentOutputKey,VkPipeline> fo_lib_;

public:
    GplLibraryHandleCache() = default;
    ~GplLibraryHandleCache();

    void Init(VkDevice device, VkPipelineCache pipeline_cache);

    /**
     * AcquireVertexInputLibrary  — Vertex Input Interface 库
     * @param key    本次请求对应的 VertexInputKey
     * @param req    完整 GraphicsPipelineBuildRequest（用于首次创建）
     * @return 成功返回 VkPipeline（库 handle），失败返回 VK_NULL_HANDLE
     */
    VkPipeline AcquireVertexInputLibrary(const GplVertexInputKey   &key, const GraphicsPipelineBuildRequest &req);

    /**
     * AcquirePreRasterLibrary  — Pre-Rasterization Shaders 库
     */
    VkPipeline AcquirePreRasterLibrary(const GplPreRasterKey     &key, const GraphicsPipelineBuildRequest &req);

    /**
     * AcquireFragmentShaderLibrary  — Fragment Shader 库
     */
    VkPipeline AcquireFragmentShaderLibrary(const GplFragmentShaderKey &key, const GraphicsPipelineBuildRequest &req);

    /**
     * AcquireFragmentOutputLibrary  — Fragment Output Interface 库
     */
    VkPipeline AcquireFragmentOutputLibrary(const GplFragmentOutputKey &key, const GraphicsPipelineBuildRequest &req);
};
}//namespace hgl::graph
