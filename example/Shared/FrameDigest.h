#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// FrameDigest —— 帧摘要自检（A6-d 判据重建的共用件；计划档 §10 §三.3）
//
// 为什么需要：删 CPU 账后「三方像素（CPU 账 / 仅 dispatch / 接管）」失效 ⇒ 用**帧摘要**接管
// 「画面没变」这条判据。固定帧**帧内**回读颜色靶 ⇒ 对**打包字节**算
// `{宽, 高, 格式, 不同字节取值数, FNV-1a 64}`，与示例内**内联常量**比对：
// PASS 退出 0 / FAIL 退出 1 / 读回失败退出 2。
//
// 约定（别踩）：
//   · 只建在**冻结相机 + 定帧**的台架上（否则摘要非确定 ⇒ 假红）；
//   · `distinct >= expected.distinct_byte_values` 兼作**判据不空转**守卫
//     （全清屏帧 distinct 极小 ⇒ FAIL，不会因为「读回拿到恒定图」而假绿）；
//   · 摘要对的是 `*_DUMP` 落盘的**同一份字节**（同一次 `ReadbackColorTarget`）⇒ 两者可比；
//   · **不入库大图**（与「dump 家族退库」口径一致）：常量内联在示例里，沿用 ATS 的风格；
//   · 改渲染结果（少画 / 错画 / 挪相机）必须同步复核并重录常量 —— 敏感性对照就是换个相机角再跑。
//   · **配置口径（2026-10-14 定）**：像素摘要的锚定常量是在 **Debug** 拍的；Release 因浮点/优化差异
//     像素不同（实测 `GPUDrivenCull` Release 冻结后两次都 `1F32397E53E54C57`，**稳定但 ≠ 锚定值**）
//     ⇒ **Debug 钉像素（`fnv1a64`）/ Release 只钉跨配置稳定的量**：尺寸 + 格式 + `distinct` 下限
//     （仍能抓住「整帧只剩清屏色」这类回归），并把实测摘要打出来供落盘比对。
//     （逐帧 `FRAMEDIGEST_PERFRAME` 模式本就只钉「帧间一致 + distinct 下限」⇒ 两配置通用。）
//     判据由 `kFrameDigestPinPixels` **一处**决定，且**两侧都编译**（台帐规矩 3：只读判断走 constexpr）。
// ─────────────────────────────────────────────────────────────────────────────

#include<hgl/vk/VKTextureReadback.h>
#include<hgl/vk/VKFormat.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/util/hash/FNV1a.h>

#include<cstdio>
#include<cstdlib>
#include<cstdint>
#include<vector>

namespace ulre_example
{
    struct FrameDigest
    {
        uint32_t    width                 = 0;
        uint32_t    height                = 0;
        size_t      bytes                 = 0;
        uint32_t    distinct_byte_values  = 0;      ///< 期望侧 = **下限**（不空转守卫）
        uint64_t    fnv1a64               = 0;
        const char *format_name           = "unknown";
    };

    /// 是否把**像素摘要**（`fnv1a64`）计入判据：**Debug 钉 / Release 不钉**。
    ///
    /// 为什么：锚定常量在 Debug 拍；Release 的浮点/优化差异会让像素不同（实测 `GPUDrivenCull`
    /// Release 冻结后仍稳定但 ≠ 锚定值）⇒ Release 钉像素 = 常红，与回归无关。
    /// Release 仍钉：**尺寸 / 格式 / `distinct_byte_values` 下限** + 打印实测摘要供比对。
    /// 口径出处：`doc/backlog.md` §3（自检门配置口径）+ 台帐 §2。
    ///
    /// 注：这里判的是**编译器自带**的 `NDEBUG`（不是台帐开关），故用 `#if defined(...)` 两分支取常量
    /// —— 常量两侧都在，判据处不需要预处理分支（不会 rot）。
    #if defined(NDEBUG)
    inline constexpr bool kFrameDigestPinPixels = false;
    #else
    inline constexpr bool kFrameDigestPinPixels = true;
    #endif

    /// 单帧摘要（对**打包字节**算；与 `*_DUMP` 落盘字节同源）
    inline bool ComputeFrameDigest(hgl::graph::IRenderTarget * rt, FrameDigest & out)
    {
        std::vector<uint8_t> pixels;
        hgl::graph::TextureReadbackInfo info;

        if (!hgl::graph::ReadbackColorTarget(rt, pixels, 0, &info) || pixels.empty())
            return false;

        uint8_t seen[256] = {};
        uint32_t distinct = 0;

        for (const uint8_t b : pixels)
        {
            if (!seen[b])
            {
                seen[b] = 1;
                ++distinct;
            }
        }

        hgl::hash::FNV1aHasher64 hasher;
        hasher.AppendBytes(pixels.data(), pixels.size());

        out.width                = info.width;
        out.height               = info.height;
        out.bytes                = pixels.size();
        out.distinct_byte_values = distinct;
        out.fnv1a64              = hasher;
        out.format_name          = hgl::graph::GetVulkanFormatName(info.format);
        return true;
    }

    /// 装**一次性帧内回读钩子**并按判据收尾（打印实测/期望两行 + 退出码）。
    /// `tag` = 打印前缀（如 "GCSELF"）；`expected` 里 `distinct_byte_values` 是**下限**。
    inline void InstallFrameDigestHook(hgl::graph::SwapchainRenderTarget * rt,
                                       const FrameDigest & expected,
                                       const char * tag)
    {
        rt->SetInFrameReadbackHook([rt, expected, tag]()
        {
            // ── `FRAMEDIGEST_PERFRAME=N`：**逐帧**模式 ─────────────────────────────
            // 每帧打一行摘要，累计 N 帧后收尾：**所有帧一致**（且满足 distinct 下限）⇒ PASS 退 0；
            // 出现不一致 ⇒ FAIL 退 1，并把差异帧数与首个差异帧打出来。
            // 为什么需要：一次性模式只读**一张图** ⇒ **看不见"每 N 帧闪一帧"**这类帧间不稳定
            // （实测踩过：物体每 3 帧缺一帧，摘要在第 8 帧采样恰好避开）。
            static const int s_perframe = []()
            {
                const char * v = getenv("FRAMEDIGEST_PERFRAME");
                return v ? atoi(v) : 0;
            }();
            static uint32_t s_pf_frame = 0;
            static uint64_t s_pf_first_fnv = 0;
            static uint32_t s_pf_first_distinct = 0;
            static uint32_t s_pf_diff = 0;

            FrameDigest actual;

            if (!ComputeFrameDigest(rt, actual))
            {
                printf("[%s] readback failed\n", tag);
                std::exit(2);
            }

            if (s_perframe > 0)
            {
                ++s_pf_frame;

                const bool differs = (s_pf_frame > 1) && (actual.fnv1a64 != s_pf_first_fnv);

                if (s_pf_frame == 1)
                {
                    s_pf_first_fnv      = actual.fnv1a64;
                    s_pf_first_distinct = actual.distinct_byte_values;
                }
                else if (differs)
                {
                    ++s_pf_diff;
                }

                printf("[%s] frame=%u image=%u %ux%u distinct=%u fnv1a64=0x%016llX%s\n",
                                       tag, s_pf_frame,
                                       static_cast<unsigned>(rt->GetCurrentFrameIndex()),
                                       actual.width, actual.height,
                                       actual.distinct_byte_values,
                                       static_cast<unsigned long long>(actual.fnv1a64),
                                       differs ? "  <== 与首帧不同" : "");
                fflush(stdout);

                if (s_pf_frame >= static_cast<uint32_t>(s_perframe))
                {
                    const bool ok = (s_pf_diff == 0)
                                 && (s_pf_first_distinct >= expected.distinct_byte_values);

                    printf("[%s] PERFRAME %s：%u 帧，与首帧不同的帧数=%u（首帧 distinct=%u）\n",
                           tag, ok ? "PASS" : "FAIL", s_pf_frame, s_pf_diff, s_pf_first_distinct);
                    fflush(stdout);
                    std::exit(ok ? 0 : 1);
                }

                // ⚠ 钩子是**一次性**的（`VKSwapchainRenderTarget` 执行后即清除，见
                //   `VKSwapchainRenderTarget.cpp:216` "一次性：执行后即清除"）⇒ 逐帧模式
                //   必须**自续装**，否则只打第 1 帧就静默停摆（本模式长期失效的原因）。
                InstallFrameDigestHook(rt, expected, tag);
                return;
            }

            // 配置口径：Debug 钉像素摘要；Release 只钉尺寸/格式/distinct 下限（像素差异属浮点/优化）。
            const bool pixels_ok = (!kFrameDigestPinPixels) || (actual.fnv1a64 == expected.fnv1a64);

            const bool ok = (actual.width  == expected.width)
                         && (actual.height == expected.height)
                         && (actual.distinct_byte_values >= expected.distinct_byte_values)
                         && pixels_ok;

            printf("[%s] %ux%u %s  bytes=%zu  distinct_byte_values=%u  fnv1a64=0x%016llX\n",
                   tag, actual.width, actual.height, actual.format_name, actual.bytes,
                   actual.distinct_byte_values,
                   static_cast<unsigned long long>(actual.fnv1a64));

            printf("[%s] %s（期望 %ux%u distinct>=%u fnv=0x%016llX）\n",
                   tag, ok ? "PASS" : "FAIL", expected.width, expected.height,
                   expected.distinct_byte_values,
                   static_cast<unsigned long long>(expected.fnv1a64));

            if (!kFrameDigestPinPixels)
                printf("[%s] 配置口径：Release 不钉像素摘要（浮点/优化差异 ⇒ 锚定值只在 Debug 成立）；"
                       "已钉 尺寸/格式/distinct 下限。实测 fnv1a64=0x%016llX 仅供落盘比对。\n",
                       tag, static_cast<unsigned long long>(actual.fnv1a64));

            fflush(stdout);
            std::exit(ok ? 0 : 1);
        });
    }
}