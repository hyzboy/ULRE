/**
 * TransformAssignmentBuffer.cpp - ECS Transform 分配缓冲实现
 */

#include<hgl/common/RenderOptions.h>
#include<hgl/ecs/support/TransformAssignmentBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/graph/ShaderBufferSources.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/log/Log.h>
#include<algorithm>
#include<limits>
#include<cstdint>

namespace hgl::ecs
{
    namespace
    {
        constexpr uint32_t kIdentityL2WSlot = 0;
        constexpr uint32_t kFirstObjectL2WSlot = 1;

        // 2026-10-10（审计 §1.1）：`ShouldEmitPeriodicLog`（恒 false、body 已注释）与其唯一调用点
        // （EnsureBudget 里的死分支）一并删除；随它走的 `LogDeviceBufferSnapshot` 也成了零调用者 ⇒ 同删。
        // 需要时按 GLogInfo 的"budget:"行/设备快照重建即可（本轮不加回）。

        static bool WriteIdentityToL2WSlot0(graph::DeviceBuffer *buffer)
        {
            if (!buffer)
                return false;

            auto *gpu = buffer->GetGPUBuffer();
            if (!gpu)
                return false;

            const math::Matrix4f identity = math::Identity4f;
            const VkDeviceSize byte_offset = sizeof(math::Matrix4f) * kIdentityL2WSlot;
            const VkDeviceSize byte_size = sizeof(math::Matrix4f);

            if (!gpu->Write(&identity, byte_offset, byte_size))
                return false;

            graph::IGPUBuffer::DirtyRange range{ byte_offset, byte_size };
            buffer->FlushRanges(&range, 1);
            return true;
        }

        struct IndexRange
        {
            uint32_t first = 0;
            uint32_t last = 0;
        };

        static std::vector<IndexRange> BuildMergedRangesFromIndices(const std::vector<uint32_t>& dirty_indices,
                                                                     const uint32_t handle_count)
        {
            std::vector<IndexRange> result;
            if (dirty_indices.empty() || handle_count == 0)
                return result;

            std::vector<uint32_t> sorted = dirty_indices;
            std::sort(sorted.begin(), sorted.end());
            sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());

            uint32_t run_start = std::numeric_limits<uint32_t>::max();
            uint32_t run_last = std::numeric_limits<uint32_t>::max();

            for (const uint32_t idx : sorted)
            {
                if (idx >= handle_count)
                    continue;

                if (run_start == std::numeric_limits<uint32_t>::max())
                {
                    run_start = idx;
                    run_last = idx;
                    continue;
                }

                if (idx == run_last + 1)
                {
                    run_last = idx;
                    continue;
                }

                IndexRange range;
                range.first = run_start;
                range.last = run_last;
                result.push_back(range);

                run_start = idx;
                run_last = idx;
            }

            if (run_start != std::numeric_limits<uint32_t>::max())
            {
                IndexRange range;
                range.first = run_start;
                range.last = run_last;
                result.push_back(range);
            }

            return result;
        }

        static void RebuildTrackedDirtyRanges(graph::IGPUBuffer *gpu_buffer,
                                              const std::vector<graph::IGPUBuffer::DirtyRange> &ranges)
        {
            if (!gpu_buffer || ranges.empty())
                return;

            gpu_buffer->ClearDirty();
            gpu_buffer->MarkDirtyRanges(ranges.data(), ranges.size());
        }
    }

    TransformAssignmentBuffer::TransformAssignmentBuffer(graph::BufferManager* bm,
                                                         const uint32_t ring_frames,
                                                         const uint32_t max_static_rows,
                                                         const uint32_t max_dynamic_rows)
        : MaxTransformCount(0)
        , buffer_manager(bm)
        , max_static_rows(max_static_rows)
        , max_dynamic_rows(max_dynamic_rows)
        , transform_buffer_max_count(0)
        , transform_buffer(nullptr)
        , ring_layout(ring_frames ? ring_frames : HGL_L2W_RING_FRAMES)
    {
        if (buffer_manager)
        {
            auto device = buffer_manager->GetDevice();
            if (device)
                MaxTransformCount = device->GetSSBORange() / sizeof(math::Matrix4f);
        }
    }

    void TransformAssignmentBuffer::EnsureBudget(const uint32_t static_count,const uint32_t dynamic_count)
    {
        if (!AllocateBudgetBuffers())
            return;

        if (static_count > max_static_rows || dynamic_count > max_dynamic_rows)
        {
            if (!budget_warned)
            {
                budget_warned = true;
                GLogError("[TransformAssignmentBuffer] 变换预算超限：static=%u/%u dynamic=%u/%u"
                          " —— 预算没调够（行号已移动、固化 ID/存档失效）；本帧**不扩容**，超出部分不写入 L2W。",
                          static_count, max_static_rows, dynamic_count, max_dynamic_rows);
            }
        }

        // 索引行表已整块删除（B1b-2：全局 `ECS:LocalToWorldIndex` 只写不读）。
        // 接管路径（形态 B）的 per-batch `l2w_index` 表由 **GPU 剔除**写（`entity_gpu_cull.glsl`），
        // 行号来自实体行 `row[Transform]`；渲染项上曾有一份 CPU 侧副本，2026-10-14（A5）随"只写不读"删除。

    }

    uint32_t TransformAssignmentBuffer::GetDynamicBaseIndex() const
    {
        // B1b：动态段基址 = 静态段**预算** + 帧号 × 动态段**预算**（与固定容量同源 ⇒ 行号不再漂移）
        return ring_layout.GetBaseIndex(max_static_rows + kFirstObjectL2WSlot, max_dynamic_rows);
    }






    void TransformAssignmentBuffer::WriteDirtyIndicesInternal(const char* label,
                                                              uint32_t base_index,
                                                              const TransformDataStorage& storage,
                                                              const std::vector<TransformDataStorage::HandleID>& handles,
                                                              const std::vector<uint32_t>& dirty_indices)
    {
        if (!transform_buffer || handles.empty() || dirty_indices.empty())
            return;

        auto *tbuf = transform_buffer->GetGPUBuffer();
        if (!tbuf)
            return;

        const auto merged_ranges = BuildMergedRangesFromIndices(dirty_indices, static_cast<uint32_t>(handles.size()));
        if (merged_ranges.empty())
            return;

        uint32_t min_first = std::numeric_limits<uint32_t>::max();
        uint32_t max_last = 0;
        for (const auto &range : merged_ranges)
        {
            const uint32_t first = static_cast<uint32_t>(range.first);
            const uint32_t last = static_cast<uint32_t>(range.last);
            min_first = hgl_min(min_first, first);
            max_last = hgl_max(max_last, last);
        }

        if (min_first == std::numeric_limits<uint32_t>::max() || max_last < min_first)
            return;

        const VkDeviceSize span_logical_first = static_cast<VkDeviceSize>(base_index + min_first);
        const VkDeviceSize span_matrix_count = static_cast<VkDeviceSize>(max_last - min_first + 1);
        if (span_logical_first + span_matrix_count > transform_buffer_max_count)
        {
            GLogError("[TransformAssignmentBuffer] %s L2W 越界拦截：span_end=%llu > max_count=%u",
                      label,
                      static_cast<unsigned long long>(span_logical_first + span_matrix_count),
                      transform_buffer_max_count);
            return;
        }
        const VkDeviceSize span_offset_bytes = sizeof(math::Matrix4f) * span_logical_first;
        const VkDeviceSize span_size_bytes = sizeof(math::Matrix4f) * span_matrix_count;

        math::Matrix4f *mapped = static_cast<math::Matrix4f *>(tbuf->Map(span_offset_bytes, span_size_bytes));
        if (!mapped)
        {
            GLogWarning("[TransformAssignmentBuffer] %s L2W map failed: map_offset=%llu map_size=%llu handles=%u dirty_indices=%u",
                        label,
                        static_cast<unsigned long long>(span_offset_bytes),
                        static_cast<unsigned long long>(span_size_bytes),
                        static_cast<uint32_t>(handles.size()),
                        static_cast<uint32_t>(dirty_indices.size()));
            return;
        }

        std::vector<graph::IGPUBuffer::DirtyRange> flush_ranges;
        flush_ranges.reserve(merged_ranges.size());

        for (const auto &range : merged_ranges)
        {
            const uint32_t first = static_cast<uint32_t>(range.first);
            const uint32_t last = static_cast<uint32_t>(range.last);
            const uint32_t count = last - first + 1;
            const VkDeviceSize logical_index = static_cast<VkDeviceSize>(base_index + first);
            const VkDeviceSize byte_offset = sizeof(math::Matrix4f) * logical_index;
            const VkDeviceSize byte_size = sizeof(math::Matrix4f) * static_cast<VkDeviceSize>(count);

            math::Matrix4f *dst = mapped + (logical_index - span_logical_first);
            for (uint32_t i = 0; i < count; ++i)
            {
                const auto handle = handles[first + i];

                // 槽位表含空洞 ⇒ 空洞行不写（保持上次内容）
                if (handle == TransformDataStorage::INVALID_HANDLE)
                    continue;

                const glm::mat4 world_matrix = storage.GetWorldMatrix(handle);
                dst[i] = *reinterpret_cast<const math::Matrix4f*>(&world_matrix);
            }

            flush_ranges.push_back({byte_offset, byte_size});
        }

        tbuf->Unmap();

        if (!flush_ranges.empty())
        {
            RebuildTrackedDirtyRanges(tbuf, flush_ranges);
        }
    }

    void TransformAssignmentBuffer::WriteStaticDirtyIndices(const TransformDataStorage& storage,
                                                            const std::vector<TransformDataStorage::HandleID>& handles,
                                                            const std::vector<uint32_t>& dirty_indices)
    {
        WriteDirtyIndicesInternal("Static", kFirstObjectL2WSlot, storage, handles, dirty_indices);
    }

    void TransformAssignmentBuffer::WriteDynamicDirtyIndices(const TransformDataStorage& storage,
                                                             const std::vector<TransformDataStorage::HandleID>& handles,
                                                             const std::vector<uint32_t>& dirty_indices)
    {
        WriteDirtyIndicesInternal("Dynamic", GetDynamicBaseIndex(), storage, handles, dirty_indices);
    }

    void TransformAssignmentBuffer::Clear()
    {
        if (buffer_manager)
        {
            if (transform_buffer)
                buffer_manager->Release(transform_buffer);
        }
        else
        {
            SAFE_CLEAR(transform_buffer);
        }

        transform_buffer = nullptr;

        transform_buffer_max_count = 0;
    }

    bool TransformAssignmentBuffer::AllocateBudgetBuffers()
    {
        if (transform_buffer)
            return true;

        if (!buffer_manager)
        {
            GLogError("[TransformAssignmentBuffer] 预算分配失败：buffer_manager 为空（static=%u dynamic=%u）",
                      max_static_rows, max_dynamic_rows);
            return false;
        }

        uint32_t row_count = ring_layout.GetTotalCount(max_static_rows + kFirstObjectL2WSlot, max_dynamic_rows);

        if (MaxTransformCount && row_count > MaxTransformCount)
        {
            GLogError("[TransformAssignmentBuffer] 声明预算超过设备 SSBO 上限：需要 %u 行 > 设备上限 %u 行"
                      " —— 拒绝分配（请调小预算），不截断以防行号算术越界写。",
                      row_count, MaxTransformCount);
            return false;
        }

        transform_buffer_max_count = row_count;
        transform_buffer = buffer_manager->CreateSSBO("ECS:LocalToWorld",
                                                      sizeof(math::Matrix4f) * transform_buffer_max_count,
                                                      nullptr,
                                                      graph::SharingMode::Exclusive);

        if (!transform_buffer)
        {
            GLogError("[TransformAssignmentBuffer] 预算期分配失败：L2W rows=%u（static=%u + dynamic=%u）",
                      row_count, max_static_rows, max_dynamic_rows);
            return false;
        }

        if (!WriteIdentityToL2WSlot0(transform_buffer))
        {
            GLogError("[TransformAssignmentBuffer] failed to initialize identity matrix at L2W slot 0");
        }

        GLogInfo("[TransformAssignmentBuffer] 预算期一次性分配完成：L2W rows=%u（static=%u + dynamic=%u）bytes=%llu",
                 transform_buffer_max_count,
                 max_static_rows,
                 max_dynamic_rows,
                 static_cast<unsigned long long>(transform_buffer->GetSize()));
        return true;
    }



    void TransformAssignmentBuffer::SetFrameIndex(const uint32_t index)
    {
        ring_layout.SetFrameIndex(index);
    }
}//namespace hgl::ecs
