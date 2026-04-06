#pragma once

#include<hgl/vk/VK.h>
#include<hgl/log/Log.h>
#include<hgl/vk/pipeline/VKGplLibraryHandleCache.h>
#include<memory>

namespace hgl::graph{
class GraphicsPipeline;
class VulkanDevice;
class GplLibraryHandleCache;
struct GraphicsPipelineBuildRequest;

enum class GraphicsPipelineBuilderType
{
    Monolithic,
    Gpl,
};

struct GraphicsPipelineBuildContext
{
    VulkanDevice *device = nullptr;
    VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
};

class IGraphicsPipelineBuilder
{
public:
    virtual ~IGraphicsPipelineBuilder() = default;
    virtual GraphicsPipelineBuilderType GetType() const = 0;
    virtual GraphicsPipeline *Build(const GraphicsPipelineBuildContext &, const GraphicsPipelineBuildRequest &) = 0;
};

class MonolithicGraphicsPipelineBuilder final : public IGraphicsPipelineBuilder
{
    OBJECT_LOGGER

public:
    GraphicsPipelineBuilderType GetType() const override { return GraphicsPipelineBuilderType::Monolithic; }
    GraphicsPipeline *Build(const GraphicsPipelineBuildContext &, const GraphicsPipelineBuildRequest &) override;
};

class GplGraphicsPipelineBuilder final : public IGraphicsPipelineBuilder
{
    OBJECT_LOGGER

    std::unique_ptr<GplLibraryHandleCache> handle_cache_;
    std::once_flag                  init_flag_;

public:
    GraphicsPipelineBuilderType GetType() const override { return GraphicsPipelineBuilderType::Gpl; }
    GraphicsPipeline *Build(const GraphicsPipelineBuildContext &, const GraphicsPipelineBuildRequest &) override;
};
}//namespace hgl::graph
