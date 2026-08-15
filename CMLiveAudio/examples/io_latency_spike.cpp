// T3 WASAPI 可行性 spike
// 目标：
//   1) 枚举系统音频端点（播放/采集）并打印友好名
//   2) 验证 WASAPI Loopback：抓取默认播放设备正在播放的声音（含本程序自己播的测试音）
//   3) 端到端延迟测量：播放端周期脉冲（QPC 时间戳）→ loopback 回采 → RMS 检测配对
// 验收：延迟 <20ms（目标 <10ms）；loopback 抓到系统声
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>

#pragma comment(lib, "ole32.lib")

using namespace std;

// ---------------------------------------------------------------------------
// 简单 COM 智能指针（spike 够用）
// ---------------------------------------------------------------------------
template<class T> struct ComPtr
{
    T *p = nullptr;
    ~ComPtr(){ if(p) p->Release(); }
    T   *operator->()const{ return p; }
    T  **operator&(){ return &p; }
    operator T *()const{ return p; }
    bool ok()const{ return p != nullptr; }
};

static double QPC()
{
    static LARGE_INTEGER freq = []{ LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f; }();
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    return double(now.QuadPart) / double(freq.QuadPart);
}

static wstring GetDeviceFriendlyName(IMMDevice *dev)
{
    wstring name = L"(unknown)";
    IPropertyStore *store = nullptr;
    if(SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &store)))
    {
        PROPVARIANT var; PropVariantInit(&var);
        if(SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &var)) && var.vt == VT_LPWSTR)
            name = var.pwszVal;
        PropVariantClear(&var);
        store->Release();
    }
    return name;
}

// ---------------------------------------------------------------------------
// 端点枚举
// ---------------------------------------------------------------------------
static void PrintEndpoints(IMMDeviceEnumerator *enumerator, EDataFlow flow, const char *flow_name)
{
    ComPtr<IMMDeviceCollection> collection;
    if(FAILED(enumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &collection)))
    {
        printf("  [%s] 枚举失败\n", flow_name);
        return;
    }

    UINT count = 0;
    collection->GetCount(&count);
    printf("  [%s] %u 个活动端点:\n", flow_name, count);

    ComPtr<IMMDevice> default_dev;
    enumerator->GetDefaultAudioEndpoint(flow, eConsole, &default_dev);

    for(UINT i = 0; i < count; i++)
    {
        ComPtr<IMMDevice> dev;
        if(FAILED(collection->Item(i, &dev))) continue;

        wstring name = GetDeviceFriendlyName(dev);
        const bool is_default = (default_dev.ok() && dev.p == default_dev.p);

        DWORD state = 0;
        dev->GetState(&state);

        printf("    %S%s [state=%u]\n", name.c_str(), is_default ? "  <== 默认" : "", state);
    }
}

// ---------------------------------------------------------------------------
// 打开默认端点（播放/采集通用）
// ---------------------------------------------------------------------------
static bool OpenDefaultClient(IMMDeviceEnumerator *enumerator, EDataFlow flow,
                              ComPtr<IAudioClient> &client, WAVEFORMATEX **format_out)
{
    ComPtr<IMMDevice> device;
    if(FAILED(enumerator->GetDefaultAudioEndpoint(flow, eConsole, &device)))
        return false;

    if(FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void **)&client)))
        return false;

    WAVEFORMATEX *fmt = nullptr;
    if(FAILED(client->GetMixFormat(&fmt)))
        return false;

    *format_out = fmt;
    return true;
}

// ---------------------------------------------------------------------------
// 主流程
// ---------------------------------------------------------------------------
int main()
{
    SetConsoleOutputCP(CP_UTF8);
    printf("=== T3 WASAPI 可行性 spike ===\n\n");

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    ComPtr<IMMDeviceEnumerator> enumerator;
    if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                               __uuidof(IMMDeviceEnumerator), (void **)&enumerator)))
    {
        printf("无法创建 MMDeviceEnumerator\n");
        return 1;
    }

    printf("--- 1. 设备枚举 ---\n");
    PrintEndpoints(enumerator, eRender, "播放");
    PrintEndpoints(enumerator, eCapture, "采集");
    printf("\n");

    // ---- 2. 打开默认播放端点 + loopback 采集 ----
    ComPtr<IAudioClient> play_client, cap_client;
    WAVEFORMATEX *play_fmt = nullptr, *cap_fmt = nullptr;

    if(!OpenDefaultClient(enumerator, eRender, play_client, &play_fmt))
    {
        printf("无默认播放设备，无法测试（本机可能无音频设备）\n");
        return 1;
    }

    const REFERENCE_TIME buffer_5ms = 50000;    // 100ns 单位：请求 5ms 缓冲

    if(FAILED(play_client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, buffer_5ms, 0, play_fmt, nullptr)))
    {
        printf("播放设备初始化失败\n");
        return 1;
    }

    UINT32 buf_frames = 0;
    play_client->GetBufferSize(&buf_frames);   // 系统实际分配的缓冲（可能大于请求）

    ComPtr<IAudioRenderClient> render;
    if(FAILED(play_client->GetService(__uuidof(IAudioRenderClient), (void **)&render)))
    {
        printf("无法取得 IAudioRenderClient\n");
        return 1;
    }

    // loopback：在默认播放端点上以 LOOPBACK 标志初始化采集
    ComPtr<IMMDevice> default_render;
    enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &default_render);
    if(FAILED(default_render->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void **)&cap_client)))
    {
        printf("loopback 设备激活失败\n");
        return 1;
    }
    if(FAILED(cap_client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, play_fmt, nullptr)))
    {
        printf("loopback 初始化失败\n");
        return 1;
    }

    ComPtr<IAudioCaptureClient> capture;
    if(FAILED(cap_client->GetService(__uuidof(IAudioCaptureClient), (void **)&capture)))
    {
        printf("无法取得 IAudioCaptureClient\n");
        return 1;
    }

    const UINT32 sample_rate = play_fmt->nSamplesPerSec;
    const UINT32 channels    = play_fmt->nChannels;
    const UINT32 block       = buf_frames;            // 块 = 系统实际缓冲
    const double block_sec   = double(block) / sample_rate;

    printf("--- 2. 链路信息 ---\n");
    printf("  采样率 %u Hz, %u 声道, 系统缓冲 %u 帧 (%.1f ms), 块 %u 样本\n",
           sample_rate, channels, block, block_sec * 1000.0, block);

    // ---- 3. 播放脉冲 + 回采检测 ----
    const double pulse_sec   = 0.100;    // 100ms 脉冲
    const double period_sec  = 1.0;      // 周期 1s
    const int    total_sec   = 12;       // 运行 12 秒

    const UINT32 pulse_blocks = UINT32(pulse_sec  / block_sec);   // 脉冲块数
    const UINT32 period_blocks= UINT32(period_sec / block_sec);   // 周期块数
    const int    total_blocks = int(period_sec * total_sec / block_sec);

    vector<float> pulse_signal(block);
    for(UINT32 i = 0; i < block; i++)
        pulse_signal[i] = 0.9f * sinf(2.0f * 3.14159265f * 1000.0f * i / sample_rate);

    vector<double> play_times;    // 每个脉冲的写入时刻
    vector<double> cap_times;     // 每个脉冲的检测时刻

    play_client->Start();
    cap_client->Start();

    // 预填 4 块静音，避免起始爆音/欠载
    for(int i = 0; i < 4; i++)
    {
        BYTE *data = nullptr;
        if(SUCCEEDED(render->GetBuffer(block, &data)) && data)
        {
            memset(data, 0, block * channels * (play_fmt->wBitsPerSample / 8));
            render->ReleaseBuffer(block, 0);
        }
    }

    // 检测状态机
    enum { IDLE, PULSE } detect_state = IDLE;
    int quiet_blocks = 0;

    double next_block_time = QPC();   // 严格块节奏（QPC 绝对时间片）

    for(int b = 0; b < total_blocks; b++)
    {
        // 等待到本块时间片（避免 Sleep 抖动累积导致节奏漂移）
        while(QPC() < next_block_time) Sleep(1);
        next_block_time += block_sec;

        const bool is_pulse = (b % period_blocks) < pulse_blocks;

        // ---- 写播放块（GetBuffer 失败重试，绝不丢块）----
        BYTE *data = nullptr;
        for(;;)
        {
            HRESULT hr = render->GetBuffer(block, &data);
            if(hr == AUDCLNT_E_BUFFER_TOO_LARGE || hr == AUDCLNT_E_BUFFER_ERROR)
            {
                Sleep(1);
                continue;
            }
            if(FAILED(hr) || !data)
            {
                data = nullptr;
                break;
            }
            break;
        }

        if(data)
        {
            if(is_pulse && (b % period_blocks) == 0)   // 脉冲首块：记时间戳
                play_times.push_back(QPC());

            if(is_pulse)
            {
                const int bytes_per_sample = play_fmt->wBitsPerSample / 8;
                for(UINT32 i = 0; i < block; i++)
                    for(UINT32 c = 0; c < channels; c++)
                    {
                        float s = pulse_signal[i];
                        if(play_fmt->wBitsPerSample == 32)
                            ((float *)data)[i * channels + c] = s;
                        else if(play_fmt->wBitsPerSample == 16)
                            ((short *)data)[i * channels + c] = short(s * 32767.0f);
                    }
            }
            else
                memset(data, 0, block * channels * (play_fmt->wBitsPerSample / 8));

            render->ReleaseBuffer(block, 0);
        }

        // ---- 读 loopback 采集包 ----
        UINT32 packets = 0;
        while(capture->GetNextPacketSize(&packets) == S_OK && packets > 0)
        {
            BYTE *cap_data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;

            if(FAILED(capture->GetBuffer(&cap_data, &frames, &flags, nullptr, nullptr)))
                break;

            // RMS 检测
            double sum = 0.0;
            const int bytes_per_sample = play_fmt->wBitsPerSample / 8;
            const UINT32 step = channels;

            if(play_fmt->wBitsPerSample == 32)
                for(UINT32 i = 0; i < frames * step; i += step)
                { const float s = ((float *)cap_data)[i]; sum += s * s; }
            else if(play_fmt->wBitsPerSample == 16)
                for(UINT32 i = 0; i < frames * step; i += step)
                { const float s = ((short *)cap_data)[i] / 32768.0f; sum += s * s; }

            const double rms = (frames > 0) ? sqrt(sum / frames) : 0.0;

            if(detect_state == IDLE && rms > 0.05)
            {
                detect_state = PULSE;
                cap_times.push_back(QPC());
                quiet_blocks = 0;
            }
            else if(detect_state == PULSE)
            {
                if(rms < 0.05)
                {
                    if(++quiet_blocks >= 2)
                        detect_state = IDLE;
                }
                else
                    quiet_blocks = 0;
            }

            capture->ReleaseBuffer(frames);
        }

        Sleep(9);   // 10ms 块节奏（spike 用轮询）
    }

    play_client->Stop();
    cap_client->Stop();

    // ---- 4. 报告 ----
    printf("\n--- 3. 结果 ---\n");

    const size_t n = (play_times.size() < cap_times.size()) ? play_times.size() : cap_times.size();
    if(n == 0)
    {
        printf("  未检测到脉冲（loopback 未抓到声音？）\n");
        return 1;
    }

    double sum = 0.0, min_v = 1e9, max_v = 0.0;
    for(size_t k = 0; k < n; k++)
    {
        const double latency_ms = (cap_times[k] - play_times[k]) * 1000.0;
        if(latency_ms < min_v) min_v = latency_ms;
        if(latency_ms > max_v) max_v = latency_ms;
        sum += latency_ms;
        if(k < 5)
            printf("  脉冲 %zu: %.2f ms\n", k, latency_ms);
    }
    const double mean = sum / n;
    printf("  脉冲数 %zu, 平均 %.2f ms, 最小 %.2f ms, 最大 %.2f ms, 抖动 %.2f ms\n",
           n, mean, min_v, max_v, max_v - min_v);

    const bool loopback_ok = (n >= 3);
    const bool latency_ok  = (mean < 20.0);
    printf("  loopback 抓取: %s\n", loopback_ok ? "OK" : "FAIL");
    printf("  延迟达标(<20ms): %s\n", latency_ok ? "OK" : "NO");
    printf("\n%s\n", (loopback_ok && latency_ok) ? "T3 验收通过" : "T3 未达标，见报告");
    return (loopback_ok && latency_ok) ? 0 : 1;
}

#else   // !_WIN32

int main()
{
    printf("WASAPI spike 仅支持 Windows\n");
    return 1;
}

#endif  // _WIN32
