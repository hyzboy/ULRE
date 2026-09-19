#pragma once

#include <hgl/graph/module/GraphModule.h>
#include <hgl/graph/ShaderBufferSources.h>
#include <hgl/vk/buffer/ActiveRowPool.h>
#include <hgl/log/Log.h>

namespace hgl::graph
{

class DeviceBuffer;
class GraphicsContext;

/**
 * MeshDrawParamsPool —— 全局几何绘制参数行池。
 *
 * 集中管理每个几何体的 MeshDrawParams 行（112 字节）：
 * - 预分配固定上限 Arena 缓冲（默认 16384 行），永不重建重分配，保证 BDA 永不突变；
 * - 行 0 为保留零行；
 * - 提供 Acquire(params) 分配行号 GeometryID，并将绘制拓扑与流 BDA 一次性写入；
 * - 提供 ReleaseID(id) 释放行号；
 * - 提供 GetGPUBase() 获取全局池 BDA 基址，供 SceneBinding UBO 引用。
 */
GRAPH_MODULE_CLASS(MeshDrawParamsPool)
{
private:
    static constexpr uint32_t DefaultMeshDrawParamsCapacity = 16384u;

    ActiveRowPool pool;
    bool pool_initialized = false;

    MeshDrawParamsPool(GraphicsContext *);
    ~MeshDrawParamsPool() = default;

    friend class GraphModuleManager;

    void OnGraphicsContextChanged(GraphicsContext *) override;
    bool InitializePool();

public:
    void Release() override;

    bool IsInitialized() const { return pool_initialized; }

    uint32_t Acquire(const mtl::MeshDrawParams &params);
    bool Write(uint32_t id, const mtl::MeshDrawParams &params);
    bool ReleaseID(uint32_t id);

    bool IsActive(uint32_t id) const;

    uint64_t GetGPUBase() const { return pool.GetGPUBase(); }
    uint64_t GetRowGPU(uint32_t id) const { return pool.RowGPU(id); }
    uint32_t GetCapacity() const { return pool.GetRowCapacity(); }
    uint32_t GetActiveCount() const { return pool.GetActiveCount(); }

    DeviceBuffer *GetBuffer() const { return pool.GetBuffer(); }
};

} // namespace hgl::graph
