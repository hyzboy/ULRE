#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/pipeline/VKPipeline.h>
#include<hgl/type/ObjectList.h>

VK_NAMESPACE_BEGIN

using VkFormatList=ArrayList<VkFormat>;

/**
 * RenderPass功能封装<br>
 * RenderPass在创建时，需要指定输入的color imageview与depth imageview象素格式，
 * 在随后创建Framebuffer时，需要同时指定RenderPass与ColorImageView,DepthImageView，象素格式要一致。
 */
class RenderPass
{
    VulkanDevice *device;
    VkPipelineCache pipeline_cache;
    VkRenderPass render_pass;

    VkFormatList color_formats;
    VkFormat depth_format;

    VkExtent2D granularity;

protected:

    ObjectList<Pipeline> pipeline_list;

    Pipeline *CreatePipeline(const AnsiString &,PipelineData *,const ShaderStageCreateInfoList &,VkPipelineLayout,const VIL *);

private:

    friend class RenderPassManager;

    RenderPass(VulkanDevice *,VkRenderPass rp,const VkFormatList &cf,VkFormat df);

public:

    virtual ~RenderPass();

    operator const VkRenderPass()const{return render_pass;}

    const VkRenderPass      GetVkRenderPass()const{return render_pass;}
    const VkPipelineCache   GetPipelineCache()const{return pipeline_cache;}

    const uint              GetColorCount()const{return color_formats.GetCount();}
    const VkFormatList &    GetColorFormat()const{return color_formats;}
    const VkFormat          GetColorFormat(int index)const
    {
        if(index<0||index>=color_formats.GetCount())return VK_FORMAT_UNDEFINED;

        return color_formats.GetData()[index];
    }
    const VkFormat          GetDepthFormat()const{return depth_format;}

    const VkExtent2D &      GetGranularity()const{return granularity;}

public:

    Pipeline *CreatePipeline(Material *,const VIL *,const PipelineData *,   const bool prim_restart=false);
    Pipeline *CreatePipeline(Material *,const VIL *,const InlinePipeline &, const bool prim_restart=false);

    Pipeline *CreatePipeline(Material *mtl,         const PipelineData *,   const bool prim_restart=false);
    Pipeline *CreatePipeline(Material *mtl,         const InlinePipeline &, const bool prim_restart=false);

    Pipeline *CreatePipeline(MaterialInstance *,    const InlinePipeline &, const bool prim_restart=false);
    Pipeline *CreatePipeline(MaterialInstance *,    const PipelineData *,   const bool prim_restart=false);
    Pipeline *CreatePipeline(MaterialInstance *,    const OSString &,       const bool prim_restart=false);
};//class RenderPass
VK_NAMESPACE_END
