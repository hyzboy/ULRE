#include <hgl/ecs/support/CPUCullReferenceSystem.h>
#include <hgl/ecs/support/GPUCullSystem.h>
#include <hgl/ecs/core/EntityGPU.h>
#include <hgl/ecs/support/EntityGPUTable.h>
#include <hgl/ecs/support/TransformDataStorage.h>
#include <hgl/ecs/support/TransformAssignmentBuffer.h>
#include <hgl/ecs/systems/tick/TransformSystem.h>
#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/ShaderBufferSources.h>
#include <hgl/log/Log.h>
#include <set>
#include <algorithm>

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
        if (!context_)
            return;

        // opt-in 门：GPU 剔除管线未启用 ⇒ 对拍没有对照物，整段跳过
        // （本系统的 Update 会 Map 整块 L2W SSBO，白跑一次全量遍历）。
        auto gpu_cull = context_->GetSystem<GPUCullSystem>();
        if (!gpu_cull || !gpu_cull->IsEnabled())
            return;

        // 1. 执行 CPU 剔除
        PerformCPUCulling();

        // 2. 读取 GPU 结果
        ReadGPUVisibleIndices();

        // 3. 对比并打印统计
        CompareResults();
    }

    void CPUCullReferenceSystem::PerformCPUCulling()
    {
        cpu_visible_indices_.clear();

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
        for (uint32_t i = 0; i < entity_count; ++i)
        {
            const EntityGPU* row = entity_table.Row(i);
            if (!row) continue;

            // 跳过非活跃实体
            if (!EntityGPUTable::IsActive(*row)) continue;

            // 跳过不可见实体
            if (!EntityGPUTable::IsVisible(*row)) continue;

            // 获取 L2W 行号和 Geometry 行号
            const uint32_t l2w_row = row->row[static_cast<uint32_t>(ComponentType::Transform)];
            const uint32_t geometry_id = row->row[static_cast<uint32_t>(ComponentType::Geometry)];

            if (l2w_row == 0 || geometry_id == 0) continue;

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

            if (visible)
            {
                cpu_visible_indices_.push_back(i);
            }
        }

        // Unmap L2W buffer
        l2w_device_buffer->Unmap();
    }

    void CPUCullReferenceSystem::ReadGPUVisibleIndices()
    {
        gpu_visible_indices_.clear();

        // 从 GPUCullSystem 获取 GPU 结果
        auto gpu_cull_system = context_->GetSystem<GPUCullSystem>();
        if (!gpu_cull_system) return;

        const uint32_t gpu_count = gpu_cull_system->GetGPUVisibleCount();
        if (gpu_count == 0) return;

        // 读取 visible_indices buffer
        auto* buffer = gpu_cull_system->GetVisibleIndicesBuffer();
        if (!buffer) return;

        void* mapped = buffer->Map(0, gpu_count * sizeof(uint32_t));
        if (mapped)
        {
            const uint32_t* indices = static_cast<const uint32_t*>(mapped);
            gpu_visible_indices_.assign(indices, indices + gpu_count);
            buffer->Unmap();
        }
    }

    void CPUCullReferenceSystem::CompareResults()
    {
        last_parity_stats_ = ParityStats{};
        last_parity_stats_.cpu_count = static_cast<uint32_t>(cpu_visible_indices_.size());
        last_parity_stats_.gpu_count = static_cast<uint32_t>(gpu_visible_indices_.size());

        // 使用集合对比（不关心顺序）
        std::set<uint32_t> cpu_set(cpu_visible_indices_.begin(), cpu_visible_indices_.end());
        std::set<uint32_t> gpu_set(gpu_visible_indices_.begin(), gpu_visible_indices_.end());

        // 计算交集
        std::set<uint32_t> intersection;
        std::set_intersection(
            cpu_set.begin(), cpu_set.end(),
            gpu_set.begin(), gpu_set.end(),
            std::inserter(intersection, intersection.begin())
        );

        last_parity_stats_.match_count = static_cast<uint32_t>(intersection.size());
        last_parity_stats_.mismatch_count = (last_parity_stats_.cpu_count + last_parity_stats_.gpu_count)
                                           - 2 * last_parity_stats_.match_count;

        // 每 60 帧打印一次统计
        static uint32_t frame_counter = 0;
        if (++frame_counter % 60 == 0)
        {
            GLogInfo(u8"[CPUCullReferenceSystem] Parity: CPU=%u, GPU=%u, match=%u, mismatch=%u",
                     last_parity_stats_.cpu_count,
                     last_parity_stats_.gpu_count,
                     last_parity_stats_.match_count,
                     last_parity_stats_.mismatch_count);

            if (last_parity_stats_.mismatch_count > 0)
            {
                // 打印前 5 个不匹配的实体
                std::vector<uint32_t> cpu_only, gpu_only;
                std::set_difference(cpu_set.begin(), cpu_set.end(),
                                   gpu_set.begin(), gpu_set.end(),
                                   std::back_inserter(cpu_only));
                std::set_difference(gpu_set.begin(), gpu_set.end(),
                                   cpu_set.begin(), cpu_set.end(),
                                   std::back_inserter(gpu_only));

                GLogWarning(u8"[CPUCullReferenceSystem] First 5 CPU-only: %u,%u,%u,%u,%u",
                           cpu_only.size() > 0 ? cpu_only[0] : 0,
                           cpu_only.size() > 1 ? cpu_only[1] : 0,
                           cpu_only.size() > 2 ? cpu_only[2] : 0,
                           cpu_only.size() > 3 ? cpu_only[3] : 0,
                           cpu_only.size() > 4 ? cpu_only[4] : 0);

                GLogWarning(u8"[CPUCullReferenceSystem] First 5 GPU-only: %u,%u,%u,%u,%u",
                           gpu_only.size() > 0 ? gpu_only[0] : 0,
                           gpu_only.size() > 1 ? gpu_only[1] : 0,
                           gpu_only.size() > 2 ? gpu_only[2] : 0,
                           gpu_only.size() > 3 ? gpu_only[3] : 0,
                           gpu_only.size() > 4 ? gpu_only[4] : 0);
            }
        }
    }

    uint32_t CPUCullReferenceSystem::GetGPUVisibleCount() const
    {
        auto gpu_cull_system = context_->GetSystem<GPUCullSystem>();
        return gpu_cull_system ? gpu_cull_system->GetGPUVisibleCount() : 0;
    }
}
