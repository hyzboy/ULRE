#pragma once

#include <vulkan/vulkan.h>

#include <hgl/common/DescriptorSetTypeDef.h>
#include <hgl/type/String.h>

namespace hgl::graph
{
    class IGPUBuffer;

    /**
     * 全局 Scene UBO 描述符集（对应 Descriptor Set 0）。
     *
     * 所有材质共用同一份 viewport / camera / sky UBO，
     * 一帧写一次、绑一次，不再走 per-material 描述符分配。
     *
     * 硬编码 binding（见 kSceneBinding* 常量）：
     *   binding=0 : camera   (kSceneBindingCamera)
     *   binding=1 : sky      (kSceneBindingSky)
     *   binding=2 : viewport (kSceneBindingViewport)
     *
     * 注：与 BindlessTextureManager 一样属于设备级全局资源，
     *     由 GraphicsContext 持有并管理生命周期。
     */
    class GlobalSceneUBOSet
    {
    private:
        VkDevice device_ = VK_NULL_HANDLE;

        VkDescriptorPool pool_        = VK_NULL_HANDLE;
        VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
        VkDescriptorSet  set_         = VK_NULL_HANDLE;

        // 已绑定的 buffer（避免每帧重复 vkUpdateDescriptorSets）
        VkBuffer bound_buffers_[3]{};

    public:
        GlobalSceneUBOSet() = default;
        ~GlobalSceneUBOSet() { Destroy(); }

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
         * 将指定 binding 的 UBO 写入描述符集。
         * 仅当 buffer 变化时才触发 vkUpdateDescriptorSets。
         * @param binding kSceneBindingCamera / kSceneBindingSky / kSceneBindingViewport
         * @param gpu     对应 UBO 的 GPU buffer（nullptr 时不更新）
         */
        bool UpdateUBO(uint32_t binding, const IGPUBuffer *gpu);

        /**
         * 绑定到命令缓冲区（Set 0）。
         * @param cmd             目标命令缓冲
         * @param pipeline_layout 当前管线布局（其 set 0 必须与本集 layout 一致）
         */
        void BindToCmd(VkCommandBuffer cmd, VkPipelineLayout pipeline_layout) const;
    };

}//namespace hgl::graph
