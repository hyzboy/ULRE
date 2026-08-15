// CMLiveAudio Hello
// 模块冒烟：验证 CMLiveAudio 库 + 跨模块链接 CMAudio（复用其 DSP）
#include <iostream>
#include <hgl/audio/live/LiveAudio.h>
#include <hgl/audio/Compressor.h>

using namespace hgl;
using namespace hgl::audio;
using namespace hgl::audio::live;

int main()
{
    std::cout << "CMLiveAudio Hello" << std::endl;
    std::cout << "=================" << std::endl;

    // 1. 模块版本（Windows 下 os_char = wchar_t，用 wcout 输出）
    std::wcout << L"Version: " << GetLiveAudioVersion() << std::endl;

    // 2. 跨模块链接验证：复用 CMAudio 的 Compressor（纯 CPU，实时逐样本）
    Compressor comp(48000, {.threshold_db = -20.0f, .ratio = 4.0f,
                            .attack_sec = 0.01f, .release_sec = 0.1f,
                            .makeup_gain_db = 6.0f});

    // 喂入多帧让 attack 平滑达到稳态：0.5 幅度 = -6dB > 阈值 -20dB，应触发压缩
    float out = 0.0f;
    for(int i = 0; i < 2000; i++)
        out = comp.Process(0.5f);

    std::cout << "Compressor 稳态: 输入 0.5 -> 输出 " << out
              << ", 增益衰减 " << comp.GetGainReductionDB() << " dB" << std::endl;

    // 理论值：超量 14dB / ratio 4 = 3.5dB 余量 → 稳态衰减 ≈ -10.5dB
    const bool ok = (comp.GetGainReductionDB() < -5.0f);
    std::cout << std::endl << (ok ? "全部通过" : "失败") << std::endl;
    return ok ? 0 : 1;
}
