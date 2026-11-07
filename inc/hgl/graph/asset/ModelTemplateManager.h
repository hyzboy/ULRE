#pragma once

#include <hgl/CoreType.h>

namespace hgl::graph
{
    class GlobalSSBOBufferRegistry;
    struct ModelAssetContent;

    /**
     * ModelTemplateManager —— GPU 常驻「模型模板池」的打包与登记（plan T1.3）。
     *
     * 分工：本类只做**打包**（`ModelAssetContent` 的节点树 → 模板行 + 连续部件行）与
     * **池生命周期**（发号 / 还号）；模板池本体由 `GlobalSSBOBufferRegistry` 托管
     * （`GlobalSSBOType::ModelTemplatePool / ModelPartPool`，BDA 地址经
     * `GlobalAddresses.addr_model_template_pool / addr_model_part_pool` 下发给着色器）。
     *
     * 语义约定（详见 `ModelTemplateRows.h` 头注释）：
     * - 返回值 0（`ssbo::kInvalidModelTemplateID`）= 注册失败 / 无可用部件；
     * - 同一份 content 重复注册会得到**多个** template_id（每份独立部件段）；
     *   去重缓存是调用方（模型就绪回调 / 场景层）的职责，本类不持有任何 CPU 侧表；
     * - 注册后的行数据终身驻留 VRAM，直到显式 `UnregisterModelTemplate`。
     *
     * 线程约定：与 registry 其余写口一致 —— 仅主线程（模型流送提交路径）调用。
     */
    class ModelTemplateManager
    {
    public:

        /**
         * @brief 把一个已解析的模型内容打包注册进模板池。
         *
         * 部件 = (节点 × primitive) 的每个可绘制叶子；行序 = 节点下标升序、
         * 节点内 primitiveIndices 升序（展开侧按同一口径消费）。
         * 部件 `local_matrix` 直接取 `StaticMeshNode::worldMatrix`
         * （加载器已用 `ComposeNodeMatrices` 沿父链复合到模型根空间）。
         * 几何未就绪的图元仍会占一行，`geometry_id = 0` ⇒ GPU 展开跳过
         * （降级为「少画一个部件」，与哨兵哲学一致，不让物体整体消失）。
         *
         * @return 模板池行号（≠0）；失败返回 0
         */
        static uint32_t RegisterModelTemplate(GlobalSSBOBufferRegistry *registry,
                                              const ModelAssetContent &content);

        /**
         * @brief 注销一个模板：还回模板行与其部件区间。
         * @note 行号读回越界 / 行内容为空壳（part_count==0）都返回 false 且不做任何归还。
         */
        static bool UnregisterModelTemplate(GlobalSSBOBufferRegistry *registry,
                                            uint32_t template_id);
    };
}
