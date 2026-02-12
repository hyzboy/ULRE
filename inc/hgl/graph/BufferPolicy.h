#pragma once

#include<hgl/graph/VK.h>
#include<string>

VK_NAMESPACE_BEGIN

enum class BufferCommitPolicy
{
    Auto           = 0,     // 根据缓冲区使用情况/内存类型自动决定
    StagedOnly     = 1,     // 仅在脏时刷新暂存缓冲区
    Always         = 2,     // 总是在更新时刷新
    Manual         = 3      // 从不自动刷新
};

enum class BufferUpdateClass
{
    Default        = 0,     // 默认
    CriticalPerFrame = 1,   // 关键帧数据 (相机, 每帧UBO)
    TransformData  = 2,     // 变换ID/数据缓冲区
    MeshStatic     = 3,     // 静态VBO/IBO
    MeshDynamic    = 4,     // 动态VBO/IBO
    TextureTile    = 5,     // 瓦片/流式纹理
    Particle       = 6,     // 粒子位置数据
    Deferred       = 7,     // 可延迟到下一帧
    Manual         = 8      // 由调用者手动控制
};

enum class BufferPriority
{
    CRITICAL       = 0,     // 必须最早提交
    HIGH           = 1,     // 高优先级
    NORMAL         = 2,     // 普通优先级
    LOW            = 3      // 低优先级, 可延迟
};

enum class BufferUpdateRate
{
    PER_FRAME      = 0,     // 每帧更新
    FREQUENT       = 1,     // 频繁更新
    BURST          = 2,     // 突发更新后稳定
    SPARSE         = 3,     // 稀疏更新
    RARE           = 4      // 很少更新
};

enum class BufferSubmitTiming
{
    IMMEDIATE      = 0,     // 立即提交
    SAME_FRAME     = 1,     // 必须在同一帧内提交
    NEXT_FRAME_OK  = 2,     // 可延迟到下一帧
    DEFERRED       = 3      // 可任意延迟
};

enum class BufferDropPolicy
{
    NEVER          = 0,     // 永不丢弃数据
    DROP_OLD       = 1,     // 丢弃旧的待处理数据
    DROP_NEW       = 2      // 丢弃新的传入数据
};

enum class BufferDeadlinePolicy
{
    NONE           = 0,     // 无硬期限
    SOFT           = 1,     // 软期限, 提升优先级
    HARD           = 2      // 硬期限, 强制立即提交
};

enum class BufferPromotePolicy
{
    NONE           = 0,     // 无自动提升
    AUTO_RAISE     = 1,     // 优先级上升一级
    FORCE_HIGH     = 2      // 强制为HIGH优先级
};

enum class BufferMemoryPolicy
{
    REBAR          = 0,     // 可调整大小BAR (或回退)
    RING           = 1,     // 环形缓冲区 (N帧循环)
    STAGED         = 2,     // 暂存缓冲区 (CPU->GPU)
    AUTO           = 3      // 根据用途自动选择
};

enum class BufferCpuResident
{
    KEEP           = 0,     // 保持CPU数据活跃
    RELEASE        = 1,     // 可在提交后释放
    AUTO           = 2      // 系统决定
};

enum class BufferSplitPolicy
{
    NO_SPLIT       = 0,     // 永不分割
    ALLOW_SPLIT    = 1,     // 允许分割
    PREFER_SPLIT   = 2      // 优先分割
};

struct BufferPolicy
{
    BufferPriority      priority            = BufferPriority::NORMAL;               // 缓冲区优先级
    BufferUpdateRate    updateRate          = BufferUpdateRate::RARE;               // 更新频率
    BufferSubmitTiming  submitTiming        = BufferSubmitTiming::DEFERRED;         // 提交时机
    uint32_t            maxLatency          = 2;                                    // 最大延迟 (0=自动)
    std::string         budgetGroup         = "GLOBAL";                             // 预算组名称
    VkDeviceSize        budgetLimit         = 0;                                    // 预算限制 (0=自动)
    bool                queueing            = true;                                 // 启用提交队列
    BufferSplitPolicy   splitPolicy         = BufferSplitPolicy::NO_SPLIT;          // 分割策略
    VkDeviceSize        splitChunk          = 0;                                    // 分割块大小 (0=自动)
    BufferDropPolicy    dropPolicy          = BufferDropPolicy::NEVER;              // 丢弃策略
    BufferDeadlinePolicy deadlinePolicy     = BufferDeadlinePolicy::NONE;           // 期限策略
    uint32_t            deadline            = 0;                                    // 期限 (0=自动)
    BufferPromotePolicy promotePolicy       = BufferPromotePolicy::NONE;            // 优先级提升策略
    std::string         promoteRule;                                                // 优先级提升规则
    BufferMemoryPolicy  memoryPolicy        = BufferMemoryPolicy::AUTO;             // 内存策略
    BufferCpuResident   cpuResident         = BufferCpuResident::AUTO;              // CPU驻留策略
    uint32_t            ringFrameCount      = 3;                                    // 环形缓冲帧数 (RING策略)
    BufferCpuResident   stagedPersist       = BufferCpuResident::AUTO;              // 暂存持久化 (STAGED策略)
    BufferCommitPolicy  commitPolicy        = BufferCommitPolicy::Auto;             // 提交策略
    std::string         devNotes;                                                   // 开发备注
};//struct BufferPolicy

VK_NAMESPACE_END
