#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/pipeline/VKPipeline.h>
#include<hgl/type/ManagedArray.h>
#include<hgl/type/UnorderedMap.h>
#include<hgl/log/Log.h>

namespace hgl::graph{

using VkFormatList=ValueArray<VkFormat>;

/**
 * RenderFormat — pipeline factory keyed on attachment formats.<br>
 * Holds colour/depth attachment formats, creates Pipelines using
 * VkPipelineRenderingCreateInfoKHR (Dynamic Rendering; no VkRenderPass).
 */
class RenderFormat
{
    OBJECT_LOGGER

    VulkanDevice *  device;
    VkPipelineCache pipeline_cache;
    AnsiString      name;               ///< RenderFormat名称，用于调试

    VkFormatList    color_formats;
    VkFormat        depth_format;

protected:

    ManagedArray<Pipeline> pipeline_list;
    UnorderedMap<AnsiString, Pipeline *> pipeline_by_name;  ///< dedup cache: key=(mtl|vil|cpd|restart)

    Pipeline *CreatePipeline(const AnsiString &,PipelineData *,const ShaderStageCreateInfoList &,VkPipelineLayout,const VIL *);

private:

    friend class VulkanDevice;

    RenderFormat(VulkanDevice *,const AnsiString &name,const VkFormatList &cf,VkFormat df);

public:

    virtual ~RenderFormat();

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

    Pipeline *CreatePipeline(Material *,const VIL *,const PipelineData *,   const bool prim_restart=false);
    Pipeline *CreatePipeline(Material *,const VIL *,const InlinePipeline &, const bool prim_restart=false);

    Pipeline *CreatePipeline(Material *mtl,         const PipelineData *,   const bool prim_restart=false);
    Pipeline *CreatePipeline(Material *mtl,         const InlinePipeline &, const bool prim_restart=false);

    Pipeline *CreatePipeline(MaterialInstance *,    const InlinePipeline &, const bool prim_restart=false);
    Pipeline *CreatePipeline(MaterialInstance *,    const PipelineData *,   const bool prim_restart=false);
    Pipeline *CreatePipeline(MaterialInstance *,    const OSString &,       const bool prim_restart=false);

    /**
     * 从原始着色器阶段 + Pipeline Layout + VIL 创建管线（供 Compositor 系统使用）
     */
    Pipeline *CreatePipeline(const AnsiString &name,
                             const ShaderStageCreateInfoList &ssci,
                             VkPipelineLayout layout,
                             const VIL *vil,
                             const PipelineData *pd,
                             PrimitiveType prim = PrimitiveType::Triangles,
                             bool prim_restart = false);
};//class RenderFormat
}//namespace hgl::graph
