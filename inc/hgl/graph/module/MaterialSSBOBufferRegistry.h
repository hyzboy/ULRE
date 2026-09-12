#pragma once

#include <hgl/graph/module/GraphModule.h>
#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <hgl/log/Log.h>
#include <hgl/vk/buffer/ActiveRowPool.h>
#include <hgl/vk/buffer/ActiveRowLease.h>

namespace hgl::graph
{

class DeviceBuffer;
class IGPUBuffer;
class MaterialSSBOBufferRegistry;

/**
 * MaterialSSBODataAccessor —— 材质字段行的 RAII 访问器（**非模板：统一按字节**）。
 *
 * = 通用行租约 ActiveRowLease（关联池 + 申请行号 + 析构归还）
 *   + 材质身份（MaterialSSBOType / ssbo_id，供 recipe binding 使用）。
 *
 * 行内容按字节访问，**需要结构体时在使用点指定类型**：
 *   acc.Write(row);                      // 类型由实参推导（写 sizeof(T) 字节 + 按行提交）
 *   auto *row = acc.GetAs<PBRSurfaceRow>();   // 需要指针时显式指定
 * 不再绑定 registry：任何持有对应 ActiveRowPool 的地方都能造。
 */
class MaterialSSBODataAccessor : public ActiveRowLease
{
    mtl::MaterialSSBOType material_ssbo_type =
        mtl::MaterialSSBOType::PBRSurface;
    uint32_t ssbo_id = 0;

    MaterialSSBODataAccessor(
        ActiveRowPool *pool,
        const mtl::MaterialSSBOType in_material_ssbo_type,
        const uint32_t in_ssbo_id)
        : ActiveRowLease(pool)
        , material_ssbo_type(in_material_ssbo_type)
        , ssbo_id(in_ssbo_id)
    {
    }

    friend class MaterialSSBOBufferRegistry;

public:
    MaterialSSBODataAccessor() = default;
    MaterialSSBODataAccessor(MaterialSSBODataAccessor &&) noexcept = default;
    MaterialSSBODataAccessor &operator=(
        MaterialSSBODataAccessor &&) noexcept = default;

    uint32_t GetDataID() const { return GetRowID(); }
    uint32_t GetSSBOId() const { return ssbo_id; }
    mtl::MaterialSSBOType GetMaterialSSBOType() const
    {
        return material_ssbo_type;
    }
    mtl::MaterialSSBOBinding GetMaterialSSBOBinding() const
    {
        return {material_ssbo_type, ssbo_id, GetRowID()};
    }
};

struct MaterialRowBufferInfo
{
    mtl::MaterialSSBOType material_ssbo_type =
        mtl::MaterialSSBOType::PBRSurface;
    void *cpu_base = nullptr;
    uint64_t gpu_base = 0;
    uint32_t row_bytes = 0;
    uint32_t row_capacity = 0;
    DeviceBuffer *buffer = nullptr;
};

/**
 * MaterialSSBOBufferRegistry —— 每个材质字段类型一个行池（ActiveRowPool）。
 *
 * 创建期：ENUM_CLASS_FOR 按类型遍历，只按 (行距, 1024) 建 Buffer；
 * 使用期：GetMaterialDataAccessor(type) 借池视图 + 造租约（见 ActiveRowLease）。
 */
GRAPH_MODULE_CLASS(MaterialSSBOBufferRegistry)
{
private:
    static constexpr uint32_t MaterialSSBOTypeCount =
        static_cast<uint32_t>(mtl::MaterialSSBOType::RANGE_SIZE);
    static constexpr uint32_t DefaultMaterialDataElementCapacity = 1024u;

    ActiveRowPool material_row_pools[MaterialSSBOTypeCount];
    bool material_data_buffers_initialized = false;

private:
    static uint32_t GetMaterialSSBOTypeIndex(
        mtl::MaterialSSBOType material_type)
    {
        return static_cast<uint32_t>(material_type)
            - static_cast<uint32_t>(mtl::MaterialSSBOType::BEGIN_RANGE);
    }

    ActiveRowPool *GetMaterialRowPool(
        mtl::MaterialSSBOType material_type)
    {
        if (!mtl::IsMaterialSSBOType(material_type))
            return nullptr;

        return material_row_pools + GetMaterialSSBOTypeIndex(material_type);
    }

    const ActiveRowPool *GetMaterialRowPool(
        mtl::MaterialSSBOType material_type) const
    {
        if (!mtl::IsMaterialSSBOType(material_type))
            return nullptr;

        return material_row_pools + GetMaterialSSBOTypeIndex(material_type);
    }

    MaterialSSBOBufferRegistry(GraphicsContext *);
    ~MaterialSSBOBufferRegistry() = default;

    friend class GraphModuleManager;

    void OnGraphicsContextChanged(GraphicsContext *) override;
    bool InitializeMaterialDataBuffers();
    bool CreateMaterialDataBuffer(mtl::MaterialSSBOType material_type,
                                  const AnsiString &name);

public:
    bool TryGetRowBuffer(uint32_t ssbo_id, MaterialRowBufferInfo &out_info) const;

    void Release() override;

    bool IsMaterialDataIDActive(const mtl::MaterialSSBOType material_type,
                                uint32_t data_id) const;
    bool IsInitialized() const { return material_data_buffers_initialized; }

    /**
     * Acquires one row in the given material field SSBO. The returned accessor
     * owns that row ID and automatically returns it on destruction.
     * Keep it alive while a recipe or component references GetDataID().
     *
     * 行内容按字节访问：需要结构体时在使用点指定类型（acc.Write(row) / acc.GetAs<Row>()）。
     */
    MaterialSSBODataAccessor GetMaterialDataAccessor(
        const mtl::MaterialSSBOType material_type)
    {
        ActiveRowPool *pool = GetMaterialRowPool(material_type);

        if (!material_data_buffers_initialized
         || !pool
         || !pool->IsReady()
         || pool->GetSSBOId() == 0)
        {
            GLogError(
                "[MaterialSSBOBufferRegistry] Material data accessor requested before initialization: type=%s",
                mtl::GetMaterialSSBOTypeName(material_type));
            return {};
        }

        return MaterialSSBODataAccessor(
            pool,
            material_type,
            pool->GetSSBOId());
    }

    /** 便捷：由行类型反查池类型（T → MaterialRowTypeTraits<T>::TYPE）。 */
    template<typename T>
    MaterialSSBODataAccessor GetMaterialDataAccessor()
    {
        MaterialSSBODataAccessor accessor =
            GetMaterialDataAccessor(ssbo::MaterialRowTypeTraits<T>::TYPE);

        if (accessor && accessor.GetRowBytes() != sizeof(T))
        {
            GLogError(
                "[MaterialSSBOBufferRegistry] Material data accessor row-size mismatch: type=%s expected=%u actual=%zu",
                mtl::GetMaterialSSBOTypeName(
                    ssbo::MaterialRowTypeTraits<T>::TYPE),
                accessor.GetRowBytes(),
                static_cast<size_t>(sizeof(T)));
            return {};
        }

        return accessor;
    }
};
} // namespace hgl::graph
