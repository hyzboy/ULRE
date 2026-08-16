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
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/ecs/components/TransformComponent.h>
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
        constexpr graph::mtl::SSBOAddress kTransformIndexRowsAddress{
            graph::mtl::SSBOType::TransformIndexRows,
            graph::mtl::ECSReservedSSBOId::TransformIndexRows,
            0};
        constexpr graph::mtl::SSBOAddress kLocalToWorldAddress{
            graph::mtl::SSBOType::LocalToWorld,
            graph::mtl::ECSReservedSSBOId::LocalToWorldData,
            0};

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

    TransformAssignmentBuffer::TransformAssignmentBuffer(graph::BufferManager* bm,
                                                         graph::ResourceDomainManager* rdm,
                                                         uint32_t ring_frames)
        : buffer_manager(bm)
        , resource_domain_manager(rdm)
        , transform_buffer_max_count(0)
        , transform_buffer(nullptr)
        , transform_policy(graph::BufferAllocPolicy::Auto)
        , transform_index_rows_max_count(0)
        , transform_index_rows_buffer(nullptr)
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

    void TransformAssignmentBuffer::BindTransform(graph::ShaderProgram* mtl) const
    {
        if (!mtl)
        {
            GLogWarning("[TransformAssignmentBuffer::BindTransform] ShaderProgram is null");
            return;
        }

        if (!transform_buffer)
        {
            GLogWarning("[TransformAssignmentBuffer::BindTransform] Transform buffer not created");
            return;
        }

        const uint32_t expected_version = graph::mtl::GetSSBOTypeStructVersion(graph::mtl::SSBOType::LocalToWorld);
        const uint32_t expected_stride = graph::mtl::GetSSBOTypeStructStride(graph::mtl::SSBOType::LocalToWorld);
        if (expected_version > 0 && expected_stride > 0)
        {
            const VkDeviceSize buffer_size = transform_buffer->GetSize();
            if (buffer_size == 0 || (buffer_size % expected_stride) != 0)
            {
                GLogError("[R11] Skip LocalToWorld bind: version=%u expected_stride=%u buffer_size=%llu",
                          expected_version,
                          expected_stride,
                          static_cast<unsigned long long>(buffer_size));
                return;
            }
        }

        LogDeviceBufferSnapshot("[TransformAssignmentBuffer::BindTransform] before bind", transform_buffer);

        mtl->BindSSBO(hgl::graph::mtl::SBS_LocalToWorld.set_type,
                  hgl::graph::mtl::SBS_LocalToWorld.name,
                  transform_buffer->GetGPUBuffer());
        GLogInfo("[TransformAssignmentBuffer::BindTransform] BindSSBO set_type=%d name=%s",
                 static_cast<int>(hgl::graph::mtl::SBS_LocalToWorld.set_type),
                 hgl::graph::mtl::SBS_LocalToWorld.name);
    }

    void TransformAssignmentBuffer::EnsureCapacity(const uint32_t static_count,const uint32_t dynamic_count,graph::BufferAllocPolicy policy)
    {
        const uint32_t total_count = ring_writer.GetTotalCount(static_count + kFirstObjectL2WSlot, dynamic_count);
        StatTransform(total_count, policy);
        WriteTransformIndexRows(static_count, dynamic_count);

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
        if (resource_domain_manager)
        {
            resource_domain_manager->ClearDomain(kLocalToWorldAddress);
            resource_domain_manager->ClearDomain(kTransformIndexRowsAddress);
            transform_buffer = nullptr;
            transform_index_rows_buffer = nullptr;
        }
        else if (buffer_manager)
        {
            if (transform_buffer)
                buffer_manager->Release(transform_buffer);
            if (transform_index_rows_buffer)
                buffer_manager->Release(transform_index_rows_buffer);
            transform_buffer = nullptr;
            transform_index_rows_buffer = nullptr;
        }
        else
        {
            SAFE_CLEAR(transform_buffer);
            SAFE_CLEAR(transform_index_rows_buffer);
        }

        transform_buffer_max_count = 0;
        transform_index_rows_max_count = 0;
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
            if (resource_domain_manager)
            {
                resource_domain_manager->ClearDomain(kLocalToWorldAddress);
                transform_buffer = nullptr;
            }
            else if (buffer_manager)
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
            if (resource_domain_manager)
            {
                resource_domain_manager->ClearDomain(kLocalToWorldAddress);
                transform_buffer = nullptr;
            }
            else if (buffer_manager)
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

        // 创建或重用 Transform SSBO
        if (!transform_buffer && buffer_manager)
        {
            transform_buffer = buffer_manager->CreateSSBO("ECS:LocalToWorld",
                                                          sizeof(math::Matrix4f) * transform_buffer_max_count,
                                                          nullptr,
                                                          graph::SharingMode::Exclusive);

            recreated = true;
        }

        if (resource_domain_manager && transform_buffer)
        {
            if (!resource_domain_manager->RegisterBuffer(kLocalToWorldAddress,
                                                         transform_buffer,
                                                         transform_buffer_max_count))
            {
                GLogError("[R11] Failed to register LocalToWorld domain buffer: capacity=%u bytes=%llu",
                          transform_buffer_max_count,
                          static_cast<unsigned long long>(transform_buffer->GetSize()));

                if (buffer_manager)
                    buffer_manager->Release(transform_buffer);
                else
                    SAFE_CLEAR(transform_buffer);

                transform_buffer = nullptr;
                return;
            }
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








    bool TransformAssignmentBuffer::EnsureTransformIndexRowsCapacity(const uint32_t required_count)
    {
        bool recreated = false;

        if (!transform_index_rows_buffer)
        {
            transform_index_rows_max_count = hgl::power_to_2(required_count);
            recreated = true;
        }
        else if (transform_index_rows_max_count < required_count)
        {
            transform_index_rows_max_count = hgl::power_to_2(required_count);
            if (resource_domain_manager)
            {
                resource_domain_manager->ClearDomain(kTransformIndexRowsAddress);
                transform_index_rows_buffer = nullptr;
            }
            else if (buffer_manager)
            {
                buffer_manager->Release(transform_index_rows_buffer);
                transform_index_rows_buffer = nullptr;
            }
            else
            {
                SAFE_CLEAR(transform_index_rows_buffer);
            }
            recreated = true;
        }

        if (!transform_index_rows_buffer && buffer_manager)
        {
            transform_index_rows_buffer = buffer_manager->CreateSSBO("ECS:TransformIndexRows",
                                                                     sizeof(uint32_t) * transform_index_rows_max_count,
                                                                     nullptr,
                                                                     graph::SharingMode::Exclusive);
            recreated = true;
        }

        if (resource_domain_manager && transform_index_rows_buffer)
        {
            if (!resource_domain_manager->RegisterBuffer(kTransformIndexRowsAddress,
                                                         transform_index_rows_buffer,
                                                         transform_index_rows_max_count))
            {
                GLogError("[R11] Failed to register TransformIndexRows domain buffer: capacity=%u bytes=%llu",
                          transform_index_rows_max_count,
                          static_cast<unsigned long long>(transform_index_rows_buffer->GetSize()));

                if (buffer_manager)
                    buffer_manager->Release(transform_index_rows_buffer);
                else
                    SAFE_CLEAR(transform_index_rows_buffer);

                transform_index_rows_buffer = nullptr;
                transform_index_rows_max_count = 0;
                return false;
            }
        }

        if (recreated && transform_index_rows_buffer)
        {
            auto *gpu = transform_index_rows_buffer->GetGPUBuffer();
            GLogInfo("[TransformAssignmentBuffer] TransformIndexRows buffer ready: required=%u capacity=%u bytes=%llu gpu=0x%llX",
                     required_count,
                     transform_index_rows_max_count,
                     static_cast<unsigned long long>(gpu ? gpu->GetSize() : 0ULL),
                     static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(gpu)));
        }

        return transform_index_rows_buffer != nullptr;
    }

    bool TransformAssignmentBuffer::WriteTransformIndexRows(const uint32_t static_count, const uint32_t dynamic_count)
    {
        const uint32_t required_count = static_count + dynamic_count + 1; // slot 0 保留 identity
        if (!EnsureTransformIndexRowsCapacity(required_count))
            return false;

        auto *gpu = transform_index_rows_buffer ? transform_index_rows_buffer->GetGPUBuffer() : nullptr;
        if (!gpu)
            return false;

        std::vector<uint32_t> rows(required_count, 0);
        rows[0] = 0;

        for (uint32_t i = 0; i < static_count; ++i)
            rows[1 + i] = 1 + i;

        const uint32_t dynamic_base = ring_writer.GetBaseIndex(static_count + kFirstObjectL2WSlot, dynamic_count);
        for (uint32_t i = 0; i < dynamic_count; ++i)
            rows[1 + static_count + i] = dynamic_base + i;

        const VkDeviceSize byte_size = static_cast<VkDeviceSize>(rows.size()) * sizeof(uint32_t);
        if (!gpu->Write(rows.data(), 0, byte_size))
            return false;

        graph::IGPUBuffer::DirtyRange dirty{0, byte_size};
        transform_index_rows_buffer->FlushRanges(&dirty, 1);
        return true;
    }

    std::vector<TransformAssignmentBuffer*> TransformAssignmentBuffer::all_instances;

    void TransformAssignmentBuffer::SetFrameIndex(const uint32_t index)
    {
        for (auto *inst : all_instances)
        {
            if (inst)
                inst->ring_writer.SetFrameIndex(index);
        }
    }
}//namespace hgl::ecs
