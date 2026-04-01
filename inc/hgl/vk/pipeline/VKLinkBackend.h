#pragma once

#include<hgl/vk/VK.h>
#include<hgl/log/Log.h>

namespace hgl::graph{
class Pipeline;
class VulkanDevice;
struct GplPipelineRequest;

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

class ILinkBackend
{
public:
    virtual ~ILinkBackend() = default;
    virtual LinkBackendType GetType() const = 0;
    virtual Pipeline *Build(const PipelineBuildContext &, const GplPipelineRequest &) = 0;
};

class MonolithicLinkBackend final : public ILinkBackend
{
    OBJECT_LOGGER

public:
    LinkBackendType GetType() const override { return LinkBackendType::Monolithic; }
    Pipeline *Build(const PipelineBuildContext &, const GplPipelineRequest &) override;
};

class GplLinkBackend final : public ILinkBackend
{
    OBJECT_LOGGER
public:
    LinkBackendType GetType() const override { return LinkBackendType::Gpl; }
    Pipeline *Build(const PipelineBuildContext &, const GplPipelineRequest &) override;
};
}//namespace hgl::graph
