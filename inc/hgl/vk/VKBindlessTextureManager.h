#pragma once

#include <vulkan/vulkan.h>
#include <hgl/type/String.h>
#include <hgl/type/UnorderedMap.h>
#include <hgl/type/ValueArray.h>
#include <hgl/log/Log.h>

namespace hgl::graph
{
    class Texture;
    class Sampler;
    class VulkanDevice;
    struct VulkanDevAttr;

    /**
     * 全局 Bindless 纹理管理器（对应 Descriptor Set 1）。
     *
     * 纹理与 sampler 彻底分离：
     *   binding=0 : texture2DArray[]（SAMPLED_IMAGE，非均匀索引）
     *   binding=1 : sampler[]        （SAMPLER，统一预设，按索引引用）
     *   binding=2 : textureCubeArray[]（SAMPLED_IMAGE，Cubemap 纹理统一注册为
     *              单张=6 层的 CUBE_ARRAY companion view；与 binding=0 共享
     *              1-based handle 空间，按纹理类型分流）
     *
     * 基于 VK_EXT_descriptor_buffer：通过 Host-visible GPU 内存
     * 直接由 CPU 写入描述符，零 Pool 开销与驱动验证消耗。
     *
     * RegisterTexture 返回纯 tex_handle（1-based，0=无效）；
     * RegisterSamplers 按 ShaderLibrary/sampler.toml 的顺序一次性创建全部 sampler，
     * GLSL 侧以编译期 "#define <name>Sampler <idx>u" 引用，SSBO 只存纯 tex_handle。
     */
    class BindlessTextureManager
    {
        OBJECT_LOGGER

    public:
        // 一次最多支持的纹理数量（可按需调大，受 maxDescriptorSetSampledImages 约束）
        static constexpr uint32_t kMax = 8192;

        // 采样器池上限（可按需调大，受 maxDescriptorSetSamplers 约束）
        static constexpr uint32_t kMaxSampler = 64;

    private:
        VkDevice       device_ = VK_NULL_HANDLE;
        VulkanDevAttr *attr_   = nullptr;
        bool           use_descriptor_buffer_ = false;

        // ── Descriptor Pool 资源（传统回退路径） ──
        VkDescriptorPool pool_ = VK_NULL_HANDLE;
        VkDescriptorSet  set_  = VK_NULL_HANDLE;

        // ── Descriptor Buffer 资源 ──
        VkBuffer        desc_buffer_             = VK_NULL_HANDLE;
        VkDeviceMemory  desc_memory_             = VK_NULL_HANDLE;
        VkDeviceAddress desc_buffer_address_     = 0;
        uint8_t *       mapped_ptr_              = nullptr;
        VkDeviceSize    layout_size_             = 0;
        VkDeviceSize    binding_offset_0_        = 0; // texture2DArray[]
        VkDeviceSize    binding_offset_1_        = 0; // sampler[]
        VkDeviceSize    binding_offset_2_        = 0; // textureCubeArray[]
        size_t          sampled_image_desc_size_ = 0;
        size_t          sampler_desc_size_       = 0;

        // 描述符集布局
        VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;

        // 1-based 纹理 handle 池；0=无效
        uint32_t next_handle_ = 1;

        // 纹理 → tex_handle（1-based）去重映射
        hgl::UnorderedMap<const Texture *, uint32_t> tex_cache_;

        // 统一注册机制：由 RegisterSamplers 创建的 VkSampler 句柄，index = 预设索引。
        hgl::ValueArray<VkSampler> samplers_;

    private:
        bool InitDescriptorBuffer();
        bool InitDescriptorPool();

    public:
        BindlessTextureManager() = default;
        ~BindlessTextureManager() { Destroy(); }

        /**
         * 创建描述符布局与缓冲区。
         * 基于 VK_EXT_descriptor_buffer。
         */
        bool Init(VulkanDevice *device);
        bool Init(VkDevice device, VulkanDevAttr *attr = nullptr);

        /** 释放所有 Vulkan 资源 */
        void Destroy();

        bool IsValid() const
        {
            if (use_descriptor_buffer_)
                return desc_buffer_ != VK_NULL_HANDLE && mapped_ptr_ != nullptr;
            else
                return set_ != VK_NULL_HANDLE;
        }

        bool IsDescriptorBufferMode() const { return use_descriptor_buffer_; }
        VkDescriptorSet GetDescriptorSet() const { return set_; }

        VkDescriptorSetLayout GetLayout() const { return layout_; }

        VkBuffer GetDescriptorBuffer() const { return desc_buffer_; }
        VkDeviceAddress GetDescriptorBufferAddress() const { return desc_buffer_address_; }

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
         * 预分配一个 1-based 纹理 handle（可选关联一个占位纹理写入描述符）。
         * @param placeholder_tex 占位纹理（可为 nullptr）
         * @return 1-based handle
         */
        uint32_t AllocateHandle(Texture *placeholder_tex = nullptr);

        /**
         * 动态更新指定 handle 槽位的描述符内容（原子写入 Host-visible Descriptor Buffer）。
         * 用于异步纹理就绪后无缝替换占位符。
         * @param tex_handle 目标 handle
         * @param tex 目标纹理
         * @return 是否更新成功
         */
        bool UpdateTextureHandle(uint32_t tex_handle, Texture *tex);

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
         * 仅设置描述符缓冲区偏移（在描述符缓冲区已统一通过 vkCmdBindDescriptorBuffersEXT 绑定后使用）。
         * @param cmd            目标命令缓冲
         * @param pipeline_layout 当前管线布局
         * @param set_index      绑定到第几个 descriptor set（通常 = 1）
         * @param buffer_index   对应 vkCmdBindDescriptorBuffersEXT 中该缓冲区所在的 binding index
         * @param bind_point     绑定点：图形管线用 GRAPHICS（默认），ComputeCmdBuffer 用 COMPUTE
         */
        void BindOffsetToCmd(VkCommandBuffer cmd,
                             VkPipelineLayout pipeline_layout,
                             uint32_t set_index,
                             uint32_t buffer_index = 0,
                             VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS) const;

        /**
         * 绑定到命令缓冲区（独立绑定描述符缓冲区并设置偏移）。
         * @param cmd            目标命令缓冲
         * @param pipeline_layout 当前管线布局
         * @param set_index      绑定到第几个 descriptor set（通常 = 1）
         * @param bind_point     绑定点：图形管线用 GRAPHICS（默认），ComputeCmdBuffer 用 COMPUTE
         */
        void BindToCmd(VkCommandBuffer cmd, VkPipelineLayout pipeline_layout, uint32_t set_index,
                       VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS) const;
    };

}//namespace hgl::graph
