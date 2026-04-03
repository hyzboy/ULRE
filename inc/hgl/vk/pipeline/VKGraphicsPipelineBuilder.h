#pragma once

#include<hgl/vk/VK.h>
#include<hgl/log/Log.h>
#include<hgl/vk/pipeline/VKGplLibraryPool.h>
#include<memory>

namespace hgl::graph{
class Pipeline;
class VulkanDevice;
class GplLibraryPool;
struct GraphicsPipelineBuildRequest;

enum class LinkBackendType
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
    virtual LinkBackendType GetType() const = 0;
    virtual Pipeline *Build(const PipelineBuildContext &, const GraphicsPipelineBuildRequest &) = 0;
};

class MonolithicGraphicsPipelineBuilder final : public IGraphicsPipelineBuilder
{
    OBJECT_LOGGER

public:
    LinkBackendType GetType() const override { return LinkBackendType::Monolithic; }
    Pipeline *Build(const PipelineBuildContext &, const GraphicsPipelineBuildRequest &) override;
};

class GplLinkBackend final : public IGraphicsPipelineBuilder
{
    OBJECT_LOGGER

    std::unique_ptr<GplLibraryPool> library_pool_;
    std::once_flag                  init_flag_;

public:
    LinkBackendType GetType() const override { return LinkBackendType::Gpl; }
    Pipeline *Build(const PipelineBuildContext &, const GraphicsPipelineBuildRequest &) override;
};
}//namespace hgl::graph
