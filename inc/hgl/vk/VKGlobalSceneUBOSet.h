#pragma once

#include <vulkan/vulkan.h>

#include <hgl/common/DescriptorSetTypeDef.h>
#include <hgl/type/String.h>

namespace hgl::graph
{
    class IGPUBuffer;
    struct VulkanDevAttr;

    /**
     * 全局 Scene UBO 描述符集（对应 Descriptor Set 0）。
     *
     * 所有材质共用同一份 viewport / camera / sky / color_palette UBO，
     * 一帧写一次、绑一次，不再走 per-material 描述符分配。
     *
     * 硬编码 binding（见 kSceneBinding* 常量）：
     *   binding=0 : camera           (kSceneBindingCamera)
     *   binding=1 : sky              (kSceneBindingSky)
     *   binding=2 : viewport         (kSceneBindingViewport)
     *   binding=3 : color_palette    (kSceneBindingColorPalette)
     *   binding=4 : global_addresses (kSceneBindingGlobalAddresses)
     *   binding=5 : shadow           (kSceneBindingShadow)
     *
     * 注：与 BindlessTextureManager 一样属于设备级全局资源，
     *     由 GraphicsContext 持有并管理生命周期。
     */
    class GlobalSceneUBOSet
    {
    private:
        VkDevice device_ = VK_NULL_HANDLE;
        VulkanDevAttr *attr_ = nullptr;
        bool use_descriptor_buffer_ = false;

        // ── 传统 DescriptorPool 资源（回退路径） ──
        VkDescriptorPool pool_ = VK_NULL_HANDLE;
        VkDescriptorSet  set_  = VK_NULL_HANDLE;

        VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
        PFN_vkCmdPushDescriptorSet push_fn_ = nullptr;

        // Push Descriptor Buffer (for hardware where bufferlessPushDescriptors == false)
        VkBuffer        push_desc_buffer_  = VK_NULL_HANDLE;
        VkDeviceMemory  push_desc_memory_  = VK_NULL_HANDLE;
        VkDeviceAddress push_desc_address_ = 0;
        bool            needs_push_buffer_ = false;

        // 已配置的 buffer 信息（供 vkCmdPushDescriptorSet 组装）
        mutable VkDescriptorBufferInfo bound_buffers_info_[size_t(SceneBinding::RANGE_SIZE)]{};
        mutable bool binding_valid_[size_t(SceneBinding::RANGE_SIZE)]{};

    private:
        bool InitDescriptorBuffer();
        bool InitDescriptorPool();

    public:
        GlobalSceneUBOSet() = default;
        ~GlobalSceneUBOSet() { Destroy(); }

        /**
         * 创建描述符集布局与缓冲区/描述符池。
         * 必须在 VkDevice 创建完毕后调用一次。
         */
        bool Init(VkDevice device);

        /** 释放所有 Vulkan 资源 */
        void Destroy();

        bool IsValid() const
        {
            if (use_descriptor_buffer_)
                return layout_ != VK_NULL_HANDLE && push_fn_ != nullptr;
            else
                return layout_ != VK_NULL_HANDLE && set_ != VK_NULL_HANDLE;
        }

        bool IsDescriptorBufferMode() const { return use_descriptor_buffer_; }

        VkDescriptorSetLayout GetLayout() const { return layout_; }
        VkDescriptorSet       GetSet()    const { return set_; }

        bool NeedsPushDescriptorBuffer() const { return use_descriptor_buffer_ && needs_push_buffer_; }
        VkBuffer GetPushDescriptorBuffer() const { return push_desc_buffer_; }
        VkDeviceAddress GetPushDescriptorBufferAddress() const { return push_desc_address_; }

        /**
         * 将指定 binding 的 UBO 记录到推送缓存。
         * @param binding kSceneBindingCamera / kSceneBindingSky / kSceneBindingViewport / kSceneBindingColorPalette 等
         * @param gpu     对应 UBO 的 GPU buffer（nullptr 时禁用该 binding）
         */
        bool UpdateUBO(uint32_t binding, const IGPUBuffer *gpu);

        /**
         * 通过 Push Descriptor 直接向命令缓冲区推送 Set 0。
         * @param cmd             目标命令缓冲
         * @param pipeline_layout 当前管线布局（其 set 0 必须与本集 layout 一致）
         * @param bind_point      绑定点：图形管线用 GRAPHICS（默认），ComputeCmdBuffer 用 COMPUTE
         */
        void BindToCmd(VkCommandBuffer cmd, VkPipelineLayout pipeline_layout,
                       VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS) const;
    };

}//namespace hgl::graph
