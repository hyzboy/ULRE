#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/pipeline/VKPipeline.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/type/ManagedArray.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/log/Log.h>

namespace hgl::graph{

class GeometryVertexFormat;

using VkFormatList=ValueArray<VkFormat>;

/**
 * RenderPass功能封装<br>
 * RenderPass在创建时，需要指定输入的color imageview与depth imageview象素格式，
 * 在随后创建Framebuffer时，需要同时指定RenderPass与ColorImageView,DepthImageView，象素格式要一致。
 */
class RenderPass
{
    OBJECT_LOGGER

    VulkanDevice *  device;
    VkPipelineCache pipeline_cache;
    VkRenderPass    render_pass;
    AnsiString      name;               ///< RenderPass名称，用于调试

    VkFormatList    color_formats;
    VkFormat        depth_format;

    VkExtent2D      granularity;        //FBO尺寸对齐粒度

protected:

    ManagedArray<Pipeline> pipeline_list;

    Pipeline *CreatePipeline(const AnsiString &,
                             const ShaderStageCreateInfoList &,
                             VkPipelineLayout,
                             const mtl::MaterialPipelineConfig &config);

private:

    friend class RenderPassManager;

    RenderPass(VulkanDevice *,const AnsiString &name,VkRenderPass rp,const VkFormatList &cf,VkFormat df);

public:

    virtual ~RenderPass();

    operator const VkRenderPass()const{return render_pass;}

    const AnsiString &      GetName         ()const{return name;}
    const VkRenderPass      GetVkRenderPass ()const{return render_pass;}
    const VkPipelineCache   GetPipelineCache()const{return pipeline_cache;}

    const uint              GetColorCount   ()const{return color_formats.GetCount();}
    const VkFormatList &    GetColorFormat  ()const{return color_formats;}
    const VkFormat          GetColorFormat  (int index)const
    {
        if(index<0||index>=color_formats.GetCount())return VK_FORMAT_UNDEFINED;

        return color_formats.GetData()[index];
    }
    const VkFormat          GetDepthFormat  ()const{return depth_format;}

    const VkExtent2D &      GetGranularity  ()const{return granularity;}

public:

    Pipeline *CreatePipeline(ShaderProgram *,const mtl::MaterialPipelineConfig &config);
    Pipeline *CreatePipeline(ShaderProgram *,const mtl::MaterialRecipe &recipe);

    /**
     * 从原始着色器阶段 + Pipeline Layout 创建管线（供 Compositor 系统使用）
     */
    Pipeline *CreatePipeline(const AnsiString &name,
                             const ShaderStageCreateInfoList &ssci,
                             VkPipelineLayout layout);
};//class RenderPass
}//namespace hgl::graph
