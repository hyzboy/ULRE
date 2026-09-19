#include <hgl/graph/module/MeshDrawParamsPool.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/log/Log.h>
#include <cstring>

namespace hgl::graph
{

GRAPH_MODULE_CONSTRUCT(MeshDrawParamsPool)
{
}

void MeshDrawParamsPool::OnGraphicsContextChanged(GraphicsContext *graphics_context)
{
    if (!graphics_context || pool_initialized)
        return;

    if (!InitializePool())
    {
        GLogError("[MeshDrawParamsPool] Failed to initialize global MeshDrawParams pool");
    }
}

bool MeshDrawParamsPool::InitializePool()
{
    if (pool_initialized)
        return true;

    auto *device = GetDevice();
    if (!device)
        return false;

    // 行 0 保留为全零行，bda_align16 = true 保证 16 字节对齐
    if (!pool.Create(
            device,
            "Global:MeshDrawParamsPool",
            sizeof(mtl::MeshDrawParams),
            DefaultMeshDrawParamsCapacity,
            0u,
            1u,     // reserve row 0
            true    // bda_align16
        ))
    {
        GLogError("[MeshDrawParamsPool] Failed to create ActiveRowPool for MeshDrawParams");
        return false;
    }

    pool_initialized = true;
    return true;
}

void MeshDrawParamsPool::Release()
{
    pool.Reset();
    pool_initialized = false;
}

uint32_t MeshDrawParamsPool::Acquire(const mtl::MeshDrawParams &params)
{
    if (!pool_initialized)
    {
        if (!InitializePool())
            return ActiveRowPool::InvalidRowID;
    }

    const uint32_t id = pool.Acquire();
    if (id == ActiveRowPool::InvalidRowID)
    {
        GLogError("[MeshDrawParamsPool] Capacity exhausted: capacity=%u", pool.GetRowCapacity());
        return ActiveRowPool::InvalidRowID;
    }

    void *cpu_ptr = pool.RowCPU(id);
    if (!cpu_ptr)
    {
        pool.Release(id);
        return ActiveRowPool::InvalidRowID;
    }

    std::memcpy(cpu_ptr, &params, sizeof(mtl::MeshDrawParams));
    pool.CommitRow(id);
    return id;
}

bool MeshDrawParamsPool::Write(uint32_t id, const mtl::MeshDrawParams &params)
{
    if (!pool_initialized || !pool.IsActive(id))
        return false;

    void *cpu_ptr = pool.RowCPU(id);
    if (!cpu_ptr)
        return false;

    std::memcpy(cpu_ptr, &params, sizeof(mtl::MeshDrawParams));
    return pool.CommitRow(id);
}

bool MeshDrawParamsPool::ReleaseID(uint32_t id)
{
    if (!pool_initialized)
        return false;

    return pool.Release(id);
}

bool MeshDrawParamsPool::IsActive(uint32_t id) const
{
    if (!pool_initialized)
        return false;

    return pool.IsActive(id);
}

} // namespace hgl::graph
