#pragma once

#include <vulkan/vulkan.h>
#include <hgl/type/String.h>
#include <hgl/type/UnorderedMap.h>

namespace hgl::graph
{
    class Texture;
    class Sampler;

    /**
     * 全局 Bindless 纹理管理器（对应 Descriptor Set 3）。
     *
     * 维护单一纹理数组：
     *   binding=0 : sampler2DArray[]（2D 纹理注册为单层 2D_ARRAY view）
     *
     * 每个纹理注册后返回一个 uint32_t handle（1-based，0 保留为无效）。
     * handle 直接对应 GLSL 中 bindless_tex2darray[handle-1] 的数组下标。
     *
     * 注：描述符集使用 UPDATE_AFTER_BIND + PARTIALLY_BOUND，
     *     可在帧内随时注册新纹理。
     */
    class BindlessTextureManager
    {
    public:
        // 一次最多支持的纹理数量（可按需调大，受 maxDescriptorSetSampledImages 约束）
        static constexpr uint32_t kMax = 8192;

    private:
        VkDevice device_ = VK_NULL_HANDLE;

        VkDescriptorPool pool_        = VK_NULL_HANDLE;
        VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
        VkDescriptorSet  set_         = VK_NULL_HANDLE;

        // 1-based handle pool；0=无效
        uint32_t next_handle_ = 1;

        // 防止同一 (texture, sampler) 对重复分配
        hgl::UnorderedMap<uint64_t, uint32_t> handle_cache_;

        static uint64_t MakeCacheKey(const void *tex, const void *sampler) noexcept
        {
            return (reinterpret_cast<uintptr_t>(tex) * 2654435761ULL)
                 ^ (reinterpret_cast<uintptr_t>(sampler) * 40503ULL);
        }

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

        /**
         * 注册一张纹理（2D 或 2DArray 均可），返回 handle（1-based）。
         * 相同 (tex, sampler) 对会直接返回已有 handle，不重复占用槽位。
         */
        uint32_t Register(Texture *tex, Sampler *sampler);

        /**
         * 绑定到命令缓冲区。
         * @param cmd            目标命令缓冲
         * @param pipeline_layout 当前管线布局
         * @param set_index      绑定到第几个 descriptor set（通常 = 4）
         */
        void BindToCmd(VkCommandBuffer cmd, VkPipelineLayout pipeline_layout, uint32_t set_index) const;
    };

}//namespace hgl::graph
