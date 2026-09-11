#pragma once

#include<hgl/common/RenderOptions.h>
#include<cstdint>

namespace hgl::graph
{
    /**
     * RingLayout — 静态段 + 动态段×帧数 的**环形行布局算术**
     *
     * L2 侧的工具类（不实现 IGPUBuffer）：只回答"这一帧的动态段落在哪一行"与
     * "整块需要多少行"。缓冲本体与其写入由调用方持有并经
     * DeviceBuffer::GetGPUBuffer() 完成——本类不持有 buffer，也不做 Map/Write。
     */
    class RingLayout
    {
        uint32_t ring_frames;
        uint32_t frame_index;

    public:
        explicit RingLayout(const uint32_t frames = HGL_L2W_RING_FRAMES)
            : ring_frames(frames ? frames : 1)
            , frame_index(0)
        {
        }

        /// 设置当前帧号（自动对 ring_frames 取模）
        void SetFrameIndex(const uint32_t index)
        {
            frame_index = ring_frames ? (index % ring_frames) : 0;
        }

        uint32_t GetFrameIndex() const
        {
            return frame_index;
        }

        /// 本帧动态段起始行号 = 静态段行数 + frame_index × 动态段行数
        uint32_t GetBaseIndex(const uint32_t static_count, const uint32_t dynamic_count) const
        {
            return static_count + frame_index * dynamic_count;
        }

        /// 容纳静态段 + ring_frames 份动态段所需的整块行数
        uint32_t GetTotalCount(const uint32_t static_count, const uint32_t dynamic_count) const
        {
            return static_count + dynamic_count * ring_frames;
        }
    };
}
