#pragma once

#include<hgl/vk/VK.h>
#include<hgl/log/Log.h>
#include<hgl/vk/pipeline/VKGplLibraryHandleCache.h>
#include<memory>

namespace hgl::graph{
class Pipeline;
class VulkanDevice;
class GplLibraryHandleCache;
struct GraphicsPipelineBuildRequest;

enum class GraphicsPipelineBuilderType
{
    Monolithic,
    Gpl,
};

struct PipelineBuildContext
{
    VulkanDevice *device = nullptr;
    VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
};

class IGraphicsPipelineBuilder
{
public:
    virtual ~IGraphicsPipelineBuilder() = default;
    virtual GraphicsPipelineBuilderType GetType() const = 0;
    virtual Pipeline *Build(const PipelineBuildContext &, const GraphicsPipelineBuildRequest &) = 0;
};

class MonolithicGraphicsPipelineBuilder final : public IGraphicsPipelineBuilder
{
    OBJECT_LOGGER

public:
    GraphicsPipelineBuilderType GetType() const override { return GraphicsPipelineBuilderType::Monolithic; }
    Pipeline *Build(const PipelineBuildContext &, const GraphicsPipelineBuildRequest &) override;
};

class GplGraphicsPipelineBuilder final : public IGraphicsPipelineBuilder
{
    OBJECT_LOGGER

    std::unique_ptr<GplLibraryHandleCache> library_pool_;
    std::once_flag                  init_flag_;

public:
    GraphicsPipelineBuilderType GetType() const override { return GraphicsPipelineBuilderType::Gpl; }
    Pipeline *Build(const PipelineBuildContext &, const GraphicsPipelineBuildRequest &) override;
};
}//namespace hgl::graph
