#pragma once

#include <hgl/graph/module/GraphModule.h>
#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <hgl/type/UnorderedMap.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/buffer/ActiveArrayView.h>

namespace hgl::graph
{
class DeviceBuffer;
class IGPUBuffer;

struct MaterialSSBOBufferBinding
{
    mtl::MaterialSSBOType material_ssbo_type =
        mtl::MaterialSSBOType::PBRSurface;
    uint32_t ssbo_id = 0;
    DeviceBuffer *buffer = nullptr;
    uint32_t element_capacity = 0;
    uint32_t element_stride = 0;
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

GRAPH_MODULE_CLASS(MaterialSSBOBufferRegistry)
{
private:
    static constexpr uint32_t DefaultMaterialDataElementCapacity = 1024u;

    hgl::UnorderedMap<uint32_t, MaterialSSBOBufferBinding> material_domain_map;
    hgl::UnorderedMap<uint32_t, MaterialRowBufferInfo> row_buffers;
    ActiveArrayView<ssbo::PBRSurfaceRow> pbr_surface_rows;
    ActiveArrayView<ssbo::EmissiveSurfaceRow> emissive_surface_rows;
    ActiveArrayView<ssbo::TransmissionSurfaceRow> transmission_surface_rows;
    uint32_t next_ssbo_id = 1;

private:
    uint32_t AllocateMaterialSSBOId()
    {
        return mtl::MakeRecipeSSBOId(next_ssbo_id++);
    }

    MaterialSSBOBufferRegistry(GraphicsContext *);
    ~MaterialSSBOBufferRegistry() = default;

    friend class GraphModuleManager;

    bool BindMaterialDataAccessor(
        mtl::MaterialSSBOType material_type,
        uint32_t ssbo_id,
        DeviceBuffer *buffer,
        uint32_t element_count,
        void *&out_cpu_base);
    void ResetMaterialDataAccessors();

    ActiveArrayView<ssbo::PBRSurfaceRow> *GetMaterialDataAccessorForRow(
        ssbo::PBRSurfaceRow *)
    {
        return &pbr_surface_rows;
    }

    ActiveArrayView<ssbo::EmissiveSurfaceRow> *GetMaterialDataAccessorForRow(
        ssbo::EmissiveSurfaceRow *)
    {
        return &emissive_surface_rows;
    }

    ActiveArrayView<ssbo::TransmissionSurfaceRow> *GetMaterialDataAccessorForRow(
        ssbo::TransmissionSurfaceRow *)
    {
        return &transmission_surface_rows;
    }

public:
    bool TryGetRowBuffer(uint32_t ssbo_id, MaterialRowBufferInfo &out_info) const
    {
        const auto *buffer_info = row_buffers.GetValuePointer(ssbo_id);
        if (!buffer_info)
            return false;

        out_info = *buffer_info;
        return true;
    }

    void Release() override;

    bool RegisterMaterialBuffer(const mtl::MaterialSSBOType material_type,
                                DeviceBuffer *buffer,
                                uint32_t element_capacity);
    bool TryGetMaterialBinding(const mtl::MaterialSSBOType material_type,
                               MaterialSSBOBufferBinding &out_binding) const;
    DeviceBuffer *GetMaterialBuffer(const mtl::MaterialSSBOType material_type) const;
    const IGPUBuffer *GetMaterialGPUBuffer(const mtl::MaterialSSBOType material_type) const;
    uint32_t GetMaterialElementCapacity(const mtl::MaterialSSBOType material_type) const;
    uint32_t GetMaterialSSBOId(const mtl::MaterialSSBOType material_type) const;
    bool IsMaterialDataIDActive(const mtl::MaterialSSBOType material_type,
                                uint32_t data_id) const;

    bool EnsureMaterialDataSSBOs();
    bool EnsureMaterialDataSSBO(const mtl::MaterialSSBOType material_type,
                                const AnsiString &name,
                                uint32_t element_count,
                                SharingMode sm = SharingMode::Exclusive);

    /**
     * Returns the shared, ID-managed accessor for T's MaterialSSBOType.
     * Each AcquireID() result is a stable row index in that type's sole
     * physical backing SSBO until ReleaseID() returns it to the pool.
     */
    template<typename T>
    ActiveArrayView<T>* GetMaterialDataAccessor(
        const AnsiString &name = AnsiString(),
        uint32_t minimum_capacity = 1,
        SharingMode sm = SharingMode::Exclusive)
    {
        const auto material_type = ssbo::MaterialRowTypeTraits<T>::TYPE;

        AnsiString actual_name = name;
        if (actual_name.IsEmpty())
            actual_name = AnsiString("MaterialData:") + ssbo::GetMaterialSSBOBufferName(material_type);

        if (!EnsureMaterialDataSSBO(
                material_type,
                actual_name,
                minimum_capacity,
                sm))
            return nullptr;

        return GetMaterialDataAccessorForRow(static_cast<T *>(nullptr));
    }
};

} // namespace hgl::graph
