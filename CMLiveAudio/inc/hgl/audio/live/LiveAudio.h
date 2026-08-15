#pragma once

#include<hgl/platform/Platform.h>
#include<hgl/CoreType.h>

namespace hgl::audio::live
{
    /**
    * CMLiveAudio 直播音频设备模块
    *
    * - 低延迟 WASAPI I/O（采集 / loopback / 输出）
    * - 实时音频图（Node/Port，固定块处理）
    * - 统一效果链：采集与播放双路径均支持变声/效果
    * - 效果型变声（WSOLA 变调 / LPC 共振峰 / 调制效果）
    */

    const os_char *GetLiveAudioVersion();   ///< 获取模块版本字符串
}//namespace hgl::audio::live
