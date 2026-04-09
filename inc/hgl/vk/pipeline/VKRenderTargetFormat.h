#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/pipeline/VKGraphicsPipeline.h>
#include<hgl/log/Log.h>
#include<vector>

namespace hgl::graph{

using VkFormatList=std::vector<VkFormat>;

/**
 * RenderTargetFormat — pipeline factory keyed on attachment formats.<br>
 * Holds colour/depth attachment formats, creates Pipelines using
 * VkPipelineRenderingCreateInfoKHR (Dynamic Rendering; no VkRenderPass).
 */
class RenderTargetFormat
{
    OBJECT_LOGGER

    VulkanDevice *  device;
    VkPipelineCache pipeline_cache;
    std::string     name;               ///< RenderFormat名称，用于调试

    VkFormatList    color_formats;
    VkFormat        depth_format;

private:

    friend class VulkanDevice;

    RenderTargetFormat(VulkanDevice *,const std::string &name,const VkFormatList &cf,VkFormat df);

public:

    virtual ~RenderTargetFormat();

    const std::string &     GetName         ()const{return name;}
    const VkPipelineCache   GetPipelineCache()const{return pipeline_cache;}

    const uint              GetColorCount   ()const{return static_cast<uint>(color_formats.size());}
    const VkFormatList &    GetColorFormat  ()const{return color_formats;}
    const VkFormat          GetColorFormat  (int index)const
    {
        if(index<0||index>=static_cast<int>(color_formats.size()))return VK_FORMAT_UNDEFINED;

        return color_formats[index];
    }
    const VkFormat          GetDepthFormat  ()const{return depth_format;}

public:
    /// Returns the process-lifetime count of successful vkCreateGraphicsPipelines calls.
    /// Use the delta between two calls to count pipelines created in a time window.
    static uint64_t GetVkCreateCount();

    /// Increments the global vkCreateGraphicsPipelines counter by 1.
    /// Called by non-RenderTargetFormat paths (e.g. GplGraphicsPipelineBuilder) that issue their own vkCreateGraphicsPipelines.
    static void IncrVkCreateCount();
};//class RenderTargetFormat
}//namespace hgl::graph
