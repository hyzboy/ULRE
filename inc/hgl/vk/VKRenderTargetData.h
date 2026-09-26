#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/VKSemaphore.h>
#include<hgl/vk/VKQueue.h>
#include<hgl/common/RenderOptions.h>
#include<hgl/log/Log.h>

namespace hgl::graph{

/**
 * 离屏渲染目标的设备侧资源集合。
 *
 * ## in-flight 帧槽（A7）
 *
 * `slot_count` 组 {cmd_buf, queue(1 fence), render_complete_semaphore} 按提交次数轮转：
 * 一次提交占用一格，复用某格前等它自己的 fence（标准 WSI 模型，见
 * SwapchainRenderTarget 的 sync_slots）。slot_count==1 时行为与改造前一致
 * （每次提交前都等上一次提交完成，零重叠）。
 *
 * ## per-frame 数据槽
 *
 * 本 RT 的每帧 CPU 数据（L2W ring / CameraInfo 行 / Viewport / 阴影 UBO）写在数据槽
 * `[data_slot_base, data_slot_base+slot_count)`，与交换链主帧槽 `[0, HGL_FRAME_SLOT_MAIN)`
 * **不相交**（见 RenderOptions.h 的槽划分）。这是 A1/A7 的关键：改造前离屏 RT 的
 * `GetCurrentFrameIndex()` 恒为 0，prepass 的每帧写会落在在途主帧的槽 0 上，只能靠
 * RenderTo 开头的全槽排空兜住（RenderSystemCore::BeginFrame 的 T10 注释）。
 */
struct RenderTargetData
{
    OBJECT_LOGGER

    Framebuffer *       fbo            = nullptr;

    uint32_t            color_count    = 0;         ///<颜色成分数量
    Texture2D **        color_textures = nullptr;   ///<颜色成分纹理列表
    Texture2D *         depth_texture  = nullptr;   ///<深度成分纹理

    // ---- in-flight 帧槽 ----

    uint32_t            slot_count     = 0;         ///<帧槽数（>=1）
    uint32_t            slot_index     = 0;         ///<当前槽游标（每次提交前进一格）

    RenderCmdBuffer **  cmd_bufs                   = nullptr;   ///<[slot_count]
    DeviceQueue **      queues                     = nullptr;   ///<[slot_count]，各持 1 fence

    // ---- 车道（A1：双向跨帧排序）----

    /// timeline 信号量：本 RT 的车道。离屏提交在它上面 signal（每次提交前进一个值），
    /// 主帧提交 await 本帧提交过的值 ⇒ GPU 侧保证「主帧采样阴影前，prepass 已写完」。
    Semaphore *         lane                       = nullptr;
    uint64_t            lane_value                 = 0;         ///<最近一次已提交的信号值（0=未提交）

    // ---- per-frame 数据槽空间 ----

    uint32_t            data_slot_base   = 0;                       ///<本 RT 数据槽带起点
    uint32_t            frame_slot_total = HGL_FRAME_SLOT_TOTAL;    ///<全局数据槽总数

public:

    uint32_t GetSlotCount() const { return slot_count; }
    uint32_t GetSlotIndex() const { return slot_index; }

    /// 本帧使用的 per-frame 数据槽号（L2W ring / CameraInfo 行 / Viewport 槽按它取号）
    uint32_t GetCurrentFrameIndex() const { return data_slot_base + slot_index; }

    /// per-frame 数据槽总数（全局：主帧槽 + 离屏槽共用一段索引空间）
    uint32_t GetFrameCount() const { return frame_slot_total; }

    RenderCmdBuffer *GetCmdBuffer() const { return cmd_bufs ? cmd_bufs[slot_index] : nullptr; }
    DeviceQueue *    GetQueue()     const { return queues   ? queues[slot_index]   : nullptr; }

    /// 本 RT 的车道（timeline）（A1 双向排序：离屏 signal / 主帧 await）
    Semaphore *      GetLane()      const { return lane; }
    uint64_t         GetLaneValue() const { return lane_value; }

    /// 提交前取下一个车道信号值（signal 用），并记录为本 RT 最近提交值
    uint64_t         NextLaneValue() { lane_value = lane ? lane->NextValue() : 0; return lane_value; }

    /// 提交（等待列表由调用方给出：上传完成、主帧车道等）
    bool Submit(const SemaphoreSubmit *extra_waits,uint32_t extra_wait_count);

    Texture2D *GetColorTexture(const uint32_t index)
    {
        if(index>=color_count)
            return(nullptr);

        return color_textures[index];
    }

    RenderCmdBuffer *BeginRender();

    void EndRender();
    virtual void Clear();
};//struct RenderTargetData

}//namespace hgl::graph
