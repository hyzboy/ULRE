// ============================================================================
// GpuQueryProfiler 实现（详见 VKQueryProfiler.h 头注释）
// ============================================================================

#include<hgl/vk/VKQueryProfiler.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKPhysicalDevice.h>

namespace hgl::graph
{

GpuQueryProfiler *GpuQueryProfiler::active = nullptr;

const char *GpuQueryProfiler::GetStatName(StatType type)
{
    switch(type)
    {
    case StatFragmentShaderInvocations:return "fragment_shader_invocations";
    case StatComputeShaderInvocations: return "compute_shader_invocations";
    case StatTaskShaderInvocations:    return "task_shader_invocations";
    case StatMeshShaderInvocations:    return "mesh_shader_invocations";
    default:                           return "unknown";
    }
}

static void CopyName(char *dst,size_t cap,const char *src)
{
    if(!src){ dst[0]=0; return; }
    size_t i=0;
    for(; i+1<cap && src[i]; i++) dst[i]=src[i];
    dst[i]=0;
}

GpuQueryProfiler::GpuQueryProfiler(VulkanDevice *device)
{
    for(uint32_t i=0;i<StatCount;i++) stats_result_map[i]=0xFFFFFFFFu;

    const VulkanPhyDevice *phy = device->GetPhyDevice();
    const VkPhysicalDeviceProperties &props = phy->GetProperties();

    timestamp_period_ns = props.limits.timestampPeriod;
    CopyName(device_name,sizeof(device_name),props.deviceName);

    LogInfo(u8"GpuQueryProfiler: device=\"%hs\" timestampPeriod=%.3f ns",
            props.deviceName, timestamp_period_ns);
}

GpuQueryProfiler::~GpuQueryProfiler()
{
    if(stats_pool) vkDestroyQueryPool(device,stats_pool,nullptr);
    if(ts_pool)    vkDestroyQueryPool(device,ts_pool,nullptr);
}

// ── 池创建：timestamp 必建；stats 依特性/建池结果逐级降配 ──────────────────

static const VkQueryPipelineStatisticFlagBits kStatBits[static_cast<int>(GpuQueryProfiler::StatCount)] =
{
    VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,
    VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT,
    VK_QUERY_PIPELINE_STATISTIC_TASK_SHADER_INVOCATIONS_BIT_EXT,
    VK_QUERY_PIPELINE_STATISTIC_MESH_SHADER_INVOCATIONS_BIT_EXT,
};

bool GpuQueryProfiler::InitPools(VulkanDevice *device_)
{
    device = device_->GetDevice();

    // 回收复用依赖 host 复位（Vulkan 1.2 起核心特性；1.4 设备必支持）
    if(!device_->GetPhyDevice()->GetFeatures12().hostQueryReset)
    {
        LogError(u8"GpuQueryProfiler: 设备不支持 hostQueryReset，禁用 GPU 查询采样");
        return false;
    }

    VkQueryPoolCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;

    // timestamp：一对/pass × pass 上限 × 帧槽
    ci.queryType   = VK_QUERY_TYPE_TIMESTAMP;
    ci.queryCount  = kFrameSlots * kMaxPassesPerFrame * 2;
    if(vkCreateQueryPool(device,&ci,nullptr,&ts_pool)!=VK_SUCCESS)
    {
        LogError(u8"GpuQueryProfiler: timestamp QueryPool 创建失败");
        return false;
    }

    // 查询池新建后各查询处于“未初始化”态，必须先复位才能首次使用（VUID-00830）
    vkResetQueryPool(device,ts_pool,0,kFrameSlots*kMaxPassesPerFrame*2);

    if(!device_->GetPhyDevice()->GetFeatures10().pipelineStatisticsQuery)
    {
        LogInfo(u8"GpuQueryProfiler: 设备不支持 pipelineStatisticsQuery，仅采集 timestamp");
        return true;
    }

    // 只请求与 mesh shader 绘制兼容的位（见头文件 07076 注释）；
    // VUID-07069：未启用 meshShaderQueries 时不得请求 task/mesh 位 —— 校验层在
    // **首次建池**就会判错，不能只靠失败后重试（重试前的那次 VUID 已被记录）。
    // 本工程在 VKDeviceCreater 里把设备支持的 mesh 特性原样启用，故此处读支持值即可。
    VkQueryPipelineStatisticFlags stat_flags =
        VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT
      | VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;

    if(device_->GetPhyDevice()->GetMeshShaderFeatures().meshShaderQueries)
        stat_flags |= VK_QUERY_PIPELINE_STATISTIC_TASK_SHADER_INVOCATIONS_BIT_EXT
                    | VK_QUERY_PIPELINE_STATISTIC_MESH_SHADER_INVOCATIONS_BIT_EXT;

    VkQueryPoolCreateInfo sci{};
    sci.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    sci.queryType  = VK_QUERY_TYPE_PIPELINE_STATISTICS;
    sci.queryCount = kFrameSlots * kMaxPassesPerFrame;
    sci.pipelineStatistics = stat_flags;

    // 建池仍可能因其他原因失败（位不受支持等）：去掉 task/mesh 位重试
    if(vkCreateQueryPool(device,&sci,nullptr,&stats_pool)!=VK_SUCCESS)
    {
        sci.pipelineStatistics &= ~(VK_QUERY_PIPELINE_STATISTIC_TASK_SHADER_INVOCATIONS_BIT_EXT
                                  | VK_QUERY_PIPELINE_STATISTIC_MESH_SHADER_INVOCATIONS_BIT_EXT);
        if(vkCreateQueryPool(device,&sci,nullptr,&stats_pool)!=VK_SUCCESS)
        {
            LogInfo(u8"GpuQueryProfiler: statistics QueryPool 创建失败，仅采集 timestamp");
            stats_pool = VK_NULL_HANDLE;
            return true;
        }
    }

    // 结果映射：统计结果按 bit 升序排列；为每个已请求位计算下标
    {
        uint32_t index=0;
        for(uint32_t bit=0; bit<32; bit++)
        {
            const VkQueryPipelineStatisticFlagBits flag = static_cast<VkQueryPipelineStatisticFlagBits>(1u<<bit);
            if(!(sci.pipelineStatistics & flag)) continue;
            for(uint32_t s=0;s<StatCount;s++)
                if(kStatBits[s]==flag) stats_result_map[s]=index;
            index++;
        }
        stats_result_count = index;
    }

    vkResetQueryPool(device,stats_pool,0,kFrameSlots*kMaxPassesPerFrame);

    LogInfo(u8"GpuQueryProfiler: pipeline statistics 已启用（%u 个计数器）",stats_result_count);
    return true;
}

GpuQueryProfiler *GpuQueryProfiler::Create(VulkanDevice *device)
{
    if(active) return active;

    GpuQueryProfiler *profiler = new GpuQueryProfiler(device);

    if(!profiler->InitPools(device))
    {
        delete profiler;
        return(nullptr);
    }

    active = profiler;
    return profiler;
}

void GpuQueryProfiler::DestroyActive()
{
    if(!active) return;

    delete active;
    active = nullptr;
}

// ── 录制钩子 ───────────────────────────────────────────────────────────────

void GpuQueryProfiler::PassBegin(VkCommandBuffer cb,const char *name,uint32_t width,uint32_t height)
{
    if(!enabled) return;

    FrameSlot &fs = slots[write_slot];
    if(fs.submitted) return;                          // GPU 落后整环：本帧丢弃记录
    if(fs.pass_count >= kMaxPassesPerFrame) return;   // pass 数超上限：不记录（避免未闭合查询）

    if(fs.pass_count==0 && fs.queries_dirty)
    {
        // 上一轮该槽的记录未随帧提交（提交失败），复位后重录，避免 00830
        vkResetQueryPool(device,ts_pool,write_slot*kMaxPassesPerFrame*2,kMaxPassesPerFrame*2);
        if(stats_pool)
            vkResetQueryPool(device,stats_pool,write_slot*kMaxPassesPerFrame,kMaxPassesPerFrame);
        fs.queries_dirty = false;
    }

    fs.queries_dirty = true;

    const uint32_t pi = fs.pass_count;
    PassSlot &ps = fs.passes[pi];

    ps.ts_first    = write_slot*kMaxPassesPerFrame*2 + pi*2;
    ps.stats_index = write_slot*kMaxPassesPerFrame + pi;
    ps.width=width; ps.height=height;
    CopyName(ps.name,sizeof(ps.name),name);

    // timestamp 查询也必须在本命令缓冲内复位；仅依赖设备侧 reset
    // 可能被校验层漏记每个 slot 的可用状态。
    vkCmdResetQueryPool(cb,ts_pool,ps.ts_first,2);
    vkCmdWriteTimestamp(cb,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,ts_pool,ps.ts_first);

    ps.has_stats = false;
    if(stats_pool)
    {
        // reset+begin 必须在 renderpass 实例之外（此处调用点在 vkCmdBeginRendering 之前）
        vkCmdResetQueryPool(cb,stats_pool,ps.stats_index,1);
        vkCmdBeginQuery(cb,stats_pool,ps.stats_index,0);
        ps.has_stats = true;
    }

    ps.recording = true;
    fs.pass_count++;
}

void GpuQueryProfiler::PassEnd(VkCommandBuffer cb)
{
    if(!enabled) return;

    FrameSlot &fs = slots[write_slot];
    if(fs.submitted || fs.pass_count==0) return;

    PassSlot &ps = fs.passes[fs.pass_count-1];
    if(!ps.recording) return;                         // 未配对的 End（如 BeginRendering(ri) 路径）

    ps.recording = false;

    if(ps.has_stats)
        vkCmdEndQuery(cb,stats_pool,ps.stats_index);

    vkCmdWriteTimestamp(cb,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,ts_pool,ps.ts_first+1);
}

void GpuQueryProfiler::FrameSubmitted(uint32_t fence_index)
{
    if(!enabled) return;

    FrameSlot &fs = slots[write_slot];
    if(fs.submitted) return;                          // 未轮转（异常路径），忽略

    fs.frame_id    = frame_counter++;
    fs.fence_index = fence_index;
    fs.in_fifo     = true;

    // 记录环满（GPU 落后 kFrameSlots 帧）时丢最老一帧的回收：直接复位它
    if(fifo_count == kFrameSlots)
    {
        const uint32_t drop = fifo[fifo_head];
        FrameSlot &oldest = slots[drop];
        oldest.submitted = false;
        oldest.in_fifo   = false;
        oldest.pass_count = 0;
        fifo_head = (fifo_head+1)%kFrameSlots;
        fifo_count--;

        // 丢弃帧的查询未回收过，复用前必须复位，否则 00830（查询仍可用）
        vkResetQueryPool(device,ts_pool,drop*kMaxPassesPerFrame*2,kMaxPassesPerFrame*2);
        if(stats_pool)
            vkResetQueryPool(device,stats_pool,drop*kMaxPassesPerFrame,kMaxPassesPerFrame);

        LogWarning(u8"GpuQueryProfiler: 回收环溢出，丢帧（GPU 落后 %u 帧）",kFrameSlots);
    }

    fifo[(fifo_head+fifo_count)%kFrameSlots] = write_slot;
    fs.submitted = true;
    fifo_count++;

    write_slot = (write_slot+1)%kFrameSlots;

    Collect();
}

// ── 非阻塞回收 ─────────────────────────────────────────────────────────────

bool GpuQueryProfiler::CollectSlot(uint32_t slot_index)
{
    FrameSlot &fs = slots[slot_index];

    if(fs.pass_count==0)
    {
        // 空帧（无 pass 记录）：直接可出队
        return true;
    }

    // timestamp 结果：每个 u64 查询后跟一个 availability u64
    uint64_t ts_buf[kMaxPassesPerFrame*4];
    const VkResult ts_r = vkGetQueryPoolResults(device,ts_pool,
        slot_index*kMaxPassesPerFrame*2, fs.pass_count*2,
        sizeof(ts_buf), ts_buf, sizeof(uint64_t)*2,
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
    if(ts_r!=VK_SUCCESS) return false;

    uint64_t st_buf[kMaxPassesPerFrame*(1+GpuQueryProfiler::StatCount+1)];
    if(stats_pool)
    {
        const VkResult st_r = vkGetQueryPoolResults(device,stats_pool,
            slot_index*kMaxPassesPerFrame, fs.pass_count,
            sizeof(st_buf), st_buf, sizeof(uint64_t)*(stats_result_count+1),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
        if(st_r!=VK_SUCCESS) return false;
    }

    // 全部可用才成帧（保序简单）
    for(uint32_t i=0;i<fs.pass_count;i++)
    {
        if(ts_buf[i*4+1]==0 || ts_buf[i*4+3]==0) return false;
        if(stats_pool && fs.passes[i].has_stats && st_buf[i*(stats_result_count+1)+stats_result_count]==0)
            return false;
    }

    FrameMetric metric;
    metric.frame_id = fs.frame_id;
    metric.pass_count = fs.pass_count;

    for(uint32_t i=0;i<fs.pass_count;i++)
    {
        const PassSlot &ps = fs.passes[i];
        PassMetric &pm = metric.passes[i];

        pm.name    = AnsiString(ps.name);
        pm.width   = ps.width;
        pm.height  = ps.height;

        const uint64_t t0 = ts_buf[i*4+0];
        const uint64_t t1 = ts_buf[i*4+2];
        pm.gpu_ms = static_cast<double>(t1-t0) * timestamp_period_ns / 1.0e6;
        metric.total_gpu_ms += pm.gpu_ms;

        if(stats_pool && ps.has_stats)
        {
            const uint64_t *row = st_buf + i*(stats_result_count+1);
            for(uint32_t s=0;s<StatCount;s++)
            {
                const uint32_t r = stats_result_map[s];
                pm.stats[s] = (r<stats_result_count) ? row[r] : 0;
            }
            pm.has_stats = true;
        }
    }

    PushHistory(metric);

    // 复位查询供环复用（结果已回收 = 查询均已执行完毕，设备侧复位安全）
    vkResetQueryPool(device,ts_pool,slot_index*kMaxPassesPerFrame*2,fs.pass_count*2);
    if(stats_pool)
        vkResetQueryPool(device,stats_pool,slot_index*kMaxPassesPerFrame,fs.pass_count);

    fs.queries_dirty = false;

    return true;
}

void GpuQueryProfiler::Collect()
{
    while(fifo_count>0)
    {
        const uint32_t si = fifo[fifo_head];

        if(!CollectSlot(si)) break;       // 保序：最老帧未完成则等下一轮

        FrameSlot &fs = slots[si];
        fs.submitted  = false;
        fs.in_fifo    = false;
        fs.pass_count = 0;
        for(uint32_t i=0;i<kMaxPassesPerFrame;i++)
            fs.passes[i].recording = false;

        fifo_head = (fifo_head+1)%kFrameSlots;
        fifo_count--;
    }
}

// ── 历史读取 ───────────────────────────────────────────────────────────────

void GpuQueryProfiler::PushHistory(const FrameMetric &metric)
{
    hgl::MutexLock lock(&history_mutex);
    history[history_head] = metric;
    history_head = (history_head+1)%kHistoryFrames;
    if(history_count < kHistoryFrames) history_count++;
}

const GpuQueryProfiler::FrameMetric *GpuQueryProfiler::GetHistory(uint32_t index)const
{
    if(index >= history_count) return(nullptr);

    const uint32_t oldest = (history_head + kHistoryFrames - history_count)%kHistoryFrames;
    return(&history[(oldest+index)%kHistoryFrames]);
}

const GpuQueryProfiler::FrameMetric *GpuQueryProfiler::GetLatestFrame()const
{
    if(history_count==0) return(nullptr);
    return(&history[(history_head + kHistoryFrames - 1)%kHistoryFrames]);
}

bool GpuQueryProfiler::GetLatestFrameSnapshot(FrameMetric &snapshot)const
{
    hgl::MutexLock lock(&history_mutex);
    if(history_count==0) return false;

    snapshot=history[(history_head+kHistoryFrames-1)%kHistoryFrames];
    return true;
}

uint32_t GpuQueryProfiler::GetHistorySnapshot(FrameMetric *snapshots,uint32_t max_count)const
{
    if(!snapshots || max_count==0) return 0;

    hgl::MutexLock lock(&history_mutex);
    const uint32_t count=(history_count<max_count)?history_count:max_count;
    const uint32_t oldest=(history_head+kHistoryFrames-history_count)%kHistoryFrames;
    for(uint32_t i=0;i<count;++i)
        snapshots[i]=history[(oldest+i)%kHistoryFrames];
    return count;
}

}//namespace hgl::graph
