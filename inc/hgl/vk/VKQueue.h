#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/VKFence.h>
#include<hgl/time/TimeConst.h>
#include<hgl/log/Log.h>

namespace hgl::graph{
struct VulkanDevAttr;

/**
 * 一次提交的等待/信号项。
 *
 * - 二进制信号量：`value` 必须为 0（WSI 的 acquire/present、上传/回读路径）
 * - timeline 信号量：`value` 表达时点，允许跨帧、多等待方、重复信号（A1/A7 的车道排序）
 * - `stage_mask`：等待项的等待阶段 / 信号项的可见阶段
 */
struct SemaphoreSubmit
{
    Semaphore *             semaphore  = nullptr;
    uint64_t                value      = 0;
    VkPipelineStageFlags2   stage_mask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    /// 等待项：默认等待「片元/计算」阶段（上传/回读等数据依赖）
    static SemaphoreSubmit Wait(Semaphore *s,const uint64_t v=0,
                                const VkPipelineStageFlags2 m=VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
    {
        return SemaphoreSubmit{s,v,m};
    }

    /// 等待项：交换链图像可用（COLOR_ATTACHMENT_OUTPUT）
    static SemaphoreSubmit WaitImage(Semaphore *s)
    {
        return SemaphoreSubmit{s,0,VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT};
    }

    /// 信号项：默认全部命令完成后可见
    static SemaphoreSubmit Signal(Semaphore *s,const uint64_t v=0,
                                  const VkPipelineStageFlags2 m=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT)
    {
        return SemaphoreSubmit{s,v,m};
    }

    const bool IsValid()const{return semaphore!=nullptr;}
};

class DeviceQueue
{
    OBJECT_LOGGER

protected:

    const VulkanDevAttr *dev_attr;
    VkDevice device;
    VkQueue queue;

    uint32_t current_fence;
    uint32_t last_submitted_fence;
    bool has_last_submit;
    Fence **fence_list;
    uint32_t fence_count;

private:

    friend class VulkanDevice;

    DeviceQueue(const VulkanDevAttr *attr,VkQueue q,Fence **,const uint32_t fc);

public:

    virtual ~DeviceQueue();

    operator VkQueue(){return queue;}

    VkResult Present(const VkPresentInfoKHR *pi){return vkQueuePresentKHR(queue,pi);}

    /**
    * 等待Submit的队列完成操作。这个操作会阻塞当前线程，所以在Submit后请不要立即使用它。而是在下一次队列提交前再做这个操作。
    */
    bool WaitQueue();

    /**
    * 等待Queue命令执行完成的fence信号
    */
    bool WaitFence(const bool wait_all=true,const uint64_t time_out=HGL_NANO_SEC_PER_SEC);

    /**
    * 等待上一条提交使用的fence信号
    */
    bool WaitLastSubmitFence(const bool wait_all=true,const uint64_t time_out=HGL_NANO_SEC_PER_SEC);

    /**
    * 检查上一条提交是否已完成（非阻塞）
    */
    bool IsLastSubmitComplete() const;

    /// 提交（等待/信号全部列表化）。
    ///
    /// - 二进制信号量 `value` 必须为 0；timeline 信号量用 `value` 表达时点
    /// - 本队列的 fence 环照常轮转：一次提交占用一格，可用 WaitLastSubmitFence 等它
    bool Submit(const VkCommandBuffer *cmd_buf,uint32_t cb_count,
                const SemaphoreSubmit *waits,uint32_t wait_count,
                const SemaphoreSubmit *signals,uint32_t signal_count);

    bool Submit(VulkanCmdBuffer *cmd_buf,
                const SemaphoreSubmit *waits,uint32_t wait_count,
                const SemaphoreSubmit *signals,uint32_t signal_count);
};//class DeviceQueue
}//namespace hgl::graph
