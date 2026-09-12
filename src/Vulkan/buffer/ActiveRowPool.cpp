#include <hgl/vk/buffer/ActiveRowPool.h>
#include <hgl/vk/buffer/DeviceBuffer.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/log/Log.h>
#include <hgl/type/Smart.h>

#include <climits>
#include <cstring>

namespace hgl::graph
{

bool ActiveRowPool::Create(VulkanDevice *device,
                           const AnsiString &name,
                           const uint32_t in_row_bytes,
                           const uint32_t in_capacity,
                           const uint32_t in_ssbo_id,
                           const uint32_t reserve_rows,
                           const bool bda_align16)
{
    if (buffer)
    {
        GLogError("[ActiveRowPool] Create rejected: pool already created (name=%s)", name.c_str());
        return false;
    }

    if (!device || in_row_bytes == 0 || in_capacity == 0)
    {
        GLogError("[ActiveRowPool] Create rejected: device=%p row_bytes=%u capacity=%u",
                  (void *)device, in_row_bytes, in_capacity);
        return false;
    }

    const VkDeviceSize buffer_bytes = VkDeviceSize(in_row_bytes) * in_capacity;

    AutoDelete<DeviceBuffer> new_buffer(
        device->CreateArenaBuffer(name, buffer_bytes));
    if (!new_buffer)
    {
        GLogError("[ActiveRowPool] buffer allocation failed: name=%s row_bytes=%u capacity=%u",
                  name.c_str(), in_row_bytes, in_capacity);
        return false;
    }

    auto *gpu_buffer = new_buffer->GetGPUBuffer();
    if (!gpu_buffer)
    {
        GLogError("[ActiveRowPool] buffer has no GPU buffer: name=%s", name.c_str());
        return false;
    }

    const uint64_t addr = bda_align16
        ? device->GetBufferDeviceAddressAligned16(new_buffer->GetBuffer())
        : device->GetBufferDeviceAddress(new_buffer->GetBuffer());
    if (addr == 0)
    {
        GLogError("[ActiveRowPool] buffer has no device address: name=%s", name.c_str());
        return false;
    }

    void *cpu = gpu_buffer->Map(0, buffer_bytes);
    if (!cpu)
    {
        GLogError("[ActiveRowPool] buffer mapping failed: name=%s", name.c_str());
        return false;
    }

    memset(cpu, 0, static_cast<size_t>(buffer_bytes));
    gpu_buffer->MarkDirty(0, buffer_bytes);

    buffer       = new_buffer.Finish();
    cpu_base     = cpu;
    gpu_base     = addr;
    ssbo_id      = in_ssbo_id;
    row_bytes    = in_row_bytes;
    row_capacity = in_capacity;

    ids.Clear(true);
    pending_releases.clear();

    // 预留行（从 0 起）：直接占住行号，永不进入 Idle —— 典型用途 = 行 0 恒为零行。
    reserved_rows = (reserve_rows < in_capacity) ? reserve_rows : in_capacity;
    for (uint32_t i = 0; i < reserved_rows; ++i)
    {
        int reserved_id = -1;
        if (ids.CreateActive(&reserved_id) != 1)
        {
            GLogError("[ActiveRowPool] reserve row failed: name=%s i=%u", name.c_str(), i);
            Reset();
            return false;
        }
    }

    return true;
}

void ActiveRowPool::Reset()
{
    ids.Clear(true);
    pending_releases.clear();
    reserved_rows = 0;

    if (buffer)
    {
        auto *gpu_buffer = buffer->GetGPUBuffer();
        if (cpu_base && gpu_buffer)
            gpu_buffer->Unmap();

        SAFE_CLEAR(buffer);
    }

    buffer       = nullptr;
    cpu_base     = nullptr;
    gpu_base     = 0;
    ssbo_id      = 0;
    row_bytes    = 0;
    row_capacity = 0;
}

ActiveRowPool::RowID ActiveRowPool::Acquire()
{
    if (!IsReady())
    {
        GLogError("[ActiveRowPool] Acquire failed: pool not ready (ssbo_id=%u)", ssbo_id);
        return InvalidRowID;
    }

    if (row_capacity > static_cast<uint32_t>(INT_MAX))
    {
        GLogError("[ActiveRowPool] Acquire rejected invalid capacity: ssbo_id=%u capacity=%u",
                  ssbo_id, row_capacity);
        return InvalidRowID;
    }

    int id = -1;
    if (ids.HasIdleID())
    {
        id = ids.GetIdle();
    }
    else
    {
        if (ids.GetHistoryMaxId() >= static_cast<int>(row_capacity))
        {
            GLogError("[ActiveRowPool] Acquire exceeded capacity: ssbo_id=%u capacity=%u",
                      ssbo_id, row_capacity);
            return InvalidRowID;
        }

        if (ids.CreateActive(&id) != 1)
        {
            GLogError("[ActiveRowPool] Acquire failed: ssbo_id=%u", ssbo_id);
            return InvalidRowID;
        }
    }

    if (id < 0 || static_cast<uint32_t>(id) >= row_capacity)
    {
        if (id >= 0 && ids.IsActive(id))
            ids.Release(&id);

        GLogError("[ActiveRowPool] Acquire produced an invalid ID: ssbo_id=%u id=%d capacity=%u",
                  ssbo_id, id, row_capacity);
        return InvalidRowID;
    }

    return static_cast<RowID>(id);
}

bool ActiveRowPool::Release(const RowID id)
{
    if (id == InvalidRowID
     || id > static_cast<RowID>(INT_MAX)
     || !ids.IsActive(static_cast<int>(id)))
    {
        GLogError("[ActiveRowPool] Release rejected inactive ID: ssbo_id=%u id=%u",
                  ssbo_id, id);
        return false;
    }

    const int raw_id = static_cast<int>(id);
    return ids.Release(&raw_id) == 1;
}

bool ActiveRowPool::IsActive(const RowID id) const
{
    return id != InvalidRowID
        && id <= static_cast<RowID>(INT_MAX)
        && ids.IsActive(static_cast<int>(id));
}

bool ActiveRowPool::CommitRow(const RowID id)
{
    auto *gpu_buffer = GetGPUBuffer();
    if (!gpu_buffer
     || id >= row_capacity
     || !IsActive(id))
    {
        GLogError("[ActiveRowPool] CommitRow rejected invalid row: ssbo_id=%u id=%u",
                  ssbo_id, id);
        return false;
    }

    gpu_buffer->MarkDirty(VkDeviceSize(id) * row_bytes, row_bytes);
    return true;
}

bool ActiveRowPool::ReleaseDeferred(const RowID id, const uint64_t ready_epoch)
{
    if (!IsActive(id))
    {
        GLogError("[ActiveRowPool] ReleaseDeferred rejected inactive ID: ssbo_id=%u id=%u",
                  ssbo_id, id);
        return false;
    }

    for (const auto &pending : pending_releases)
    {
        if (pending.row_id == id)
        {
            GLogError("[ActiveRowPool] ReleaseDeferred rejected duplicated ID: ssbo_id=%u id=%u",
                      ssbo_id, id);
            return false;
        }
    }

    pending_releases.push_back(PendingRelease{id, ready_epoch});
    return true;
}

uint32_t ActiveRowPool::CollectRecyclable(const uint64_t completed_epoch,
                                          std::vector<RowID> *out_recycled)
{
    uint32_t collected = 0;

    for (size_t i = 0; i < pending_releases.size();)
    {
        const PendingRelease pending = pending_releases[i];
        if (pending.ready_epoch > completed_epoch)
        {
            ++i;
            continue;
        }

        if (void *row = RowCPU(pending.row_id))
            memset(row, 0, row_bytes);      // 复用前清零（字节行：避免残留旧内容）

        const int raw_id = static_cast<int>(pending.row_id);
        if (ids.Release(&raw_id) == 1)
        {
            ++collected;
            if (out_recycled)
                out_recycled->push_back(pending.row_id);
        }
        else
        {
            GLogError("[ActiveRowPool] CollectRecyclable failed to release: ssbo_id=%u id=%u",
                      ssbo_id, pending.row_id);
        }

        pending_releases.erase(pending_releases.begin() + static_cast<ptrdiff_t>(i));
    }

    return collected;
}

IGPUBuffer *ActiveRowPool::GetGPUBuffer() const
{
    return buffer ? buffer->GetGPUBuffer() : nullptr;
}

const IGPUBuffer *ActiveRowPool::GetGPUBufferConst() const
{
    return buffer ? buffer->GetGPUBuffer() : nullptr;
}

} // namespace hgl::graph
