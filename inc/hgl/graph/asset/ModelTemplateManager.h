#pragma once

#include <hgl/CoreType.h>
#include <hgl/graph/ssbo/ModelTemplateRows.h>
#include <cstdint>

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
         * @brief 模板代理配置（LOD 级 1..3 + 阴影代理）—— **T4.1 / T4.2 的唯一写入口径**。
         *
         * 全 0 / 全 0.0f 的默认值 = 「无代理」⇒ 写回后模板行为与改造前完全一致。
         * 数组下标 `i` 对应**级号 i+1**（级 0 是本体，不占槽）。
         */
        struct ProxyConfig
        {
            uint32_t lod_proxy_template_ids[ssbo::kModelLodProxyCount] = { 0u, 0u, 0u };
            float    lod_screen_ratios[ssbo::kModelLodProxyCount]      = { 0.0f, 0.0f, 0.0f };
            uint32_t shadow_proxy_template_id = 0u;
        };

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

        /**
         * @brief 回填一个模板的 LOD / 阴影代理（T4.1 + T4.2）。
         *
         * **全有或全无**：任一校验失败 ⇒ 返回 false 且模板行**一个字节都不改**。
         *
         * 校验口径（与 `ModelTemplateRows.h` 的命名/阈值标准一一对应）：
         * - `template_id` 必须指向有效模板（`part_count/part_offset` 非 0）；
         * - 每个非 0 的 LOD 代理 id 必须指向有效模板，且 **≠ 本体、≠ 其它代理**（禁自引）；
         * - 非 0 代理必须配 **> 0** 的阈值，且阈值序列**单调不增**；`id == 0` ⇒ 阈值强制 0；
         * - 阴影代理 id 必须指向有效模板且 ≠ 本体（阴影代理可以等于某个 LOD 代理）；
         * - **禁套娃**：代理模板自己若已声明代理，不影响本次写入，但展开侧只跟随一跳
         *   （代理行的 `lod_*` / `shadow_proxy_template_id` 不被消费）⇒ 这里不做递归校验。
         *
         * 成功时同步维护 `flags`：有任一 LOD 代理 ⇒ 置 `kModelTemplateFlagLodEnabled`；
         * 有阴影代理 ⇒ 置 `kModelTemplateFlagShadowProxy`；两者皆无 ⇒ 两位清零（幂等）。
         *
         * @return true = 已写入（行级 dirty 已标），false = 校验失败（未写入）
         */
        static bool SetModelTemplateProxies(GlobalSSBOBufferRegistry *registry,
                                            uint32_t template_id,
                                            const ProxyConfig &cfg);
    };
}
