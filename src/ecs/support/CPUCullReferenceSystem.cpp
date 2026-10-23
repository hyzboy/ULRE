#include <hgl/ecs/support/CPUCullReferenceSystem.h>
#include <hgl/ecs/support/GPUCullSystem.h>
#include <hgl/ecs/core/EntityGPU.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/components/ShadowProxy.h>
#include <hgl/ecs/support/EntityGPUTable.h>
#include <hgl/ecs/support/TransformDataStorage.h>
#include <hgl/ecs/support/TransformAssignmentBuffer.h>
#include <hgl/ecs/support/BoundingVolumeCull.h>
#include <hgl/ecs/systems/tick/TransformSystem.h>
#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/ShaderBufferSources.h>
#include <hgl/common/DebugChannel.h>
#include <hgl/log/Log.h>
#include <cmath>
#include <cstring>
#include <cstdlib>

namespace hgl::ecs
{
    CPUCullReferenceSystem::CPUCullReferenceSystem()
        : System("CPUCullReferenceSystem")
    {
        // T3i（1A + 2β）：cull 相位迁到 RenderFrameSync（本帧批装配之后、绘制之前）
        // ⇒ 台架必须同相位且**显式排在 cull 之后**（旧版靠 RenderCollect 同相位 +
        // 注册顺序的隐式依赖）。对拍关系不变：读的仍是"本帧 cull 提交 + Sync 后的真值"。
        SetExecutionPhase(ExecutionPhase::RenderFrameSync);
        AddDependency<GPUCullSystem>();
    }

    bool CPUCullReferenceSystem::Initialize(ECSContext* context)
    {
        if (!context) return false;
        context_ = context;
        GLogInfo(u8"[CPUCullReferenceSystem] Initialized");
        return true;
    }

    void CPUCullReferenceSystem::Update(float delta_time)
    {
        // 测量模式（perf 通道）：本台架 = CPU 参考剔除全量遍历 + 同步读回 GPU
        // 计数 + 位图比对 ⇒ 本身就是毫秒级 CPU 活。测量运行关掉它，A/B 差值才
        // 不被同一份常数成本稀释；判据跑批不受影响。
        if (ULRE_DEBUG_ON(Perf))
            return;

        if (!context_)
            return;

        // opt-in 门：GPU 剔除管线未启用 ⇒ 对拍没有对照物，整段跳过
        // （本系统的采样会 Map 整块 L2W SSBO，白跑一次全量遍历）。
        auto gpu_cull = context_->GetSystem<GPUCullSystem>();
        if (!gpu_cull)
            return;

        // 未定帧不采样（T3g）：派发攒满 2 帧前读回的是空表/旧表，
        // 对拍会把首帧假象报成 mismatch。
        if (!gpu_cull->IsDispatchSettled())
            return;

        // T3g 台架节奏：**每 60 帧一个采样窗口**（旧版每帧全量跑 + 内部再降频
                // 打印）。异步化后读回真值需要 fence（SyncForParityReadback）⇒ 每帧跑 =
                // 每帧 stall，与"生产路径零同步"矛盾 ⇒ fence + 全量遍历的成本只在采样帧
                // 付出。窗口内三步恒**成套执行**（算 CPU 账 → 同步读 GPU → 位图比对）。
                //
                // ⚠ 采样周期必须与**交换链图数互质**：60 % 3 == 0 ⇒ 本判据**永远只落在
                //   图槽 0**，而"某一张图整体异常"（实测：全量接管下 gather 在槽 1 少写
                //   一行 ⇒ 该图地面消失、`Σ批计数 < 全局幸存`）在**结构上**看不见 —— 恒报
                //   match=3/mismatch=0。改用 61（与 3 互质）⇒ 采样逐步扫过所有图槽。
                static const uint32_t kParitySamplePeriod = 61u;
                        if (++frame_counter_ % kParitySamplePeriod != 0)
                            return;

        // 1. 执行 CPU 剔除（产 cpu 位图）
        PerformCPUCulling();

        // 2. 同步读回 GPU 结果（等本帧 dispatch 完成 → 产 gpu 位图）
        ReadGPUVisibleIndices();

        // 3. 位图对拍 + 打印统计
        CompareResults();

        // 4. T3h：阴影值链三段对拍（A6-d T2：接管已强制 ⇒ gather 行恒可读）
        CheckShadowFacts();
    }

    void CPUCullReferenceSystem::CheckShadowFacts()
    {
        // T3i（③ 多批）：幸存行**不再**是"从 0 连续"——每批写自己的切片。本判据因此
        // 改为**逐批切片走查**，并顺带做两本账的交叉核对：
        //   · overflow 计数 == 0（3A 硬判据：切片溢出即红）
        //   · Σ counters[b] ≤ 全局幸存（全量接管时相等；部分接管时 <，属预期）
        //   · 行 → 实体映射走 **l2w 行号反查**（多批 atomicAdd 交错下保序表作废）
        auto gpu_cull = context_->GetSystem<GPUCullSystem>();
        if (!gpu_cull)
            return;

        const uint32_t batch_count = gpu_cull->GetBatchCount();

        if (batch_count == 0 || gpu_count_ == 0)
            return;

        auto* mtl_buf = gpu_cull->GetMtlOutBuffer();
        auto* l2w_buf = gpu_cull->GetL2WOutBuffer();

        if (!mtl_buf || !l2w_buf)
            return;

        const uint32_t *counters = gpu_cull->MapCurrentBatchCounters(0u /*pass: 主帧*/);
        if (!counters)
            return;

        const uint32_t overflow = counters[batch_count];   // 块内溢出槽 = 块内第 batch_count 行
        uint32_t sum = 0;

        for (uint32_t b = 0; b < batch_count; ++b)
            sum += counters[b];

        // 实体真值 + l2w 行号 → entity index 反查表（存活遍历一次；ForEachAliveEntity
        // 只给存活实体 ⇒ 天然无悬垂句柄问题）
        //
        // ⚠ 反查表在**成员数组**里（不是栈），且填表/查表都先判界：`row[1]` 对未挂载
        // Transform 的实体是哨兵值 ⇒ 直接当索引写就是越界崩溃。
        const auto& entity_table = context_->GetEntityGPUTable();

        for (uint32_t i = 0; i < kMaxVisible; ++i)
            alive_owner_[i] = nullptr;

        for (uint32_t i = 0; i < kL2WRowCapacity; ++i)
            l2w_row_to_entity_[i] = UINT32_MAX;

        context_->ForEachAliveEntity([this, &entity_table](const Entity *e)
        {
            const uint32_t idx = e->GetEntityID().index;
            if (idx >= kMaxVisible)
                return;

            alive_owner_[idx] = e;

            if (const EntityGPU *row = entity_table.Row(idx))
            {
                const uint32_t l2w_row = row->row[1u];   // row[1] = L2W 行（Transform）

                if (l2w_row < kL2WRowCapacity)
                    l2w_row_to_entity_[l2w_row] = idx;
            }
        });

        uint32_t checked = 0, bad = 0, nondefault_seen = 0, rows_read = 0;

        for (uint32_t b = 0; b < batch_count; ++b)
        {
            uint32_t row_base = 0, capacity = 0;

            if (!gpu_cull->GetBatchLayout(0u /*pass: 主帧*/, b, row_base, capacity))
                continue;

            const uint32_t n = (counters[b] < capacity) ? counters[b] : capacity;

            if (n == 0)
                continue;

            const auto *mtl_rows = static_cast<const graph::mtl::MaterialInstanceAddresses *>(
                mtl_buf->Map(VkDeviceSize(row_base) * sizeof(graph::mtl::MaterialInstanceAddresses),
                             VkDeviceSize(n) * sizeof(graph::mtl::MaterialInstanceAddresses)));
            const uint32_t *l2w_rows = static_cast<const uint32_t *>(
                l2w_buf->Map(VkDeviceSize(row_base) * sizeof(uint32_t),
                             VkDeviceSize(n) * sizeof(uint32_t)));

            if (!mtl_rows || !l2w_rows)
            {
                if (mtl_rows) mtl_buf->Unmap();
                if (l2w_rows) l2w_buf->Unmap();
                continue;
            }

            for (uint32_t r = 0; r < n; ++r)
            {
                // 段零：行 → 实体（l2w 行号反查；查不到 = 行内容与实体表不同源 ⇒ 红）
                const uint32_t l2w_row = l2w_rows[r];
                const uint32_t entity_index = (l2w_row < kL2WRowCapacity)
                                            ? l2w_row_to_entity_[l2w_row] : UINT32_MAX;

                if (entity_index == UINT32_MAX)
                {
                    if (bad < 5)
                        GLogWarning(u8"[CPUCullReferenceSystem] slice row→entity 反查失败："
                                    u8"batch=%u r=%u l2w_row=%u", b, r, l2w_row);
                    ++bad;
                    continue;
                }

                const EntityGPU *row = entity_table.Row(entity_index);

                if (!row)
                {
                    ++bad;
                    continue;
                }

                // 段一：CPU 真值（行字节归一式 = 唯一出处；与 collect 事实上行点同一套自由函数）
                const Entity *owner = alive_owner_[entity_index];
                const float bias_for_row = NormalizeShadowBiasToRow(GetShadowBiasMultiplier(owner));
                const uint32_t flags_for_row = CanReceiveShadow(owner) ? 0u
                    : uint32_t(graph::mtl::kMaterialShadowFlagNoReceive);

                const bool nondefault = (flags_for_row != 0u) || (bias_for_row != 1.0f);

                if (nondefault)
                {
                    if (ULRE_DEBUG_ON(Cull) && nondefault_seen < 8)
                        GLogInfo(u8"[CPUCullReferenceSystem] shadow non-default: batch=%u"
                                 u8" idx=%u bias=%.3f flags=%u（三段字节=%.3f/%.3f/%.3f %u/%u/%u）",
                                 b, entity_index, bias_for_row, flags_for_row,
                                 bias_for_row, row->shadow_bias_multiplier,
                                 mtl_rows[r].shadow_bias_multiplier,
                                 flags_for_row, row->shadow_receive, mtl_rows[r].shadow_flags);
                    ++nondefault_seen;
                }

                // 段二（实体行）+ 段三（gather 行）：逐字节比
                if (row->shadow_bias_multiplier != bias_for_row
                 || (row->shadow_receive == 0u ? graph::mtl::kMaterialShadowFlagNoReceive : 0u)
                 != flags_for_row
                 || mtl_rows[r].shadow_bias_multiplier != bias_for_row
                 || mtl_rows[r].shadow_flags != flags_for_row)
                {
                    if (bad < 5)
                        GLogWarning(u8"[CPUCullReferenceSystem] shadow facts chain broken:"
                                    u8" batch=%u r=%u idx=%u bias(accessor/row/gather)=%.3f/%.3f/%.3f"
                                    u8" flags(receive-bit/row/gather)=%u/%u/%u",
                                    b, r, entity_index,
                                    bias_for_row, row->shadow_bias_multiplier,
                                    mtl_rows[r].shadow_bias_multiplier,
                                    flags_for_row,
                                    row->shadow_receive == 0u ? 1u : 0u,
                                    mtl_rows[r].shadow_flags);
                    ++bad;
                }

                ++checked;
            }

            mtl_buf->Unmap();
            l2w_buf->Unmap();

            rows_read += n;
        }

        gpu_cull->UnmapCurrentBatchCounters();

        // 硬判据（3A：非零即红）+ 两本账交叉核对
        // （Σ ≤ 全局幸存恒成立：全局表记**所有**幸存者，切片只记被接管批的）
        if (overflow != 0)
            GLogError(u8"[T3i] 切片溢出！overflow=%u（批归属与切片容量不同源 = 布局错配）",
                      overflow);

        if (sum > gpu_count_)
            GLogError(u8"[T3i] 两本账越界：Σbatch_counts=%u > 全局幸存=%u"
                      u8"（标签/分区与 gather 输出不同源）", sum, gpu_count_);

        if (ULRE_DEBUG_ON(Cull))
            GLogInfo(u8"[T3i] batch slices: image=%u batches=%u rows=%u Σcount=%u global=%u overflow=%u"
                             u8" 值链=%u/%u bad=%u nondefault=%u",
                             context_->GetFrameIndex(), batch_count, rows_read, sum, gpu_count_, overflow,
                             checked, checked, bad, nondefault_seen);
    }

    void CPUCullReferenceSystem::PerformCPUCulling()
    {
        std::memset(cpu_visible_map_, 0, sizeof(cpu_visible_map_));
        cpu_count_ = 0;

        // 获取实体表
        const auto& entity_table = context_->GetEntityGPUTable();
        const uint32_t entity_count = entity_table.kCapacity;

        // 获取 TransformSystem 和 TransformAssignmentBuffer
        auto transform_system = context_->GetSystem<TransformSystem>();
        if (!transform_system) return;

        auto* transform_buffer = transform_system->GetTransformBuffer();
        if (!transform_buffer) return;

        // 获取 L2W buffer 的 CPU 侧映射
        auto* l2w_device_buffer = transform_buffer->GetTransformDataBuffer();
        if (!l2w_device_buffer) return;

        // Map L2W buffer 读取矩阵数据
        const uint32_t l2w_buffer_size = l2w_device_buffer->GetSize();
        void* mapped_data = l2w_device_buffer->Map(0, l2w_buffer_size);
        if (!mapped_data) return;

        const glm::mat4* l2w_matrices = static_cast<const glm::mat4*>(mapped_data);
        const uint32_t l2w_matrix_count = l2w_buffer_size / sizeof(glm::mat4);

        // 获取相机信息
        const auto& cache = context_->GetRenderFrameCache();
        const graph::CameraInfo* camera_info = cache.cameraInfo;
        if (!camera_info)
        {
            l2w_device_buffer->Unmap();
            return;
        }

        // 获取全局 SSBO 注册表（用于访问 GeometryAABB）
        auto* gc = context_->GetGraphicsContext();
        if (!gc)
        {
            l2w_device_buffer->Unmap();
            return;
        }
        auto* global_registry = gc->GetGlobalSSBOBufferRegistry();
        if (!global_registry)
        {
            l2w_device_buffer->Unmap();
            return;
        }

        // 遍历所有实体
        const EntityRenderIDTable &render_ids = context_->GetEntityRenderIDTable();

        for (uint32_t i = 0; i < entity_count; ++i)
        {
            const EntityGPU* row = entity_table.Row(i);
            if (!row) continue;

            // 跳过非活跃实体
            if (!EntityGPUTable::IsActive(*row)) continue;

            // 跳过不可见实体
            if (!EntityGPUTable::IsVisible(*row)) continue;

            // T3d-3a：geometry_id 取**实体 4-ID 表**（池口径，与 GPU 剔除逐字同式；
            // `row[Geometry]` 是组件池行、与 AABB 池不同口径 —— 旧版两端错得同源，见 §8.2d）
            const graph::RenderItemDescriptor *ids = render_ids.Row(0u /*pass: 主帧*/, i);

            if (!ids || ids->geometry_id == 0) continue;

            const uint32_t geometry_id = ids->geometry_id;

            // 获取 L2W 行号（仍走实体行表 row[Transform]：变换侧写的帧槽含基址行号）
            const uint32_t l2w_row = row->row[static_cast<uint32_t>(ComponentType::Transform)];

            if (l2w_row == 0) continue;

            // 边界检查：L2W 行号必须在 buffer 范围内
            if (l2w_row >= l2w_matrix_count) continue;

            // 从 L2W buffer 读取世界矩阵
            const glm::mat4 world_matrix = l2w_matrices[l2w_row];

            // 获取 GeometryAABB（从全局池）
            const graph::mtl::GeometryAABB* aabb = global_registry->GetGeometryAABB(geometry_id);
            if (!aabb) continue;

            // 变换 AABB 中心到世界空间
            const glm::vec4 local_center(aabb->center[0], aabb->center[1], aabb->center[2], 1.0f);
            const glm::vec3 world_center = glm::vec3(world_matrix * local_center);

            // 计算变换后的 extents（简化：取最大缩放）
            // 公式单一来源 = `GetMaxWorldScaleRaw`（**用 Raw 不用带退化保护的 GetMaxWorldScale**：
            // 后者在 scale==0 时给 1.0，会与 GPU 判出不同可见性 ⇒ 假对拍差异）
            const float max_scale = GetMaxWorldScaleRaw(world_matrix);
            const glm::vec3 world_extents(aabb->extents[0] * max_scale, aabb->extents[1] * max_scale, aabb->extents[2] * max_scale);

            // 视锥剔除（6 个平面）。center[3] = 外接球半径，< 0 ⇒ 「始终可见」哨兵
            //（与 entity_gpu_cull.glsl 逐字同式；写者 = Geometry::RefreshGeometryAABBRow，
            //  2D 几何（V2F 位置）无有效本地 AABB ⇒ 全靠这条不静默剔空）
            bool visible = true;
            for (int p = 0; aabb->center[3] >= 0.0f && p < 6; ++p)
            {
                const glm::vec4& plane = camera_info->frustum_planes[p];
                const glm::vec3 normal = glm::vec3(plane);
                const float distance = plane.w;

                // 计算 AABB 在平面法线上的投影半径
                const float radius = std::abs(normal.x) * world_extents.x
                                   + std::abs(normal.y) * world_extents.y
                                   + std::abs(normal.z) * world_extents.z;

                // 计算中心到平面的有符号距离
                const float center_distance = glm::dot(normal, world_center) + distance;

                // 如果中心距离 < -半径，则完全在平面外侧
                if (center_distance < -radius)
                {
                    visible = false;
                    break;
                }
            }

            if (visible && i < kMaxVisible)
            {
                cpu_visible_map_[i] = 1;
                ++cpu_count_;
            }
        }

        // Unmap L2W buffer
        l2w_device_buffer->Unmap();
    }

    void CPUCullReferenceSystem::ReadGPUVisibleIndices()
    {
        std::memset(gpu_visible_map_, 0, sizeof(gpu_visible_map_));
        gpu_count_ = 0;

        // 从 GPUCullSystem 获取 GPU 结果
        auto gpu_cull_system = context_->GetSystem<GPUCullSystem>();
        if (!gpu_cull_system) return;

        // T3g 同步点（**只在采样窗口付出**）：派发是异步提交 ⇒ 不等就 Map 读回
        // 的可能是上一已定帧的残影。Host Coherent ≠ 已执行（FIFO 只保证序，
        // 不保证到）⇒ 先等本帧 dispatch 的 fence，再读。
        if (!gpu_cull_system->SyncForParityReadback())
            return;

        const uint32_t gpu_count = gpu_cull_system->GetGPUVisibleCount();
        if (gpu_count == 0) return;

        // 读取 visible_indices buffer
        auto* buffer = gpu_cull_system->GetVisibleIndicesBuffer();
        if (!buffer) return;

        const uint32_t read_count = (gpu_count < kMaxVisible) ? gpu_count : kMaxVisible;

        void* mapped = buffer->Map(0, read_count * sizeof(uint32_t));
        if (mapped)
        {
            const uint32_t* indices = static_cast<const uint32_t*>(mapped);
            for (uint32_t k = 0; k < read_count; ++k)
            {
                const uint32_t idx = indices[k];
                if (idx < kMaxVisible && gpu_visible_map_[idx] == 0)
                {
                    gpu_visible_map_[idx] = 1;
                    ++gpu_count_;
                }
            }
            buffer->Unmap();
        }
    }

    void CPUCullReferenceSystem::CompareResults()
    {
        // 位图对拍（逐槽）：集合相等 ⇔ 两侧每一位一致；不一致槽数 = mismatch。
        // match = 两侧同时为 1 的槽数。旧 std::set 路子是给"atomicAdd 顺序不定"
        // 买的单，位图把顺序问题消掉（且零动态分配，替代 <set>/<algorithm>）。
        uint32_t mismatch = 0, both = 0;
        uint32_t cpu_only_shown = 0, gpu_only_shown = 0;

        for (uint32_t i = 0; i < kMaxVisible; ++i)
        {
            const uint8_t c = cpu_visible_map_[i];
            const uint8_t g = gpu_visible_map_[i];

            if (c == g)
            {
                if (c)
                    ++both;
                continue;
            }

            ++mismatch;
            if (c)
            {
                if (cpu_only_shown < 5)
                {
                    if (cpu_only_shown == 0)
                        GLogWarning(u8"[CPUCullReferenceSystem] CPU-only indices:");
                    GLogWarning(u8"  idx=%u", i);
                    ++cpu_only_shown;
                }
            }
            else
            {
                if (gpu_only_shown < 5)
                {
                    if (gpu_only_shown == 0)
                        GLogWarning(u8"[CPUCullReferenceSystem] GPU-only indices:");
                    GLogWarning(u8"  idx=%u", i);
                    ++gpu_only_shown;
                }
            }
        }

        if (ULRE_DEBUG_ON(Cull))
            GLogInfo(u8"[CPUCullReferenceSystem] Parity: image=%u CPU=%u, GPU=%u, match=%u, mismatch=%u",
                     context_->GetFrameIndex(), cpu_count_, gpu_count_, both, mismatch);
    }

}
