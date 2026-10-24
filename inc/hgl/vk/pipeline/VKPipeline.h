#pragma once

#include<hgl/vk/VK.h>

namespace hgl::graph{
/**
 * `VkPipeline` 的**薄包装**（只管「句柄 + 名字 + 设备」）。
 *
 * ## 为什么这里**没有**渲染状态（重要，勿再"补回来"）
 *
 * 本引擎已全面启用 Vulkan 的 **EDS 1/2/3（Extended Dynamic State）**：
 *
 * - `VKPipelineResolver::BuildFinalPipelineKey` 的键只有
 *   `shader_stages_hash / color_attachment_count / color_formats_hash / depth_stencil_format`
 *   ⇒ 底层 `VkPipeline` 只由 **Shader Stages + FBO（RenderPass）附件格式** 决定；
 * - `MaterializePipeline` 把全部可变状态声明为 `VK_DYNAMIC_STATE_*`
 *   （viewport/scissor/cull/depth test/depth write/depth compare/color blend enable+equation/
 *    color write mask/blend constants/polygon mode/line width 共 12 项）
 *   ⇒ 状态一律由渲染期 `vkCmdSet*`（`RenderCmdBuffer::ApplyPipelineState`）下发。
 *
 * 由此推出两条必须遵守的结论：
 *
 * 1. **不要把 `MaterialPipelineConfig`（cull/depth/blend/...）拷进 Pipeline**。
 *    两个材质即使配置不同，只要 shader 与 FBO 格式相同，就**应当**共用同一个 `VkPipeline`
 *    ——这正是 EDS 的收益。把配置存在这里只会制造"同句柄、多份互相矛盾的配置副本"，
 *    读到的到底是哪一份取决于谁先创建（历史 bug：`pipeline_memo` 命中后拿到首个包装对象，
 *    绘制时套用了别的材质的 cull/blend 状态）。
 *
 * 2. **不要在绘制期回读 Pipeline 取渲染状态**。
 *    状态由**使用方**提供：ECS 侧是 `MaterialBatch::pipeline_config`（合批键的一部分，
 *    ⇒ 配置不同的图元天然拆成不同批，每批只下发一次动态状态）；
 *    Line/Text 等自建管线由各自持有的配置成员提供。
 *
 * 同理 `overlay`（是否走覆盖层 pass）也是**材质/批次事实**，不在 Pipeline 上——
 * 见 `MaterialBatch::pipeline_config.overlay` 与 `PrimitiveRenderSystem::IsOverlayLikeBatch`。
 */
class Pipeline
{
    VkDevice device;

    AnsiString name;

    VkPipeline pipeline;

private:

    friend class RenderPass;

    Pipeline(const AnsiString &n,
             VkDevice dev,
             VkPipeline p)
    {
        name=n;
        device=dev;
        pipeline=p;
    }

public:

    virtual ~Pipeline();

    const AnsiString &GetName()const{return name;}

    operator VkPipeline(){return pipeline;}
};//class Pipeline
}//namespace hgl::graph
