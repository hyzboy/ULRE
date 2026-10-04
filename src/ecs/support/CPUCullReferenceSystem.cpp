#include <hgl/ecs/support/CPUCullReferenceSystem.h>
#include <hgl/ecs/support/GPUCullSystem.h>
#include <hgl/ecs/core/EntityGPU.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/components/ShadowProxy.h>
#include <hgl/ecs/support/EntityGPUTable.h>
#include <hgl/ecs/support/TransformDataStorage.h>
#include <hgl/ecs/support/TransformAssignmentBuffer.h>
#include <hgl/ecs/systems/tick/TransformSystem.h>
#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/ShaderBufferSources.h>
#include <hgl/log/Log.h>
#include <cmath>
#include <cstring>
#include <cstdlib>

namespace hgl::ecs
{
    CPUCullReferenceSystem::CPUCullReferenceSystem()
        : System("CPUCullReferenceSystem")
    {
        SetExecutionPhase(ExecutionPhase::RenderCollect);
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
        // 测量模式（PBR_PERF=1）：本台架 = CPU 参考剔除全量遍历 + 同步读回 GPU
        // 计数 + 位图比对 ⇒ 本身就是毫秒级 CPU 活。测量运行关掉它，A/B 差值才
        // 不被同一份常数成本稀释；判据跑批不受影响。
        static const bool s_measure_mode = getenv("PBR_PERF") != nullptr;
        if (s_measure_mode)
            return;

        if (!context_)
            return;

        // opt-in 门：GPU 剔除管线未启用 ⇒ 对拍没有对照物，整段跳过
        // （本系统的采样会 Map 整块 L2W SSBO，白跑一次全量遍历）。
        auto gpu_cull = context_->GetSystem<GPUCullSystem>();
        if (!gpu_cull || !gpu_cull->IsEnabled())
            return;

        // 未定帧不采样（T3g）：派发攒满 2 帧前读回的是空表/旧表，
        // 对拍会把首帧假象报成 mismatch。
        if (!gpu_cull->IsDispatchSettled())
            return;

        // T3g 台架节奏：**每 60 帧一个采样窗口**（旧版每帧全量跑 + 内部再降频
        // 打印）。异步化后读回真值需要 fence（SyncForParityReadback）⇒ 每帧跑 =
        // 每帧 stall，与"生产路径零同步"矛盾 ⇒ fence + 全量遍历的成本只在采样帧
        // 付出。窗口内三步恒**成套执行**（算 CPU 账 → 同步读 GPU → 位图比对）。
        if (++frame_counter_ % 60 != 0)
            return;

        // 1. 执行 CPU 剔除（产 cpu 位图）
        PerformCPUCulling();

        // 2. 同步读回 GPU 结果（等本帧 dispatch 完成 → 产 gpu 位图）
        ReadGPUVisibleIndices();

        // 3. 位图对拍 + 打印统计
        CompareResults();

        // 4. T3h：阴影值链三段对拍（仅形态 B 接管中有 gather 行可读）
        if (gpu_cull->IsDrawOverrideEnabled())
            CheckShadowFacts();
    }

    void CPUCullReferenceSystem::CheckShadowFacts()
    {
        // 前提（都在采样窗口后成立）：台架已 SyncForParityReadback ⇒ gather 输出
        // = 本帧真值。值链 = CPU 访问器(真值) == EntityGPU 行(collect 归一上行)
        // == gather mtl 行(GPU 照抄) ⇒ 三段逐字节相等才算接线成立。
        auto gpu_cull = context_->GetSystem<GPUCullSystem>();
        auto* mtl_buf = gpu_cull->GetMtlOutBuffer();
        if (!mtl_buf || gpu_count_ == 0)
            return;

        const auto& entity_table = context_->GetEntityGPUTable();

        const uint32_t read_count = (gpu_count_ < kMaxVisible) ? gpu_count_ : kMaxVisible;

        const auto* mtl_rows = static_cast<const graph::mtl::MaterialInstanceAddresses *>(
            mtl_buf->Map(0, static_cast<VkDeviceSize>(read_count)
                                * sizeof(graph::mtl::MaterialInstanceAddresses)));
        if (!mtl_rows)
            return;

        // index → owner 指针表（采样窗口一次，容量同实体表）：**不**用裸 EntityID
        // 造 index-only 句柄——generation 不真 ⇒ IsValidID 判悬垂会失真；
        // ForEachAliveEntity 只给存活实体，天然安全。
        const Entity *owners[kMaxVisible] = {};
        context_->ForEachAliveEntity([&owners](const Entity *e)
        {
            const uint32_t idx = e->GetEntityID().index;
            if (idx < kMaxVisible)
                owners[idx] = e;
        });

        uint32_t checked = 0, bad = 0;
        uint32_t nondefault_seen = 0;   // 判据强度：非默认实体真进链才算"测到了"
        for (uint32_t k = 0; k < read_count; ++k)
        {
            const uint32_t entity_index = gpu_visible_order_[k];

            const EntityGPU *row = entity_table.Row(entity_index);
            if (!row)
                continue;

            // 段一：CPU 真值（行字节归一式 = 唯一出处，与 collect/FinalizeBatch 同函数）
            const Entity *owner = owners[entity_index];
            const float bias_for_row =
                NormalizeShadowBiasToRow(GetShadowBiasMultiplier(owner));
            const uint32_t flags_for_row = CanReceiveShadow(owner) ? 0u
                : uint32_t(graph::mtl::kMaterialShadowFlagNoReceive);

            // 真非默认 = 与缺省约定（receive、bias 1.0）不同；未挂载实体 accessor
            // 恒返回缺省 ⇒ 不混进本日志。bias 归一为 0（显式非法值）与 CPU 同字节
            // 处理，也算显式配置差异，如实列出。
            const bool nondefault = (flags_for_row != 0u) || (bias_for_row != 1.0f);
            if (nondefault)
            {
                if (nondefault_seen < 8)
                    GLogInfo(u8"[CPUCullReferenceSystem] shadow non-default: idx=%u"
                             u8" bias=%.3f flags=%u（三段字节=%.3f/%.3f/%.3f"
                             u8" %u/%u/%u）",
                             entity_index, bias_for_row, flags_for_row,
                             bias_for_row, row->shadow_bias_multiplier,
                             mtl_rows[k].shadow_bias_multiplier,
                             flags_for_row, row->shadow_receive,
                             mtl_rows[k].shadow_flags);
                ++nondefault_seen;
            }

            // 段二（实体行）+ 段三（gather 行）：逐字节比
            if (row->shadow_bias_multiplier != bias_for_row
             || (row->shadow_receive == 0u ? graph::mtl::kMaterialShadowFlagNoReceive : 0u)
             != flags_for_row
             || mtl_rows[k].shadow_bias_multiplier != bias_for_row
             || mtl_rows[k].shadow_flags != flags_for_row)
            {
                if (bad < 5)
                    GLogWarning(u8"[CPUCullReferenceSystem] shadow facts chain broken:"
                                u8" idx=%u bias(accessor/row/gather)=%.3f/%.3f/%.3f"
                                u8" flags(receive-bit/row/gather)=%u/%u/%u",
                                entity_index,
                                bias_for_row, row->shadow_bias_multiplier,
                                mtl_rows[k].shadow_bias_multiplier,
                                flags_for_row,
                                row->shadow_receive == 0u ? 1u : 0u,
                                mtl_rows[k].shadow_flags);
                ++bad;
            }
            ++checked;
        }

        mtl_buf->Unmap();

        // checked>0 且 bad=0 ⇒ 值链对每个幸存实体成立（非默认实体由测试挂载点
        // 注入，其存在性在示例日志自证；形态 A 已退役 ⇒ 只有接管帧有 gather 行，
        // 本判据天然只在形态 B 采样窗口跑）。
        if (bad == 0)
            GLogInfo(u8"[CPUCullReferenceSystem] Shadow facts value-chain: %u/%u 实体三段一致",
                     checked, checked);
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
            const graph::RenderItemDescriptor *ids = render_ids.Row(i);

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

            // 计算变换后的 extents（简化：取最大缩放 = 与 GLSL 逐字同式）
            const float scale_x = glm::length(glm::vec3(world_matrix[0]));
            const float scale_y = glm::length(glm::vec3(world_matrix[1]));
            const float scale_z = glm::length(glm::vec3(world_matrix[2]));
            const float max_scale = std::max({scale_x, scale_y, scale_z});
            const glm::vec3 world_extents(aabb->extents[0] * max_scale, aabb->extents[1] * max_scale, aabb->extents[2] * max_scale);

            // 视锥剔除（6 个平面）
            bool visible = true;
            for (int p = 0; p < 6; ++p)
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
                gpu_visible_order_[k] = idx;   // 保序供值链判据（slot k ↔ gather 行 k）
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
        last_parity_stats_ = ParityStats{};
        last_parity_stats_.cpu_count = cpu_count_;
        last_parity_stats_.gpu_count = gpu_count_;

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

        last_parity_stats_.match_count = both;
        last_parity_stats_.mismatch_count = mismatch;

        GLogInfo(u8"[CPUCullReferenceSystem] Parity: CPU=%u, GPU=%u, match=%u, mismatch=%u",
                 last_parity_stats_.cpu_count,
                 last_parity_stats_.gpu_count,
                 last_parity_stats_.match_count,
                 last_parity_stats_.mismatch_count);
    }

    uint32_t CPUCullReferenceSystem::GetGPUVisibleCount() const
    {
        // 台架读数 = 最近一次采样窗口的同步读回结果，**不**现读 GPU
        //（现读者须自行先 SyncForParityReadback；旧版这里直接 Map 异步表 = 残影）。
        return gpu_count_;
    }
}
