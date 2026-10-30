#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/pipeline/VKPipeline.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/type/ManagedArray.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/log/Log.h>

namespace hgl::graph{

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

    /**
     * 已解析 pipeline 包装对象的旁表（**上层早退**，区别于 PipelineResolver 的
     * `FinalPipelineKey` 缓存）。
     *
     * 为什么只需 `程序内容摘要 + 是否剥 FS` 两个字段：
     * - 颜色/深度附件格式是本 RenderPass 实例的常量（构造时固定，不随材质变）；
     * - 管线包装对象与 VkPipeline 句柄**一一对应**（`~Pipeline` 独占
     *   `vkDestroyPipeline`，一个句柄两个包装对象 = 双重销毁），所以"同程序同通道
     *   形态"必须复用同一个包装对象——这与既有的"扫 pipeline_list 找同
     *   VkPipeline"语义逐字一致，只是把 O(全局管线数) 的锁+线性扫描换成 O(1) 摘要比较；
     * - 键用 `mtl::ShaderProgramKey::GetDigest()`（内容派生，非指针身份——D2：
     *   对象地址复用会误判）。
     *
     * 命中的收益：不再进 `PipelineResolver::ResolveFinalPipeline`（全局互斥 +
     * `g_pipeline_cache` 全表线性扫描 + 每 stage 的模块 hash 查询），也不再打
     * `[RenderPass::CreatePipeline] Reuse existing Pipeline wrapper` 调试日志
     *（该日志曾是每图元每帧一条的刷屏源）。
     */
    struct PipelineMemoEntry
    {
        uint64      program_digest          = 0;    ///< 程序内容摘要（ShaderProgramKey::GetDigest）
        bool        strip_fragment_shader   = false;///< 本通道形态下是否剥掉片元阶段
        Pipeline *  pipeline                = nullptr;

        /// ValueArray 的 Remove/Find 需要；语义上不参与判重。
        bool operator == (const PipelineMemoEntry &rhs) const
        {
            return pipeline==rhs.pipeline;
        }
    };

    ValueArray<PipelineMemoEntry> pipeline_memo;

    /**
     * 程序 → pipeline 包装对象的解析入口（两个 program 重载的唯一实现）。
     *
     * 先查 `pipeline_memo`：命中直接返回同一包装对象，**完全不进 Vulkan 层**；
     * 未命中才走 `CreatePipeline(name, stages, layout, config, keep_fs)` 的完整路径。
     */
    Pipeline *CreatePipelineForProgram(ShaderProgram *,
                                       const mtl::MaterialPipelineConfig &config,
                                       bool keep_fragment_shader);

    Pipeline *CreatePipeline(const AnsiString &,
                             const ShaderStageCreateInfoList &,
                             VkPipelineLayout,
                             const mtl::MaterialPipelineConfig &config,
                             bool keep_fragment_shader = false);

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

    /**
     * recipe 重载：从 recipe 解析出 `ResolvedMaterialRenderState` 后建管线。
     *
     * EDS 1/2/3 下管线**不保存**渲染状态 ⇒ 解析出的 `MaterialPipelineConfig` 是调用方
     * 必须带走的那一份（绘制期用它下发动态状态）。给了 `out_config` 就回传，没给则丢弃
     * ——调用方若还要画这一批，就必须取。
     */
    Pipeline *CreatePipeline(ShaderProgram *,const mtl::MaterialRecipe &recipe,
                             mtl::MaterialPipelineConfig *out_config = nullptr);

    /**
     * 从原始着色器阶段 + Pipeline Layout 创建管线（供 Compositor 系统使用）
     */
    Pipeline *CreatePipeline(const AnsiString &name,
                             const ShaderStageCreateInfoList &ssci,
                             VkPipelineLayout layout);
};//class RenderPass
}//namespace hgl::graph
