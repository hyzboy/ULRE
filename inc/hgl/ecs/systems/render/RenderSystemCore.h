#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/log/log.h>
#include<hgl/color/Color4f.h>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace hgl {
    namespace graph {
        class IRenderTarget;
        class VulkanDevice;
        class RenderCmdBuffer;
    }
}

namespace hgl::ecs {

class ECSContext;

/**
 * 渲染系统核心 - 替代旧的集中式入口
 * 
 * 职责：
 * - 管理 Vulkan 设备和队列
 * - 协调所有渲染系统的执行
 * - 管理 Swapchain 和帧同步（Fence、Semaphore）
 * - 处理命令缓冲区的创建、记录和提交
 * 
 * 使用流程示例：
 * @code
 *   auto world = std::make_shared<ECSContext>("main_world");
 *   world->InitializeGraphics(gpu_device, render_target);
 *   
 *   auto core = std::make_unique<RenderSystemCore>(world.get());
 *   core->Initialize();
 *   
 *   // 主循环
 *   while (running) {
 *       if (!core->BeginFrame()) continue;     // 获取 swapchain 图像
 *       world->Tick(dt);                        // 更新逻辑
 *       world->Render(core->GetRenderCmd(), dt); // 执行渲染命令
 *       core->EndFrame();                       // 提交命令并 Present
 *   }
 * @endcode
 */
class RenderSystemCore
{
    OBJECT_LOGGER

public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
    
private:
    ECSContext* world;
    hgl::graph::VulkanDevice* gpu_device;
    hgl::graph::IRenderTarget* render_target;
    
    // Vulkan 同步原语
    std::vector<VkFence> frame_fences;
    std::vector<VkSemaphore> image_available_semaphores;
    std::vector<VkSemaphore> render_finished_semaphores;
    
    // 当前帧状态
    uint32_t current_frame = 0;
    uint32_t swapchain_image_index = 0;
    bool frame_begun = false;

    hgl::Color4f clear_color{0,0,0,1};
    
    // 渲染命令缓冲区
    hgl::graph::RenderCmdBuffer* render_cmd = nullptr;
    
public:
    /**
     * 构造函数
     * @param ctx ECS 世界上下文
     */
    explicit RenderSystemCore(ECSContext* ctx);
    
    ~RenderSystemCore();
    
    /**
     * 初始化渲染系统核心
     * 
     * 可选操作（RenderSystemCore 会自动从 ECSContext 获取）：
     * - GPU 设备
     * - 渲染目标
     * - 创建同步原语
     * 
     * @return 成功返回 true，失败返回 false
     */
    bool Initialize();
    
    /**
     * 开始一帧的渲染
     * 
     * 流程：
     * 1. 等待上一帧的 Fence 完成
     * 2. 获取 Swapchain 中下一个可用图像
     * 3. 重置 Fence
     * 4. 分配并开始记录命令缓冲区
     * 
     * @return 成功返回 true
     *         失败返回 false（如 Swapchain 过期，需要重新创建）
     * 
     * 示例：
     * @code
     *   if (!core->BeginFrame()) {
     *       // Swapchain 过期，重创建后重试
     *       core->Initialize();
     *       continue;
     *   }
     * @endcode
     */
    bool BeginFrame();
    
    /**
     * 结束一帧的渲染
     * 
     * 流程：
     * 1. 停止记录命令缓冲区
     * 2. 提交命令缓冲区到 Graphics Queue
     * 3. 提交 Present 命令到 Present Queue
     * 4. 提交信号量用于下一帧同步
     * 
     * 调用此方法后，render_cmd 变为无效
     */
    void EndFrame();
    
    /**
     * 获取当前渲染命令缓冲区
     * 
     * 仅在 BeginFrame() 和 EndFrame() 之间有效
     * 
     * @return 命令缓冲区指针
     *         如果未在 BeginFrame/EndFrame 间调用，返回 nullptr
     * 
     * 示例：
     * @code
     *   auto cmd = core->GetRenderCmd();
     *   if (cmd) {
     *       world->Render(cmd, dt);
     *   }
     * @endcode
     */
    hgl::graph::RenderCmdBuffer* GetRenderCmd() {
        return frame_begun ? render_cmd : nullptr;
    }

    void SetClearColor(const hgl::Color4f &color) { clear_color = color; }
    const hgl::Color4f &GetClearColor() const { return clear_color; }
    
    /**
     * 获取当前的 Swapchain 图像索引
     * 
     * 仅在 BeginFrame() 之后有效
     */
    uint32_t GetSwapchainImageIndex() const { return swapchain_image_index; }
    
    /**
     * 获取 Vulkan 设备
     */
    hgl::graph::VulkanDevice* GetGPUDevice() { return gpu_device; }
    
    /**
     * 获取当前帧号（从 0 开始）
     * 
     * 每次 EndFrame() 后递增
     */
    uint32_t GetCurrentFrameIndex() const { return current_frame; }
    
    /**
     * 获取 Frames In Flight 数量（通常为 3）
     */
    uint32_t GetMaxFramesInFlight() const { return MAX_FRAMES_IN_FLIGHT; }
    
    /**
     * 检查是否在 BeginFrame/EndFrame 间
     */
    bool IsFrameInProgress() const { return frame_begun; }
};

} // namespace hgl::ecs
