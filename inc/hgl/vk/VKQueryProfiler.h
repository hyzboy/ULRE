#pragma once

// ============================================================================
// GpuQueryProfiler —— 引擎内嵌 GPU 帧级查询采样器（Stage 1：Vulkan QueryPool）
//
// 采样内容（每个 dynamic-rendering pass 一对）：
//   * Timestamp 查询：TOP_OF_PIPE / BOTTOM_OF_PIPE 各一次 vkCmdWriteTimestamp，
//     差值 × properties.limits.timestampPeriod ⇒ 该 pass 的 GPU 耗时（ns）。
//   * Pipeline Statistics 查询：BeginRendering 前 reset+begin、EndRendering 后 end，
//     统计 VS/裁剪/光栅/FS/CS/Task/Mesh 的调用计数（需 pipelineStatisticsQuery 特性）。
//
// 生命周期：环状 kFrameSlots 帧槽位；swapchain Submit 视为帧边界（FrameSubmitted），
// 之后在 Collect() 里以**非阻塞** availability 轮询回收最老在飞帧，写入历史环形缓冲，
// 供上层（FrameDebugger / DebugBridge / MCP）读取。查询在回收后立即 vkResetQueryPool
// 复用。无 STL、无每帧新分配（槽位与池均在 Init 一次性建）。
//
// 线程口径：PassBegin/PassEnd/FrameSubmitted 均在录制/提交线程（主渲染线程）调用；
// 历史读取同样发生在主线程（Bridge 的请求在服务端队列中由主线程 pump 执行）。
// ============================================================================

#include<hgl/vk/VK.h>
#include<hgl/type/String.h>
#include<hgl/log/Log.h>

namespace hgl::graph
{

class VulkanDevice;

class GpuQueryProfiler
{
    OBJECT_LOGGER

public:

    /// 统计计数器类型（与请求的 statistic 位一一对应；结果映射按位序动态计算）
    /// 注意：引擎主绘制路径是 mesh shader —— VUID-vkCmdDrawMeshTasks*-pipelineStatistics-07076
    /// 禁止活跃统计查询池含 INPUT_ASSEMBLY/VERTEX/GEOMETRY/CLIPPING/TESSELLATION 位，
    /// 因此本池只请求与之兼容的 FS/CS/Task/Mesh 计数器。
    enum StatType
    {
        StatFragmentShaderInvocations = 0,
        StatComputeShaderInvocations,
        StatTaskShaderInvocations,
        StatMeshShaderInvocations,
        StatCount
    };

    static const char *GetStatName(StatType type);

    /// 帧槽环深度 / 单帧 pass 上限 / 历史保留帧数
    static const uint32_t kFrameSlots        = 8;
    static const uint32_t kMaxPassesPerFrame = 32;
    static const uint32_t kHistoryFrames     = 16;

    /// 单个 pass 的已回收指标
    struct PassMetric
    {
        AnsiString name;
        uint32_t width  = 0;
        uint32_t height = 0;
        double gpu_ms   = 0.0;      ///<timestamp 差值换算（无有效结果时为 -1）
        uint64_t stats[StatCount];
        bool has_stats = false;

        PassMetric(){ for(uint32_t i=0;i<StatCount;i++) stats[i]=0; }
    };

    /// 一帧的已回收指标集合
    struct FrameMetric
    {
        uint64_t frame_id = 0;
        uint32_t pass_count = 0;
        double total_gpu_ms = 0.0;
        PassMetric passes[kMaxPassesPerFrame];
    };

public: // 生命周期（单活动实例：一个渲染设备一份）

    /// 在渲染设备创建后调用；创建 QueryPool 成功即开始采样。失败不致命（空转）。
    static GpuQueryProfiler *Create(VulkanDevice *device);
    static void DestroyActive();
    static GpuQueryProfiler *GetActive(){return active;}

    /// 关闭/开启记录（池保留；关闭时 PassBegin/PassEnd 直接空转）
    void SetEnabled(bool on){enabled=on;}
    bool IsEnabled()const{return enabled;}

    bool IsStatsSupported()const{return stats_pool!=VK_NULL_HANDLE;}
    double GetTimestampPeriodNs()const{return timestamp_period_ns;}
    uint64_t GetSubmittedFrameCount()const{return frame_counter;}
    const char *GetDeviceName()const{return device_name;}

public: // 录制钩子（RenderCmdBuffer 调用；无活动实例/未使能时安全空转）

    /// pass 开始：写首 timestamp、（可选）reset+begin statistics 查询
    void PassBegin(VkCommandBuffer cb,const char *name,uint32_t width,uint32_t height);

    /// pass 结束：end statistics、写末 timestamp。与最近一次未闭合的 PassBegin 配对。
    void PassEnd(VkCommandBuffer cb);

    /// 帧边界：swapchain Submit 成功后调用（fence_index 取 DeviceQueue 提交后的环位）
    void FrameSubmitted(uint32_t fence_index);

    /// 非阻塞回收：轮询最老在飞帧的 availability，完成帧写入历史环并复位其查询。
    void Collect();

public: // 结果读取

    uint32_t GetHistoryCount()const{return history_count;}
    /// index 0 = 最旧保留帧
    const FrameMetric *GetHistory(uint32_t index)const;
    const FrameMetric *GetLatestFrame()const;

private:

    struct PassSlot
    {
        char   name[32];
        uint32_t width  = 0;
        uint32_t height = 0;
        uint32_t ts_first = 0;      ///<本 pass 的 timestamp 查询对起始索引
        uint32_t stats_index = 0;   ///<本 pass 的 statistics 查询索引
        bool recording = false;     ///<已 Begin 未 End
        bool has_stats = false;
    };

    struct FrameSlot
    {
        uint64_t frame_id   = 0;
        uint32_t pass_count = 0;
        uint32_t fence_index = 0;
        bool submitted = false;     ///<已随帧提交，等待回收
        bool in_fifo   = false;     ///<在回收队列中
        bool queries_dirty = false; ///<本槽查询被录制过但未复位（提交失败等异常帧）
        PassSlot passes[kMaxPassesPerFrame];
    };

    explicit GpuQueryProfiler(VulkanDevice *device);
    ~GpuQueryProfiler();

    bool InitPools(VulkanDevice *device);         ///<创建 QueryPool；false=整体不可用

    static GpuQueryProfiler *active;

    VkDevice    device = VK_NULL_HANDLE;
    VkQueryPool ts_pool = VK_NULL_HANDLE;
    VkQueryPool stats_pool = VK_NULL_HANDLE;

    double timestamp_period_ns = 0.0;
    uint32_t stats_result_count = 0;              ///<单查询返回的 u64 个数
    uint32_t stats_result_map[StatCount];         ///<StatType → 结果数组下标（-1 无）
    char   device_name[128] = {0};

    FrameSlot slots[kFrameSlots];
    uint32_t write_slot = 0;                      ///<当前录制帧槽
    uint64_t frame_counter = 0;                   ///<已提交帧计数（=下一帧 frame_id）

    uint32_t fifo[kFrameSlots];                   ///<回收顺序（提交序）的槽索引环
    uint32_t fifo_head = 0;
    uint32_t fifo_count = 0;

    FrameMetric history[kHistoryFrames];
    uint32_t history_head = 0;                    ///<环形：下一个写入位置
    uint32_t history_count = 0;

    bool enabled = true;

    bool CollectSlot(uint32_t slot_index);        ///<槽全部查询可用则回收；true=可出队
    void PushHistory(const FrameMetric &metric);
};//class GpuQueryProfiler

}//namespace hgl::graph
