#pragma once

#include<hgl/common/RenderOptions.h>
#include<hgl/log/Log.h>
#include<cstdint>

namespace hgl::graph{

/**
 * OffscreenSlotBands —— 离屏 RT 的 per-frame 数据槽带分配器（纯算术，无设备依赖）
 *
 * ## 为什么存在
 *
 * 离屏 RT 的每帧 CPU 数据（L2W ring / CameraInfo 行 / Viewport / 阴影 UBO）写在数据槽带
 * `[data_slot_base, data_slot_base + slot_count)`，与交换链主帧槽 `[0, HGL_FRAME_SLOT_MAIN)`
 * 不相交（槽划分见 RenderOptions.h）。历史实现是一条**只增不还**的游标
 * （`next_offscreen_slot_base += slots`，`Destroy`/`Release` 均不归还）⇒
 * 离屏带总共只有 `HGL_FRAME_SLOT_TOTAL - HGL_FRAME_SLOT_MAIN` 个（当前 = 4），而
 * **CSM 一条级联占一带**（`EnvironmentSystem::EnableMainLightShadow`）⇒ 一次 Enable
 * 即耗尽 ⇒ 同会话第二次 Enable 必报「in-flight 槽带越界」（实测遗留缺陷 C3①的根因）；
 * `OffscreenWorld + CSM` 同应用 = 第 5 个离屏 RT 必 fail-fast（合理组合，非极端场景）。
 *
 * 本类把带记账换成**占用位图 + first-fit 分配 + 显式归还**：`RenderTargetManager::Destroy`
 * 归还带 ⇒ 槽带随 RT 生命周期回收，Enable/Disable 循环与多离屏 RT 共存不再泄漏。
 *
 * ## 约定（与引擎资源纪律同源）
 *
 * - **容量冻结、超限 fail-fast**：带空间 = `[HGL_FRAME_SLOT_MAIN, HGL_FRAME_SLOT_TOTAL)`，
 *   分配失败返回 `kInvalidBase`（调用方响亮报错，不扩容、不静默截断）；
 * - **纯算术、无设备/无堆**：一个 `uint64_t` 位图（bit i = 全局槽 `kBandBegin + i`），
 *   可单测（TestCSMIncrementalPass Test 43 直接实例化本类跑生命周期账）；
 * - **归还必须与分配成对**：`ReleaseBand` 对未占用/越界的带**报错并忽略**（双重归还 =
 *   调用方生命周期 bug，fail-fast 好过静默把别人的带标成空闲）。
 */
class OffscreenSlotBands
{
public:
    /// 带空间（全局槽号）：[kBandBegin, kBandEnd)
    static constexpr uint32_t kBandBegin = HGL_FRAME_SLOT_MAIN;
    static constexpr uint32_t kBandEnd   = HGL_FRAME_SLOT_TOTAL;
    static constexpr uint32_t kBandCount = kBandEnd - kBandBegin;

    /// 分配失败返回值（带空间耗尽 / slots 非法）
    static constexpr uint32_t kInvalidBase = UINT32_MAX;

    static_assert(kBandCount > 0, "离屏槽带空间为空：HGL_FRAME_SLOT_TOTAL 必须 > HGL_FRAME_SLOT_MAIN");
    static_assert(kBandCount <= 64, "占用位图按 uint64_t 实现：带数超限请换更宽的位图");

private:
    uint64_t occupied = 0;    ///< bit i = 全局槽 kBandBegin+i 已被某带占用

    /// 全局槽号 → 位图位号（越界返回 false）
    static bool ToBit(const uint32_t global_slot, uint32_t &out_bit)
    {
        if(global_slot < kBandBegin || global_slot >= kBandEnd)
            return(false);

        out_bit = global_slot - kBandBegin;
        return(true);
    }

public:
    /// 分配一段连续 `slots` 个槽的带（first-fit）
    /// @return 带起点（全局槽号，>= kBandBegin）；空间不足或 slots 非法返回 kInvalidBase
    uint32_t AcquireBand(const uint32_t slots)
    {
        if(slots == 0 || slots > kBandCount)
            return(kInvalidBase);

        for(uint32_t start = 0; start + slots <= kBandCount; ++start)
        {
            bool free_run = true;

            for(uint32_t i = 0; i < slots; ++i)
            {
                if(occupied & (uint64_t(1) << (start + i)))
                {
                    free_run = false;
                    break;
                }
            }

            if(!free_run)
                continue;

            for(uint32_t i = 0; i < slots; ++i)
                occupied |= uint64_t(1) << (start + i);

            return(kBandBegin + start);
        }

        return(kInvalidBase);
    }

    /// 归还 `AcquireBand` 分出的带（与分配成对；调用点 = RenderTargetManager::Destroy）
    /// @return 成功 true；带未完整占用（双重归还/越界/槽数不符）报错并返回 false
    bool ReleaseBand(const uint32_t base, const uint32_t slots)
    {
        if(slots == 0 || slots > kBandCount)
        {
            GLogError("[OffscreenSlotBands] ReleaseBand 非法槽数：base=%u slots=%u（带空间=%u）",
                      base, slots, kBandCount);
            return(false);
        }

        uint32_t start_bit = 0;

        if(!ToBit(base, start_bit) || start_bit + slots > kBandCount)
        {
            GLogError("[OffscreenSlotBands] ReleaseBand 越界：base=%u slots=%u（带=[%u,%u)）",
                      base, slots, kBandBegin, kBandEnd);
            return(false);
        }

        for(uint32_t i = 0; i < slots; ++i)
        {
            if(!(occupied & (uint64_t(1) << (start_bit + i))))
            {
                GLogError("[OffscreenSlotBands] ReleaseBand 双重归还：base=%u slots=%u 第 %u 槽本就空闲"
                          "（带归还必须与 AcquireBand 成对——调用方生命周期 bug）",
                          base, slots, i);
                return(false);
            }
        }

        for(uint32_t i = 0; i < slots; ++i)
            occupied &= ~(uint64_t(1) << (start_bit + i));

        return(true);
    }

    /// 已占用槽数（泄漏追踪 / 契约测试用）
    uint32_t GetOccupiedCount() const
    {
        uint32_t n = 0;

        for(uint32_t i = 0; i < kBandCount; ++i)
            if(occupied & (uint64_t(1) << i))
                ++n;

        return(n);
    }

    /// 空闲槽数
    uint32_t GetFreeCount() const { return kBandCount - GetOccupiedCount(); }

    /// 全部归还（仅测试/自检用；生产路径的归还一律走 ReleaseBand 成对记账）
    void Reset() { occupied = 0; }
};//class OffscreenSlotBands

}//namespace hgl::graph
