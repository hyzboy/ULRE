#pragma once

#include <vulkan/vulkan.h>
#include <hgl/type/String.h>
#include <hgl/type/UnorderedMap.h>
#include <hgl/type/ValueArray.h>

namespace hgl::graph
{
    class Texture;
    class Sampler;

    /**
     * 全局 Bindless 纹理管理器（对应 Descriptor Set 3）。
     *
     * 纹理与 sampler 彻底分离：
     *   binding=0 : texture2DArray[]（SAMPLED_IMAGE，非均匀索引）
     *   binding=1 : sampler[]        （SAMPLER，统一预设，按索引引用）
     *
     * RegisterTexture 返回纯 tex_handle（1-based，0=无效）；
     * RegisterSamplers 按 ShaderLibrary/sampler.toml 的顺序一次性创建全部 sampler，
     * GLSL 侧以编译期 "#define <name>Sampler <idx>u" 引用，SSBO 只存纯 tex_handle。
     *
     * 注：描述符集使用 UPDATE_AFTER_BIND + PARTIALLY_BOUND，
     *     可在帧内随时注册新纹理（binding=0 支持 update-after-bind；
     *     binding=1 采样器池仅 PARTIALLY_BOUND，注册须发生在集合绑定前）。
     */
    class BindlessTextureManager
    {
    public:
        // 一次最多支持的纹理数量（可按需调大，受 maxDescriptorSetSampledImages 约束）
        static constexpr uint32_t kMax = 8192;

        // 采样器池上限（可按需调大，受 maxDescriptorSetSamplers 约束）
        static constexpr uint32_t kMaxSampler = 64;

    private:
        VkDevice device_ = VK_NULL_HANDLE;

        VkDescriptorPool pool_        = VK_NULL_HANDLE;
        VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
        VkDescriptorSet  set_         = VK_NULL_HANDLE;

        // 1-based 纹理 handle 池；0=无效
        uint32_t next_handle_ = 1;

        // 纹理 → tex_handle（1-based）去重映射
        hgl::UnorderedMap<const Texture *, uint32_t> tex_cache_;

        // 统一注册机制：由 RegisterSamplers 创建的 VkSampler 句柄，index = 预设索引。
        hgl::ValueArray<VkSampler> samplers_;

    public:
        BindlessTextureManager() = default;
        ~BindlessTextureManager() { Destroy(); }

        /**
         * 创建描述符池、布局、描述符集。
         * 必须在 VkDevice 创建完毕后调用一次。
         */
        bool Init(VkDevice device);

        /** 释放所有 Vulkan 资源 */
        void Destroy();

        bool IsValid() const { return set_ != VK_NULL_HANDLE; }

        VkDescriptorSetLayout GetLayout() const { return layout_; }
        VkDescriptorSet       GetSet()    const { return set_; }

        // ── 统一 Sampler 注册 ─────────────────────────────────────────────
        //
        // RegisterTexture 只写 binding=0 返回纯 tex_handle；RegisterSamplers 按
        // 预设数组顺序一次性创建并写入 binding=1；GLSL 侧以编译期
        // "#define <name>Sampler <idx>u" 引用，SSBO 只存纯 tex_handle。

        /**
         * 注册一张纹理，返回纯 tex_handle（1-based，0=无效）。
         */
        uint32_t RegisterTexture(Texture *tex);

        /**
         * 按预设顺序创建并注册所有 sampler（写 binding=1，index=数组顺序）。
         * 若之前已注册过 sampler，会先销毁旧句柄再重建。
         * 需在描述符集绑定前调用（binding=1 无 UPDATE_AFTER_BIND）。
         */
        bool RegisterSamplers(const VkSamplerCreateInfo *infos, uint32_t count);

        /**
         * 运行时重建指定索引的 sampler（如动态重建 TerrainSampler）。
         * 宏/索引不变，仅替换 binding=1 对应槽位的 VkSampler。
         */
        bool RebuildSampler(uint32_t index, const VkSamplerCreateInfo &info);

        /** 已注册的 sampler 数量。 */
        uint32_t GetSamplerCount() const { return static_cast<uint32_t>(samplers_.GetCount()); }

        /**
         * 绑定到命令缓冲区。
         * @param cmd            目标命令缓冲
         * @param pipeline_layout 当前管线布局
         * @param set_index      绑定到第几个 descriptor set（通常 = 4）
         */
        void BindToCmd(VkCommandBuffer cmd, VkPipelineLayout pipeline_layout, uint32_t set_index) const;
    };

}//namespace hgl::graph
