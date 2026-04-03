#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/pipeline/VKGraphicsPipeline.h>
#include<hgl/log/Log.h>

namespace hgl::graph{

using VkFormatList=ValueArray<VkFormat>;

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
    AnsiString      name;               ///< RenderFormat名称，用于调试

    VkFormatList    color_formats;
    VkFormat        depth_format;

protected:

    GraphicsPipeline *CreatePipeline(const AnsiString &,GraphicsPipelineData *,const ShaderStageCreateInfoList &,VkPipelineLayout,const VIL *);

private:

    friend class VulkanDevice;

    RenderTargetFormat(VulkanDevice *,const AnsiString &name,const VkFormatList &cf,VkFormat df);

public:

    virtual ~RenderTargetFormat();

    const AnsiString &      GetName         ()const{return name;}
    const VkPipelineCache   GetPipelineCache()const{return pipeline_cache;}

    const uint              GetColorCount   ()const{return color_formats.GetCount();}
    const VkFormatList &    GetColorFormat  ()const{return color_formats;}
    const VkFormat          GetColorFormat  (int index)const
    {
        if(index<0||index>=color_formats.GetCount())return VK_FORMAT_UNDEFINED;

        return color_formats.GetData()[index];
    }
    const VkFormat          GetDepthFormat  ()const{return depth_format;}

public:

    GraphicsPipeline *CreatePipeline(Material *,const VIL *,const GraphicsPipelineData *,   const bool prim_restart=false);
    GraphicsPipeline *CreatePipeline(Material *,const VIL *,const GraphicsPipelinePreset &, const bool prim_restart=false);

    GraphicsPipeline *CreatePipeline(Material *mtl,         const GraphicsPipelineData *,   const bool prim_restart=false);
    GraphicsPipeline *CreatePipeline(Material *mtl,         const GraphicsPipelinePreset &, const bool prim_restart=false);

    GraphicsPipeline *CreatePipeline(MaterialInstance *,    const GraphicsPipelinePreset &, const bool prim_restart=false);
    GraphicsPipeline *CreatePipeline(MaterialInstance *,    const GraphicsPipelineData *,   const bool prim_restart=false);
    GraphicsPipeline *CreatePipeline(MaterialInstance *,    const OSString &,       const bool prim_restart=false);

    /**
     * 从原始着色器阶段 + GraphicsPipeline Layout + VIL 创建管线（供 Compositor 系统使用）
     */
    GraphicsPipeline *CreatePipeline(const AnsiString &name,
                             const ShaderStageCreateInfoList &ssci,
                             VkPipelineLayout layout,
                             const VIL *vil,
                             const GraphicsPipelineData *pd,
                             PrimitiveType prim = PrimitiveType::Triangles,
                             bool prim_restart = false);

    /// Returns the process-lifetime count of successful vkCreateGraphicsPipelines calls.
    /// Use the delta between two calls to count pipelines created in a time window.
    static uint64_t GetVkCreateCount();

    /// Increments the global vkCreateGraphicsPipelines counter by 1.
    /// Called by non-RenderTargetFormat paths (e.g. GplGraphicsPipelineBuilder) that issue their own vkCreateGraphicsPipelines.
    static void IncrVkCreateCount();
};//class RenderTargetFormat
}//namespace hgl::graph
