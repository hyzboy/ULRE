#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/pipeline/VKPipeline.h>
#include<hgl/type/ManagedArray.h>
#include<hgl/mtl/new/PlatformBackend.h>
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

    Pipeline *CreatePipeline(const AnsiString &,PipelineData *,const ShaderStageCreateInfoList &,VkPipelineLayout,const VIL *,const GeometryVertexFormat *gvf=nullptr);

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

    Pipeline *CreatePipeline(MaterialProgram *,const VIL *,const PipelineData *,   const bool prim_restart=false,const GeometryVertexFormat *gvf=nullptr);
    Pipeline *CreatePipeline(MaterialProgram *,const VIL *,const InlinePipeline &, const bool prim_restart=false,const GeometryVertexFormat *gvf=nullptr);

    Pipeline *CreatePipeline(MaterialProgram *mtl,         const PipelineData *,   const bool prim_restart=false);
    Pipeline *CreatePipeline(MaterialProgram *mtl,         const InlinePipeline &, const bool prim_restart=false);

    Pipeline *CreatePipeline(MaterialInstance *,    const InlinePipeline &, const bool prim_restart=false);
    Pipeline *CreatePipeline(MaterialInstance *,    const PipelineData *,   const bool prim_restart=false);
    Pipeline *CreatePipeline(MaterialInstance *,    const OSString &,       const bool prim_restart=false);

    // GeometryFetchMode-aware: when SSBO, uses empty VertexInput (nullptr VIL)
    Pipeline *CreatePipeline(MaterialInstance *,    const InlinePipeline &, GeometryFetchMode fetch_mode, const bool prim_restart=false);
    Pipeline *CreatePipeline(MaterialInstance *,    const PipelineData *,   GeometryFetchMode fetch_mode, const bool prim_restart=false);

    /**
     * 从原始着色器阶段 + Pipeline Layout + VIL 创建管线（供 Compositor 系统使用）
     */
    Pipeline *CreatePipeline(const AnsiString &name,
                             const ShaderStageCreateInfoList &ssci,
                             VkPipelineLayout layout,
                             const VIL *vil,
                             const PipelineData *pd,
                             PrimitiveType prim = PrimitiveType::Triangles,
                             bool prim_restart = false,
                             const GeometryVertexFormat *gvf=nullptr);
};//class RenderPass
}//namespace hgl::graph
