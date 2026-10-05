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
// ─────────────────────────────────────────────────────────────────────────────

#include<hgl/vk/VKTextureReadback.h>
#include<hgl/vk/VKFormat.h>
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
            FrameDigest actual;

            if (!ComputeFrameDigest(rt, actual))
            {
                printf("[%s] readback failed\n", tag);
                std::exit(2);
            }

            const bool ok = (actual.width  == expected.width)
                         && (actual.height == expected.height)
                         && (actual.distinct_byte_values >= expected.distinct_byte_values)
                         && (actual.fnv1a64 == expected.fnv1a64);

            printf("[%s] %ux%u %s  bytes=%zu  distinct_byte_values=%u  fnv1a64=0x%016llX\n",
                   tag, actual.width, actual.height, actual.format_name, actual.bytes,
                   actual.distinct_byte_values,
                   static_cast<unsigned long long>(actual.fnv1a64));

            printf("[%s] %s（期望 %ux%u distinct>=%u fnv=0x%016llX）\n",
                   tag, ok ? "PASS" : "FAIL", expected.width, expected.height,
                   expected.distinct_byte_values,
                   static_cast<unsigned long long>(expected.fnv1a64));

            fflush(stdout);
            std::exit(ok ? 0 : 1);
        });
    }
}