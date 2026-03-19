/**
 * TransformAssignmentBuffer.cpp - ECS Transform 分配缓冲实现
 */

#include<hgl/common/RenderOptions.h>
#include<hgl/ecs/support/TransformAssignmentBuffer.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKRenderAssign.h>
#include<hgl/vk/VKMaterial.h>
#include<hgl/mtl/UBOCommon.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/log/Log.h>
#include<algorithm>
#include<limits>
#include<iostream>
#include<cstdint>
#include<cstdio>

namespace hgl::ecs
{
    namespace
    {
        constexpr uint32_t kIdentityL2WSlot = 0;
        constexpr uint32_t kFirstObjectL2WSlot = 1;

        static bool ShouldEmitPeriodicLog(const uint32_t period = 120)
        {
            static uint32_t tick = 0;
            ++tick;
            return (tick % period) == 1;
        }

        static void LogDeviceBufferSnapshot(const char *tag, const graph::DeviceBuffer *buffer)
        {
            if (!tag)
                tag = "[TransformAssignmentBuffer]";

            if (!buffer)
            {
                GLogWarning("%s buffer=null", tag);
                return;
            }

            const auto *gpu = buffer->GetGPUBuffer();
            GLogInfo("%s buffer=0x%llX vk=0x%llX size=%llu gpu=0x%llX dirty=%d",
                     tag,
                     static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(buffer)),
                     static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(buffer->GetBuffer())),
                     static_cast<unsigned long long>(buffer->GetSize()),
                     static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(gpu)),
                     gpu ? (gpu->IsDirty() ? 1 : 0) : -1);
        }

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

    std::vector<TransformAssignmentBuffer*> TransformAssignmentBuffer::all_instances;

    TransformAssignmentBuffer::TransformAssignmentBuffer(graph::BufferManager* bm, const Mode m, uint32_t ring_frames)
        : MaxTransformCount(0)
        , buffer_manager(bm)
        , transform_buffer_max_count(0)
        , transform_buffer(nullptr)
        , transform_policy(graph::BufferAllocPolicy::Auto)
        , static_only(false)
        , mode(m)
        , transform_id_buffer_max_count(0)
        , transform_id_buffer(nullptr)
        , transform_id_vk_buffer(VK_NULL_HANDLE)
        , ring_writer(nullptr, sizeof(math::Matrix4f), ring_frames ? ring_frames : HGL_L2W_RING_FRAMES)
    {
        if (buffer_manager)
        {
            auto device = buffer_manager->GetDevice();
            if (device)
                MaxTransformCount = device->GetSSBORange() / sizeof(math::Matrix4f);
        }
        all_instances.push_back(this);
    }

    void TransformAssignmentBuffer::BindTransform(graph::Material* mtl) const
    {
        if (!mtl)
        {
            std::cout << "[TransformAssignmentBuffer::BindTransform] WARNING: Material is null" << std::endl;
            return;
        }

        if (!transform_buffer)
        {
            std::cout << "[TransformAssignmentBuffer::BindTransform] WARNING: Transform buffer not created" << std::endl;
            return;
        }

        LogDeviceBufferSnapshot("[TransformAssignmentBuffer::BindTransform] before bind", transform_buffer);

        mtl->BindSSBO(hgl::graph::mtl::SBS_LocalToWorld.set_type,
                  hgl::graph::mtl::SBS_LocalToWorld.name,
                  transform_buffer->GetGPUBuffer());
        GLogInfo("[TransformAssignmentBuffer::BindTransform] BindSSBO set_type=%d name=%s",
                 static_cast<int>(hgl::graph::mtl::SBS_LocalToWorld.set_type),
                 hgl::graph::mtl::SBS_LocalToWorld.name);

        BindTransformID(mtl);
    }

    void TransformAssignmentBuffer::BindTransformID(graph::Material* mtl) const
    {
        if (!mtl)
            return;

        if (!transform_id_buffer)
            return;

        auto *gpu = transform_id_buffer->GetGPUBuffer();
        if (!gpu)
            return;

        mtl->BindSSBO(hgl::graph::mtl::SBS_TransformID.set_type,
                      hgl::graph::mtl::SBS_TransformID.name,
                      gpu);
    }

    void TransformAssignmentBuffer::EnsureCapacity(const uint32_t static_count,const uint32_t dynamic_count,graph::BufferAllocPolicy policy)
    {
        const uint32_t total_count = ring_writer.GetTotalCount(static_count + kFirstObjectL2WSlot, dynamic_count);
        StatTransform(total_count, policy);

        if (ShouldEmitPeriodicLog())
        {
            const uint32_t ring_base = ring_writer.GetBaseIndex(static_count + kFirstObjectL2WSlot, dynamic_count);
            GLogInfo("[TransformAssignmentBuffer] EnsureCapacity: static=%u dynamic=%u total=%u ring_base=%u frame=%u policy=%d",
                     static_count,
                     dynamic_count,
                     total_count,
                     ring_base,
                     ring_writer.GetFrameIndex(),
                     static_cast<int>(policy));
            LogDeviceBufferSnapshot("[TransformAssignmentBuffer] EnsureCapacity snapshot", transform_buffer);
        }
    }

    uint32_t TransformAssignmentBuffer::GetDynamicBaseIndex(const uint32_t static_count,const uint32_t dynamic_count) const
    {
        return ring_writer.GetBaseIndex(static_count + kFirstObjectL2WSlot, dynamic_count);
    }

    uint32_t TransformAssignmentBuffer::GetTotalCount(const uint32_t static_count,const uint32_t dynamic_count) const
    {
        return ring_writer.GetTotalCount(static_count + kFirstObjectL2WSlot, dynamic_count);
    }

    void TransformAssignmentBuffer::WriteStaticFromStorage(const TransformDataStorage& storage,const uint32_t static_count)
    {
        if (!transform_buffer || static_count == 0)
            return;

        const auto &mats = storage.GetAllWorldMatrices();
        const uint32_t write_count = static_cast<uint32_t>(mats.size() < static_count ? mats.size() : static_count);
        if (write_count == 0)
            return;

        const VkDeviceSize map_offset = sizeof(math::Matrix4f) * kFirstObjectL2WSlot;
        const VkDeviceSize map_size = sizeof(math::Matrix4f) * write_count;
        auto *tbuf = transform_buffer->GetGPUBuffer();
        math::Matrix4f* l2wp = tbuf ? (math::Matrix4f*)tbuf->Map(map_offset, map_size) : nullptr;
        if (!l2wp)
            return;

        for (uint32_t i = 0; i < write_count; ++i)
        {
            l2wp[i] = *reinterpret_cast<const math::Matrix4f*>(&mats[i]);
            ApplyCameraRelativeOffset(l2wp[i]);
        }

        if(tbuf) tbuf->Unmap();
    }

    void TransformAssignmentBuffer::WriteDynamicFromStorage(const TransformDataStorage& storage,const uint32_t static_count,const uint32_t dynamic_count)
    {
        if (!transform_buffer || dynamic_count == 0)
            return;

        const auto &mats = storage.GetAllWorldMatrices();
        const uint32_t write_count = static_cast<uint32_t>(mats.size() < dynamic_count ? mats.size() : dynamic_count);
        if (write_count == 0)
            return;

        math::Matrix4f* l2wp = (math::Matrix4f*)(ring_writer.MapDynamicRange(static_count + kFirstObjectL2WSlot, dynamic_count));
        if (!l2wp)
            return;

        for (uint32_t i = 0; i < write_count; ++i)
        {
            l2wp[i] = *reinterpret_cast<const math::Matrix4f*>(&mats[i]);
            ApplyCameraRelativeOffset(l2wp[i]);
        }

        ring_writer.Unmap();
    }

    void TransformAssignmentBuffer::WriteStaticFromHandles(const TransformDataStorage& storage,
                                                              const std::vector<TransformDataStorage::HandleID>& handles)
    {
        const uint32_t write_count = static_cast<uint32_t>(handles.size());
        if (!transform_buffer || write_count == 0)
            return;

        const VkDeviceSize map_offset = sizeof(math::Matrix4f) * kFirstObjectL2WSlot;
        const VkDeviceSize map_size = sizeof(math::Matrix4f) * write_count;
        auto *tbuf = transform_buffer->GetGPUBuffer();
        math::Matrix4f* l2wp = tbuf ? (math::Matrix4f*)tbuf->Map(map_offset, map_size) : nullptr;
        if (!l2wp)
            return;

        for (uint32_t i = 0; i < write_count; ++i)
        {
            const auto handle = handles[i];
            const glm::mat4 world_matrix = storage.GetWorldMatrix(handle);
            l2wp[i] = *reinterpret_cast<const math::Matrix4f*>(&world_matrix);
            ApplyCameraRelativeOffset(l2wp[i]);
        }

        if(tbuf) tbuf->Unmap();
    }

    void TransformAssignmentBuffer::WriteDynamicFromHandles(const TransformDataStorage& storage,
                                                               const uint32_t static_count,
                                                               const std::vector<TransformDataStorage::HandleID>& handles)
    {
        const uint32_t write_count = static_cast<uint32_t>(handles.size());
        if (!transform_buffer || write_count == 0)
            return;

        math::Matrix4f* l2wp = (math::Matrix4f*)(ring_writer.MapDynamicRange(static_count + kFirstObjectL2WSlot, write_count));
        if (!l2wp)
            return;

        for (uint32_t i = 0; i < write_count; ++i)
        {
            const auto handle = handles[i];
            const glm::mat4 world_matrix = storage.GetWorldMatrix(handle);
            l2wp[i] = *reinterpret_cast<const math::Matrix4f*>(&world_matrix);
            ApplyCameraRelativeOffset(l2wp[i]);
        }

        ring_writer.Unmap();
    }

    void TransformAssignmentBuffer::WriteStaticDirtyIndices(const TransformDataStorage& storage,
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

        const VkDeviceSize span_logical_first = static_cast<VkDeviceSize>(kFirstObjectL2WSlot + min_first);
        const VkDeviceSize span_matrix_count = static_cast<VkDeviceSize>(max_last - min_first + 1);
        const VkDeviceSize span_offset_bytes = sizeof(math::Matrix4f) * span_logical_first;
        const VkDeviceSize span_size_bytes = sizeof(math::Matrix4f) * span_matrix_count;

        math::Matrix4f *mapped = static_cast<math::Matrix4f *>(tbuf->Map(span_offset_bytes, span_size_bytes));
        if (!mapped)
        {
            GLogWarning("[TransformAssignmentBuffer] Static L2W map failed: map_offset=%llu map_size=%llu handles=%u dirty_indices=%u",
                        static_cast<unsigned long long>(span_offset_bytes),
                        static_cast<unsigned long long>(span_size_bytes),
                        static_cast<uint32_t>(handles.size()),
                        static_cast<uint32_t>(dirty_indices.size()));
            return;
        }

        std::vector<graph::IGPUBuffer::DirtyRange> flush_ranges;
        flush_ranges.reserve(merged_ranges.size());
        VkDeviceSize total_written_bytes = 0;

        for (const auto &range : merged_ranges)
        {
            const uint32_t first = static_cast<uint32_t>(range.first);
            const uint32_t last = static_cast<uint32_t>(range.last);
            const uint32_t count = last - first + 1;
            const VkDeviceSize logical_index = static_cast<VkDeviceSize>(kFirstObjectL2WSlot + first);
            const VkDeviceSize byte_offset = sizeof(math::Matrix4f) * logical_index;
            const VkDeviceSize byte_size = sizeof(math::Matrix4f) * static_cast<VkDeviceSize>(count);

            math::Matrix4f *dst = mapped + (logical_index - span_logical_first);
            for (uint32_t i = 0; i < count; ++i)
            {
                const auto handle = handles[first + i];
                const glm::mat4 world_matrix = storage.GetWorldMatrix(handle);
                dst[i] = *reinterpret_cast<const math::Matrix4f*>(&world_matrix);
                ApplyCameraRelativeOffset(dst[i]);
            }

            flush_ranges.push_back({byte_offset, byte_size});
            total_written_bytes += byte_size;
        }

        tbuf->Unmap();

        if (!flush_ranges.empty())
        {
            RebuildTrackedDirtyRanges(tbuf, flush_ranges);

            GLogInfo("[TransformAssignmentBuffer] Static L2W flush: ranges=%u dirty_indices=%u bytes=%llu buffer_dirty=%d",
                      static_cast<uint32_t>(flush_ranges.size()),
                      static_cast<uint32_t>(dirty_indices.size()),
                      static_cast<unsigned long long>(total_written_bytes),
                      tbuf->IsDirty() ? 1 : 0);

            std::fprintf(stderr,
                         "[TransformAssignmentBuffer] Static L2W flush: ranges=%u dirty_indices=%u bytes=%llu buffer_dirty=%d\n",
                         static_cast<uint32_t>(flush_ranges.size()),
                         static_cast<uint32_t>(dirty_indices.size()),
                         static_cast<unsigned long long>(total_written_bytes),
                         tbuf->IsDirty() ? 1 : 0);

            if (ShouldEmitPeriodicLog(60))
            {
                GLogInfo("[TransformAssignmentBuffer] Static L2W detail: min=%u max=%u span_offset=%llu span_size=%llu",
                         min_first,
                         max_last,
                         static_cast<unsigned long long>(span_offset_bytes),
                         static_cast<unsigned long long>(span_size_bytes));
            }
        }
    }

    void TransformAssignmentBuffer::WriteDynamicDirtyIndices(const TransformDataStorage& storage,
                                                             const uint32_t static_count,
                                                             const std::vector<TransformDataStorage::HandleID>& handles,
                                                             const std::vector<uint32_t>& dirty_indices)
    {
        if (!transform_buffer || handles.empty() || dirty_indices.empty())
            return;

        auto *tbuf = transform_buffer->GetGPUBuffer();
        if (!tbuf)
            return;

        const uint32_t dynamic_count = static_cast<uint32_t>(handles.size());
        const uint32_t base_index = ring_writer.GetBaseIndex(static_count + kFirstObjectL2WSlot, dynamic_count);

        const auto merged_ranges = BuildMergedRangesFromIndices(dirty_indices, dynamic_count);
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
        const VkDeviceSize span_offset_bytes = sizeof(math::Matrix4f) * span_logical_first;
        const VkDeviceSize span_size_bytes = sizeof(math::Matrix4f) * span_matrix_count;

        math::Matrix4f *mapped = static_cast<math::Matrix4f *>(tbuf->Map(span_offset_bytes, span_size_bytes));
        if (!mapped)
        {
            GLogWarning("[TransformAssignmentBuffer] Dynamic L2W map failed: map_offset=%llu map_size=%llu static_count=%u dynamic_count=%u dirty_indices=%u frame=%u",
                        static_cast<unsigned long long>(span_offset_bytes),
                        static_cast<unsigned long long>(span_size_bytes),
                        static_count,
                        dynamic_count,
                        static_cast<uint32_t>(dirty_indices.size()),
                        ring_writer.GetFrameIndex());
            return;
        }

        std::vector<graph::IGPUBuffer::DirtyRange> flush_ranges;
        flush_ranges.reserve(merged_ranges.size());
        VkDeviceSize total_written_bytes = 0;

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
                const glm::mat4 world_matrix = storage.GetWorldMatrix(handle);
                dst[i] = *reinterpret_cast<const math::Matrix4f*>(&world_matrix);
                ApplyCameraRelativeOffset(dst[i]);
            }

            flush_ranges.push_back({byte_offset, byte_size});
            total_written_bytes += byte_size;
        }

        tbuf->Unmap();

        if (!flush_ranges.empty())
        {
            RebuildTrackedDirtyRanges(tbuf, flush_ranges);

            uint32_t sample_handle_u32 = 0;
            glm::vec3 sample_world_pos(0.0f);
            bool has_sample = false;
            for (const auto &range : merged_ranges)
            {
                const uint32_t first = static_cast<uint32_t>(range.first);
                if (first < handles.size())
                {
                    const auto sample_handle = handles[first];
                    const glm::mat4 sample_world = storage.GetWorldMatrix(sample_handle);
                    sample_world_pos = glm::vec3(sample_world[3]);
                    sample_handle_u32 = static_cast<uint32_t>(sample_handle);
                    has_sample = true;
                    break;
                }
            }

            GLogInfo("[TransformAssignmentBuffer] Dynamic L2W flush: static_count=%u base=%u ranges=%u dirty_indices=%u bytes=%llu buffer_dirty=%d sample_handle=%u sample_pos=(%.3f, %.3f, %.3f)",
                      static_count,
                      base_index,
                      static_cast<uint32_t>(flush_ranges.size()),
                      static_cast<uint32_t>(dirty_indices.size()),
                      static_cast<unsigned long long>(total_written_bytes),
                      tbuf->IsDirty() ? 1 : 0,
                      has_sample ? sample_handle_u32 : 0u,
                      has_sample ? sample_world_pos.x : 0.0f,
                      has_sample ? sample_world_pos.y : 0.0f,
                      has_sample ? sample_world_pos.z : 0.0f);

            std::fprintf(stderr,
                         "[TransformAssignmentBuffer] Dynamic L2W flush: static_count=%u base=%u ranges=%u dirty_indices=%u bytes=%llu buffer_dirty=%d sample_handle=%u sample_pos=(%.3f, %.3f, %.3f)\n",
                         static_count,
                         base_index,
                         static_cast<uint32_t>(flush_ranges.size()),
                         static_cast<uint32_t>(dirty_indices.size()),
                         static_cast<unsigned long long>(total_written_bytes),
                         tbuf->IsDirty() ? 1 : 0,
                         has_sample ? sample_handle_u32 : 0u,
                         has_sample ? sample_world_pos.x : 0.0f,
                         has_sample ? sample_world_pos.y : 0.0f,
                         has_sample ? sample_world_pos.z : 0.0f);

            if (ShouldEmitPeriodicLog(60))
            {
                GLogInfo("[TransformAssignmentBuffer] Dynamic L2W detail: min=%u max=%u span_offset=%llu span_size=%llu frame=%u",
                         min_first,
                         max_last,
                         static_cast<unsigned long long>(span_offset_bytes),
                         static_cast<unsigned long long>(span_size_bytes),
                         ring_writer.GetFrameIndex());
            }
        }
    }

    void TransformAssignmentBuffer::Clear()
    {
        if (buffer_manager)
        {
            if (transform_buffer)
                buffer_manager->Release(transform_buffer);
            if (transform_id_buffer)
                buffer_manager->Release(transform_id_buffer);
            transform_buffer = nullptr;
            transform_id_buffer = nullptr;
        }
        else
        {
            SAFE_CLEAR(transform_buffer);
            SAFE_CLEAR(transform_id_buffer);
        }

        transform_buffer_max_count = 0;
        transform_id_buffer_max_count = 0;
        transform_id_vk_buffer = VK_NULL_HANDLE;
        pending_updates.clear();
    }

    bool TransformAssignmentBuffer::EnsureTransformIDBufferCapacity(const size_t item_count,graph::BufferAllocPolicy policy)
    {
        (void)policy;

        if (item_count == 0)
            return true;

        bool recreated = false;

        if (!transform_id_buffer)
        {
            transform_id_buffer_max_count = hgl::power_to_2(item_count);
            recreated = true;
        }
        else if (item_count > transform_id_buffer_max_count)
        {
            transform_id_buffer_max_count = hgl::power_to_2(item_count);
            if (buffer_manager)
            {
                buffer_manager->Release(transform_id_buffer);
                transform_id_buffer = nullptr;
            }
            else
            {
                SAFE_CLEAR(transform_id_buffer);
            }

            recreated = true;
        }

        if (!transform_id_buffer && buffer_manager)
        {
            const VkDeviceSize buffer_size = sizeof(graph::Assign::TransformID::ValueType) * transform_id_buffer_max_count;

            transform_id_buffer = buffer_manager->CreateSSBO("ECS:TransformIDData",
                                                             buffer_size,
                                                             nullptr,
                                                             graph::SharingMode::Exclusive);

            recreated = true;
        }

        transform_id_vk_buffer = transform_id_buffer ? transform_id_buffer->GetBuffer() : VK_NULL_HANDLE;

        if (!transform_id_buffer)
        {
            GLogError("[TransformAssignmentBuffer] TransformID descriptor buffer allocation failed: required=%u capacity=%u policy=%d",
                      static_cast<uint32_t>(item_count),
                      transform_id_buffer_max_count,
                      static_cast<int>(policy));
            return false;
        }

        if (recreated)
        {
            GLogInfo("[TransformAssignmentBuffer] TransformID descriptor buffer ready: required=%u capacity=%u bytes=%llu policy=%d vk=0x%llX",
                     static_cast<uint32_t>(item_count),
                     transform_id_buffer_max_count,
                     static_cast<unsigned long long>(transform_id_buffer->GetSize()),
                     static_cast<int>(policy),
                     static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(transform_id_vk_buffer)));
        }

        return true;
    }

    void TransformAssignmentBuffer::StatTransform(const size_t required_count,graph::BufferAllocPolicy policy)
    {
        bool recreated = false;

        // 检查是否需要重新分配缓冲
        if (!transform_buffer)
        {
            transform_buffer_max_count = hgl::power_to_2(required_count);
            recreated = true;
        }
        else if (required_count > transform_buffer_max_count)
        {
            transform_buffer_max_count = hgl::power_to_2(required_count);
            if (buffer_manager)
            {
                buffer_manager->Release(transform_buffer);
                transform_buffer = nullptr;
            }
            else
            {
                SAFE_CLEAR(transform_buffer);
            }

            recreated = true;
        }

        // Recreate if policy changed
        if (transform_buffer && transform_policy != policy)
        {
            if (buffer_manager)
            {
                buffer_manager->Release(transform_buffer);
                transform_buffer = nullptr;
            }
            else
            {
                SAFE_CLEAR(transform_buffer);
            }

            recreated = true;
        }

        transform_policy = policy;

        // Create or reuse Transform SSBO
        if (!transform_buffer && buffer_manager)
        {
            transform_buffer = buffer_manager->CreateSSBO("ECS:LocalToWorld",
                                                          sizeof(math::Matrix4f) * transform_buffer_max_count,
                                                          nullptr,
                                                          graph::SharingMode::Exclusive);

            recreated = true;
        }

        ring_writer.SetBuffer(transform_buffer);

        if (recreated)
        {
            if (!WriteIdentityToL2WSlot0(transform_buffer))
            {
                GLogError("[TransformAssignmentBuffer] failed to initialize identity matrix at L2W slot 0");
            }
        }

        if (!transform_buffer)
        {
            GLogError("[TransformAssignmentBuffer] StatTransform failed: required=%u capacity=%u policy=%d (transform buffer is null)",
                      static_cast<uint32_t>(required_count),
                      transform_buffer_max_count,
                      static_cast<int>(policy));
            return;
        }

        if (recreated)
        {
            GLogInfo("[TransformAssignmentBuffer] LocalToWorld buffer ready: required=%u capacity=%u bytes=%llu policy=%d",
                     static_cast<uint32_t>(required_count),
                     transform_buffer_max_count,
                     static_cast<unsigned long long>(transform_buffer->GetSize()),
                     static_cast<int>(policy));

            std::fprintf(stderr,
                         "[TransformAssignmentBuffer] LocalToWorld buffer ready: required=%u capacity=%u bytes=%llu policy=%d\n",
                         static_cast<uint32_t>(required_count),
                         transform_buffer_max_count,
                         static_cast<unsigned long long>(transform_buffer->GetSize()),
                         static_cast<int>(policy));

            LogDeviceBufferSnapshot("[TransformAssignmentBuffer] L2W recreated", transform_buffer);
        }
        else if (ShouldEmitPeriodicLog())
        {
            GLogInfo("[TransformAssignmentBuffer] StatTransform reuse: required=%u capacity=%u policy=%d frame=%u",
                     static_cast<uint32_t>(required_count),
                     transform_buffer_max_count,
                     static_cast<int>(policy),
                     ring_writer.GetFrameIndex());
        }
    }

    static bool GetStorageWorldMatrix(const RenderItem* item, math::Matrix4f& out)
    {
        if (!item)
        {
            out = math::Identity4f;
            return false;
        }

        auto transform = item->GetTransform();
        if (!transform)
        {
            out = math::Identity4f;
            return false;
        }

        const auto handle = transform->GetStorageHandle();
        if (handle == TransformDataStorage::INVALID_HANDLE)
        {
            out = math::Identity4f;
            return false;
        }

        const auto storage = TransformComponent::GetSharedStorage();

        if (!storage)
        {
            out = math::Identity4f;
            return false;
        }

        const glm::mat4 world_matrix = storage->GetWorldMatrix(handle);
        out = *reinterpret_cast<const math::Matrix4f*>(&world_matrix);
        return true;
    }

    void TransformAssignmentBuffer::UpdateTransformData(const std::vector<RenderItem*>& items, const int first, const int last)
    {
        (void)items;
        QueueUpdateRange(first,last);
    }

    void TransformAssignmentBuffer::QueueUpdateRange(const int first,const int last)
    {
        if (first > last)
            return;

        UpdateRange range;
        range.first = first;
        range.last = last;

        for (auto &pending : pending_updates)
        {
            if (range.last + 1 < pending.first || range.first > pending.last + 1)
                continue;

            pending.first = hgl_min(pending.first, range.first);
            pending.last = hgl_max(pending.last, range.last);
            return;
        }

        pending_updates.push_back(range);
    }

    void TransformAssignmentBuffer::WriteRange(const std::vector<RenderItem*>& items,const int first,const int last)
    {
        if (!transform_buffer || items.empty() || first > last)
            return;

        const size_t map_size = sizeof(math::Matrix4f) * (last - first + 1);
        const size_t map_offset = sizeof(math::Matrix4f) * first;

        auto *tbuf = transform_buffer->GetGPUBuffer();
        math::Matrix4f* l2wp = tbuf ? (math::Matrix4f*)tbuf->Map(map_offset, map_size) : nullptr;
        if (!l2wp)
            return;

        const size_t count = items.size();
        for (size_t i = 0; i < count; i++)
        {
            RenderItem* item = items[i];
            if (!item)
                continue;

            const uint32_t transform_idx = item->transform_index;
            if (transform_idx < static_cast<uint32_t>(first) || transform_idx > static_cast<uint32_t>(last))
                continue;

            math::Matrix4f l2w;
            GetStorageWorldMatrix(item, l2w);
            l2wp[transform_idx - first] = l2w;
            ApplyCameraRelativeOffset(l2wp[transform_idx - first]);
        }

        if(tbuf) tbuf->Unmap();
    }

    void TransformAssignmentBuffer::SplitStaticAndMovableItems(const std::vector<RenderItem*>& items,
                                                               std::vector<RenderItem*>& static_items,
                                                               std::vector<RenderItem*>& movable_items) const
    {
        static_items.clear();
        movable_items.clear();

        static_items.reserve(items.size());
        movable_items.reserve(items.size());

        for (auto *item : items)
        {
            if (!item)
                continue;

            auto transform = item->GetTransform();
            if (transform && !transform->IsMovable())
                static_items.push_back(item);
            else
                movable_items.push_back(item);
        }
    }

    void TransformAssignmentBuffer::SortStaticItemsByHandle(std::vector<RenderItem*>& static_items) const
    {
        std::sort(static_items.begin(), static_items.end(),
            [](const RenderItem* a, const RenderItem* b)
            {
                const auto at = a ? a->GetTransform() : nullptr;
                const auto bt = b ? b->GetTransform() : nullptr;
                const auto ah = at ? at->GetStorageHandle() : TransformDataStorage::INVALID_HANDLE;
                const auto bh = bt ? bt->GetStorageHandle() : TransformDataStorage::INVALID_HANDLE;

                if (ah == TransformDataStorage::INVALID_HANDLE && bh == TransformDataStorage::INVALID_HANDLE)
                    return false;
                if (ah == TransformDataStorage::INVALID_HANDLE)
                    return false;
                if (bh == TransformDataStorage::INVALID_HANDLE)
                    return true;
                return ah < bh;
            });
    }

    void TransformAssignmentBuffer::AssignTransformIndices(std::vector<RenderItem*>& static_items,
                                                           std::vector<RenderItem*>& movable_items,
                                                           const uint32_t ring_base) const
    {
        uint32_t transform_index = kFirstObjectL2WSlot;
        for (auto *item : static_items)
        {
            if (item)
                item->transform_index = transform_index++;
        }

        uint32_t dynamic_index = 0;
        for (auto *item : movable_items)
        {
            if (item)
                item->transform_index = ring_base + dynamic_index++;
        }
    }

    bool TransformAssignmentBuffer::WriteAllLocalToWorld(const std::vector<RenderItem*>& static_items,
                                                         const std::vector<RenderItem*>& movable_items,
                                                         const uint32_t static_count,
                                                         const uint32_t dynamic_count,
                                                         const uint32_t total_count)
    {
        auto *wbuf = transform_buffer ? transform_buffer->GetGPUBuffer() : nullptr;
        math::Matrix4f* l2wp = wbuf ? (math::Matrix4f*)wbuf->Map(0, wbuf->GetSize()) : nullptr;
        if (!l2wp)
        {
            GLogWarning("[TransformAssignmentBuffer::WriteItems] L2W map failed: total=%u bytes=%llu",
                        total_count,
                        wbuf ? static_cast<unsigned long long>(wbuf->GetSize()) : 0ULL);
            GLogWarning("[TransformAssignmentBuffer::WriteItems] L2W map context: static=%u dynamic=%u frame=%u base=%u",
                        static_count,
                        dynamic_count,
                        ring_writer.GetFrameIndex(),
                        ring_writer.GetBaseIndex(static_count + kFirstObjectL2WSlot, dynamic_count));
            return false;
        }

        for (auto *item : static_items)
        {
            if (!item)
                continue;

            const uint32_t idx = item->transform_index;
            math::Matrix4f l2w;
            GetStorageWorldMatrix(item, l2w);
            l2wp[idx] = l2w;
            ApplyCameraRelativeOffset(l2wp[idx]);
        }

        for (auto *item : movable_items)
        {
            if (!item)
                continue;

            const uint32_t idx = item->transform_index;
            math::Matrix4f l2w;
            GetStorageWorldMatrix(item, l2w);
            l2wp[idx] = l2w;
            ApplyCameraRelativeOffset(l2wp[idx]);
        }

        if (wbuf)
            wbuf->Unmap();

        GLogInfo("[TransformAssignmentBuffer::WriteItems] L2W full write: static=%u dynamic=%u total=%u bytes=%llu dirty=%d",
                  static_count,
                  dynamic_count,
                  total_count,
                  static_cast<unsigned long long>(wbuf->GetSize()),
                  wbuf->IsDirty() ? 1 : 0);

        std::fprintf(stderr,
                     "[TransformAssignmentBuffer::WriteItems] L2W full write: static=%u dynamic=%u total=%u bytes=%llu dirty=%d\n",
                     static_count,
                     dynamic_count,
                     total_count,
                     static_cast<unsigned long long>(wbuf->GetSize()),
                     wbuf->IsDirty() ? 1 : 0);

        if (ShouldEmitPeriodicLog(90))
        {
            GLogInfo("[TransformAssignmentBuffer::WriteItems] L2W full context: frame=%u ring_base=%u ring_frames=%u",
                     ring_writer.GetFrameIndex(),
                     ring_writer.GetBaseIndex(static_count + kFirstObjectL2WSlot, dynamic_count),
                     ring_writer.GetRingFrames());
        }

        return true;
    }

    bool TransformAssignmentBuffer::WriteTransformIDBuffer(const std::vector<RenderItem*>& items,
                                                           const size_t item_count,
                                                           const uint32_t max_transform_id)
    {
        if (!transform_id_buffer)
            return false;

        graph::IGPUBuffer *transform_gpu = transform_id_buffer->GetGPUBuffer();
        if (!transform_gpu)
        {
            GLogError("[TransformAssignmentBuffer::WriteItems] TransformID descriptor GPU buffer is null");
            return false;
        }

        graph::Assign::TransformID::ValueType* transform_ptr =
            (graph::Assign::TransformID::ValueType*)(transform_gpu->Map(0, transform_gpu->GetSize()));

        if (!transform_ptr)
        {
            GLogWarning("[TransformAssignmentBuffer::WriteItems] TransformID descriptor map failed: items=%u bytes=%llu",
                        static_cast<uint32_t>(item_count),
                        static_cast<unsigned long long>(transform_gpu->GetSize()));
            return false;
        }

        bool warned_overflow = false;

        for (size_t i = 0; i < item_count; i++)
        {
            RenderItem* item = items[i];

            if (!item)
            {
                *transform_ptr = 0;
                ++transform_ptr;
                continue;
            }

            const uint32_t idx = item->transform_index;
            if (idx > max_transform_id)
            {
                if (!warned_overflow && sizeof(graph::Assign::TransformID::ValueType) == sizeof(uint16_t))
                {
                    std::cout << "[TransformAssignmentBuffer::WriteItems] WARNING: TransformID overflow ("
                              << idx << ")" << std::endl;
                    warned_overflow = true;
                }

                *transform_ptr = static_cast<graph::Assign::TransformID::ValueType>(0);
            }
            else
            {
                *transform_ptr = static_cast<graph::Assign::TransformID::ValueType>(idx);
            }

            ++transform_ptr;
        }

        transform_gpu->Unmap();

        GLogInfo("[TransformAssignmentBuffer::WriteItems] TransformID descriptor write complete: items=%u capacity=%u dirty=%d vkbuf=0x%llX frame=%u",
                 static_cast<uint32_t>(item_count),
                 transform_id_buffer_max_count,
                 transform_gpu->IsDirty() ? 1 : 0,
                 static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(transform_id_vk_buffer)),
                 ring_writer.GetFrameIndex());

        return true;
    }

    void TransformAssignmentBuffer::WriteItems(const std::vector<RenderItem*>& items)
    {
        const size_t item_count = items.size();

        if (item_count == 0)
        {
            std::cout << "[TransformAssignmentBuffer::WriteItems] WARNING: No items to write" << std::endl;
            return;
        }

        last_items = &items;

        static_only = (mode == Mode::StaticOnly);

        std::vector<RenderItem*> static_items;
        std::vector<RenderItem*> movable_items;
        SplitStaticAndMovableItems(items, static_items, movable_items);
        SortStaticItemsByHandle(static_items);

        const uint32_t static_count = static_cast<uint32_t>(static_items.size());
        const uint32_t dynamic_count = static_cast<uint32_t>(movable_items.size());
        const uint32_t ring_frames = ring_writer.GetRingFrames();
        const uint32_t ring_base = ring_writer.GetBaseIndex(static_count + kFirstObjectL2WSlot, dynamic_count);
        const uint32_t total_count = ring_writer.GetTotalCount(static_count + kFirstObjectL2WSlot, dynamic_count);
        const uint32_t max_transform_id = std::numeric_limits<graph::Assign::TransformID::ValueType>::max();

        if (ShouldEmitPeriodicLog(60) || (dynamic_count > 0 && static_count == 0))
        {
            GLogInfo("[TransformAssignmentBuffer::WriteItems] Begin: items=%u static=%u dynamic=%u ring_base=%u total=%u frame=%u mode=%d",
                     static_cast<uint32_t>(item_count),
                     static_count,
                     dynamic_count,
                     ring_base,
                     total_count,
                     ring_writer.GetFrameIndex(),
                     static_cast<int>(mode));
        }

        if (sizeof(graph::Assign::TransformID::ValueType) == sizeof(uint16_t)
         && total_count > max_transform_id + 1)
        {
            std::cout << "[TransformAssignmentBuffer::WriteItems] WARNING: Transform count exceeds R16 limit ("
                      << total_count << ")" << std::endl;
        }

        last_static_count = static_count;
        last_dynamic_count = dynamic_count;

        if (static_only)
            StatTransform(total_count,graph::BufferAllocPolicy::GPUOnly);
        else
            StatTransform(total_count,graph::BufferAllocPolicy::Auto);

        if (!transform_buffer)
        {
            GLogError("[TransformAssignmentBuffer::WriteItems] transform_buffer is null after StatTransform (total_count=%u)", total_count);
            return;
        }

        if (ShouldEmitPeriodicLog(60))
            LogDeviceBufferSnapshot("[TransformAssignmentBuffer::WriteItems] L2W before write", transform_buffer);

        AssignTransformIndices(static_items, movable_items, ring_base);

        WriteAllLocalToWorld(static_items, movable_items, static_count, dynamic_count, total_count);

        if (!EnsureTransformIDBufferCapacity(item_count,
                                             static_only ? graph::BufferAllocPolicy::GPUOnly : graph::BufferAllocPolicy::Auto))
            return;

        WriteTransformIDBuffer(items, item_count, max_transform_id);
    }

    void TransformAssignmentBuffer::WriteTransformIDs(const std::vector<RenderItem*>& items)
    {
        const size_t item_count = items.size();
        if (item_count == 0)
            return;

        const uint32_t max_transform_id = std::numeric_limits<graph::Assign::TransformID::ValueType>::max();

        if (!EnsureTransformIDBufferCapacity(item_count, graph::BufferAllocPolicy::Auto))
            return;

        WriteTransformIDBuffer(items, item_count, max_transform_id);
    }

    void TransformAssignmentBuffer::FlushPendingUpdates()
    {
        if (pending_updates.empty() || !last_items || !transform_buffer)
            return;

        std::sort(pending_updates.begin(), pending_updates.end(),
            [](const UpdateRange &a, const UpdateRange &b)
            {
                return a.first < b.first;
            });

        std::vector<graph::IGPUBuffer::DirtyRange> dirty_ranges;
        dirty_ranges.reserve(pending_updates.size());

        const VkDeviceSize matrix_size = sizeof(math::Matrix4f);
        const VkDeviceSize buffer_size = transform_buffer->GetSize();

        for (const auto &range : pending_updates)
        {
            if (range.first < 0 || range.last < range.first)
                continue;

            WriteRange(*last_items, range.first, range.last);

            VkDeviceSize offset = matrix_size * static_cast<VkDeviceSize>(range.first);
            VkDeviceSize size = matrix_size * static_cast<VkDeviceSize>(range.last - range.first + 1);

            if (offset >= buffer_size)
                continue;

            if (offset + size > buffer_size)
                size = buffer_size - offset;

            if (size == 0)
                continue;

            dirty_ranges.push_back({offset, size});
        }

        if (!dirty_ranges.empty())
        {
            transform_buffer->FlushRanges(dirty_ranges.data(), dirty_ranges.size());

            auto *gpu = transform_buffer->GetGPUBuffer();
            GLogInfo("[TransformAssignmentBuffer] FlushPendingUpdates: ranges=%u buffer_dirty=%d",
                      static_cast<uint32_t>(dirty_ranges.size()),
                      gpu ? (gpu->IsDirty() ? 1 : 0) : -1);

            if (ShouldEmitPeriodicLog(60))
            {
                const auto &first = dirty_ranges.front();
                const auto &last = dirty_ranges.back();
                GLogInfo("[TransformAssignmentBuffer] FlushPendingUpdates detail: first=[%llu,%llu] last=[%llu,%llu] frame=%u",
                         static_cast<unsigned long long>(first.offset),
                         static_cast<unsigned long long>(first.size),
                         static_cast<unsigned long long>(last.offset),
                         static_cast<unsigned long long>(last.size),
                         ring_writer.GetFrameIndex());
            }
        }

        pending_updates.clear();
    }

    void TransformAssignmentBuffer::FlushAllPendingUpdates()
    {
        for (auto *inst : all_instances)
        {
            if (inst)
                inst->FlushPendingUpdates();
        }
    }

    void TransformAssignmentBuffer::AdvanceFrame()
    {
        for (auto *inst : all_instances)
        {
            if (inst)
                inst->ring_writer.AdvanceFrame();
        }
    }

    void TransformAssignmentBuffer::SetFrameIndex(const uint32_t index)
    {
        for (auto *inst : all_instances)
        {
            if (inst)
                inst->ring_writer.SetFrameIndex(index);
        }
    }
}//namespace hgl::ecs

