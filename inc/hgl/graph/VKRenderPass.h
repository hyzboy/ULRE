#ifndef HGL_GRAPH_VULKAN_RENDER_PASS_INCLUDE
#define HGL_GRAPH_VULKAN_RENDER_PASS_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/type/ObjectList.h>
VK_NAMESPACE_BEGIN
/**
 * RenderPass功能封装<br>
 * RenderPass在创建时，需要指定输入的color imageview与depth imageview象素格式，
 * 在随后创建Framebuffer时，需要同时指定RenderPass与ColorImageView,DepthImageView，象素格式要一致。
 */
class RenderPass
{
    VkDevice device;
    VkPipelineCache pipeline_cache;
    VkRenderPass render_pass;

    List<VkFormat> color_formats;
    VkFormat depth_format;

    VkExtent2D granularity;

protected:

    ObjectList<Pipeline> pipeline_list;

    Pipeline *CreatePipeline(const AnsiString &,PipelineData *,const ShaderStageCreateInfoList &,VkPipelineLayout,const VIL *);

private:

    friend class RenderPassManager;

    RenderPass(VkDevice d,VkPipelineCache pc,VkRenderPass rp,const List<VkFormat> &cf,VkFormat df);

public:

    virtual ~RenderPass();

            VkRenderPass    GetVkRenderPass(){return render_pass;}
            VkPipelineCache GetPipelineCache(){return pipeline_cache;}

    const uint              GetColorCount()const{return color_formats.GetCount();}
    const List<VkFormat> &  GetColorFormat()const{return color_formats;}
    const VkFormat          GetColorFormat(int index)const
    {
        if(index<0||index>=color_formats.GetCount())return VK_FORMAT_UNDEFINED;

        return color_formats.GetData()[index];
    }
    const VkFormat          GetDepthFormat()const{return depth_format;}

    const VkExtent2D &      GetGranularity()const{return granularity;}

public:

    Pipeline *CreatePipeline(Material *,const VIL *,const PipelineData *,   const Prim &,const bool prim_restart=false);
    Pipeline *CreatePipeline(Material *,const VIL *,const InlinePipeline &, const Prim &,const bool prim_restart=false);

    Pipeline *CreatePipeline(Material *mtl,         const PipelineData *,   const Prim &,const bool prim_restart=false);
    Pipeline *CreatePipeline(Material *mtl,         const InlinePipeline &, const Prim &,const bool prim_restart=false);

    Pipeline *CreatePipeline(MaterialInstance *,    const InlinePipeline &, const Prim &,const bool prim_restart=false);
    Pipeline *CreatePipeline(MaterialInstance *,    const PipelineData *,   const Prim &,const bool prim_restart=false);
    Pipeline *CreatePipeline(MaterialInstance *,    const OSString &,       const Prim &,const bool prim_restart=false);
};//class RenderPass
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_RENDER_PASS_INCLUDE
