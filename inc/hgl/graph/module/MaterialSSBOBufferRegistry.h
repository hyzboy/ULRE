#pragma once

#include <hgl/graph/module/GraphModule.h>
#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <hgl/log/Log.h>
#include <hgl/type/ActiveIDManager.h>
#include <hgl/vk/VKDevice.h>

namespace hgl::graph
{
class DeviceBuffer;
class IGPUBuffer;
class MaterialSSBOBufferRegistry;

template<typename T>
class MaterialSSBODataAccessor
{
public:
    using DataID = uint32_t;
    static constexpr DataID InvalidDataID = ~DataID(0);

private:
    MaterialSSBOBufferRegistry *registry = nullptr;
    mtl::MaterialSSBOType material_ssbo_type =
        mtl::MaterialSSBOType::PBRSurface;
    DataID data_id = InvalidDataID;
    uint32_t ssbo_id = 0;
    T *data = nullptr;

    MaterialSSBODataAccessor(
        MaterialSSBOBufferRegistry *in_registry,
        const mtl::MaterialSSBOType in_material_ssbo_type,
        const DataID in_data_id,
        const uint32_t in_ssbo_id,
        T *in_data)
        : registry(in_registry)
        , material_ssbo_type(in_material_ssbo_type)
        , data_id(in_data_id)
        , ssbo_id(in_ssbo_id)
        , data(in_data)
    {
    }

    void Invalidate()
    {
        registry = nullptr;
        data_id = InvalidDataID;
        ssbo_id = 0;
        data = nullptr;
    }

    friend class MaterialSSBOBufferRegistry;

public:
    MaterialSSBODataAccessor() = default;
    ~MaterialSSBODataAccessor()
    {
        Release();
    }

    MaterialSSBODataAccessor(const MaterialSSBODataAccessor &) = delete;
    MaterialSSBODataAccessor &operator=(
        const MaterialSSBODataAccessor &) = delete;

    MaterialSSBODataAccessor(MaterialSSBODataAccessor &&other) noexcept;
    MaterialSSBODataAccessor &operator=(
        MaterialSSBODataAccessor &&other) noexcept;

    bool IsValid() const
    {
        return registry != nullptr
            && data != nullptr
            && data_id != InvalidDataID
            && ssbo_id != 0;
    }
    operator bool() const { return IsValid(); }

    DataID GetDataID() const { return data_id; }
    uint32_t GetSSBOId() const { return ssbo_id; }
    mtl::MaterialSSBOType GetMaterialSSBOType() const
    {
        return material_ssbo_type;
    }
    mtl::MaterialSSBOBinding GetMaterialSSBOBinding() const
    {
        return {material_ssbo_type, ssbo_id, data_id};
    }

    T *Get() { return data; }
    const T *Get() const { return data; }
    T *operator->() { return data; }
    const T *operator->() const { return data; }
    T &operator*() { return *data; }
    const T &operator*() const { return *data; }

    bool Write(const T &value);
    bool Read(T &out_value) const;
    bool Commit();
    void Release();
};

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
    static constexpr uint32_t MaterialSSBOTypeCount =
        static_cast<uint32_t>(mtl::MaterialSSBOType::RANGE_SIZE);
    static constexpr uint32_t DefaultMaterialDataElementCapacity = 1024u;

    struct MaterialSSBOBufferStorage
    {
        DeviceBuffer *buffer = nullptr;
        void *cpu_base = nullptr;
        uint64_t gpu_base = 0;
        uint32_t ssbo_id = 0;
        uint32_t row_bytes = 0;
        uint32_t row_capacity = 0;
    };

    MaterialSSBOBufferStorage material_buffers[MaterialSSBOTypeCount] = {};
    ActiveIDManager active_id_managers[MaterialSSBOTypeCount];
    bool material_data_buffers_initialized = false;

private:
    static uint32_t GetMaterialSSBOTypeIndex(
        mtl::MaterialSSBOType material_type)
    {
        return static_cast<uint32_t>(material_type)
            - static_cast<uint32_t>(mtl::MaterialSSBOType::BEGIN_RANGE);
    }

    MaterialSSBOBufferStorage *GetMaterialBufferStorage(
        mtl::MaterialSSBOType material_type)
    {
        if (!mtl::IsMaterialSSBOType(material_type))
            return nullptr;

        return material_buffers + GetMaterialSSBOTypeIndex(material_type);
    }

    const MaterialSSBOBufferStorage *GetMaterialBufferStorage(
        mtl::MaterialSSBOType material_type) const
    {
        if (!mtl::IsMaterialSSBOType(material_type))
            return nullptr;

        return material_buffers + GetMaterialSSBOTypeIndex(material_type);
    }

    ActiveIDManager *GetMaterialDataIDManager(
        mtl::MaterialSSBOType material_type)
    {
        if (!mtl::IsMaterialSSBOType(material_type))
            return nullptr;

        return active_id_managers + GetMaterialSSBOTypeIndex(material_type);
    }

    const ActiveIDManager *GetMaterialDataIDManager(
        mtl::MaterialSSBOType material_type) const
    {
        if (!mtl::IsMaterialSSBOType(material_type))
            return nullptr;

        return active_id_managers + GetMaterialSSBOTypeIndex(material_type);
    }

    MaterialSSBOBufferRegistry(GraphicsContext *);
    ~MaterialSSBOBufferRegistry() = default;

    friend class GraphModuleManager;
    template<typename T> friend class MaterialSSBODataAccessor;

    void OnGraphicsContextChanged(GraphicsContext *) override;
    bool InitializeMaterialDataBuffers();
    bool CreateMaterialDataBuffer(mtl::MaterialSSBOType material_type,
                                  const AnsiString &name);
    bool AcquireMaterialDataID(mtl::MaterialSSBOType material_type,
                               uint32_t &out_data_id);
    bool ReleaseMaterialDataID(mtl::MaterialSSBOType material_type,
                               uint32_t data_id);
    bool CommitMaterialData(mtl::MaterialSSBOType material_type,
                            uint32_t data_id);

public:
    bool TryGetRowBuffer(uint32_t ssbo_id, MaterialRowBufferInfo &out_info) const;

    void Release() override;

    bool TryGetMaterialBinding(const mtl::MaterialSSBOType material_type,
                               MaterialSSBOBufferBinding &out_binding) const;
    DeviceBuffer *GetMaterialBuffer(const mtl::MaterialSSBOType material_type) const;
    const IGPUBuffer *GetMaterialGPUBuffer(const mtl::MaterialSSBOType material_type) const;
    uint32_t GetMaterialElementCapacity(const mtl::MaterialSSBOType material_type) const;
    uint32_t GetMaterialSSBOId(const mtl::MaterialSSBOType material_type) const;
    bool IsMaterialDataIDActive(const mtl::MaterialSSBOType material_type,
                                uint32_t data_id) const;
    bool IsInitialized() const { return material_data_buffers_initialized; }

    /**
     * Acquires one row in T's pre-created shared material SSBO. The returned
     * accessor owns that row ID and automatically returns it on destruction.
     * Keep it alive while a recipe or component references GetDataID().
     */
    template<typename T>
    MaterialSSBODataAccessor<T> GetMaterialDataAccessor()
    {
        constexpr auto material_type = ssbo::MaterialRowTypeTraits<T>::TYPE;
        const MaterialSSBOBufferStorage *storage =
            GetMaterialBufferStorage(material_type);
        if (!material_data_buffers_initialized
         || !storage
         || !storage->buffer
         || !storage->cpu_base
         || storage->ssbo_id == 0)
        {
            GLogError(
                "[MaterialSSBOBufferRegistry] Material data accessor requested before initialization: type=%s",
                mtl::GetMaterialSSBOTypeName(material_type));
            return {};
        }

        if (storage->row_bytes != sizeof(T))
        {
            GLogError(
                "[MaterialSSBOBufferRegistry] Material data accessor row-size mismatch: type=%s expected=%u actual=%zu",
                mtl::GetMaterialSSBOTypeName(material_type),
                storage->row_bytes,
                static_cast<size_t>(sizeof(T)));
            return {};
        }

        uint32_t data_id = MaterialSSBODataAccessor<T>::InvalidDataID;
        if (!AcquireMaterialDataID(material_type, data_id))
            return {};

        T *data = static_cast<T *>(storage->cpu_base) + data_id;
        return MaterialSSBODataAccessor<T>(
            this,
            material_type,
            data_id,
            storage->ssbo_id,
            data);
    }
};

template<typename T>
MaterialSSBODataAccessor<T>::MaterialSSBODataAccessor(
    MaterialSSBODataAccessor &&other) noexcept
    : registry(other.registry)
    , material_ssbo_type(other.material_ssbo_type)
    , data_id(other.data_id)
    , ssbo_id(other.ssbo_id)
    , data(other.data)
{
    other.Invalidate();
}

template<typename T>
MaterialSSBODataAccessor<T> &MaterialSSBODataAccessor<T>::operator=(
    MaterialSSBODataAccessor &&other) noexcept
{
    if (this != &other)
    {
        Release();
        registry = other.registry;
        material_ssbo_type = other.material_ssbo_type;
        data_id = other.data_id;
        ssbo_id = other.ssbo_id;
        data = other.data;
        other.Invalidate();
    }
    return *this;
}

template<typename T>
bool MaterialSSBODataAccessor<T>::Write(const T &value)
{
    if (!IsValid())
    {
        GLogError("[MaterialSSBODataAccessor] Write failed: invalid accessor");
        return false;
    }

    *data = value;
    return Commit();
}

template<typename T>
bool MaterialSSBODataAccessor<T>::Read(T &out_value) const
{
    if (!IsValid())
    {
        GLogError("[MaterialSSBODataAccessor] Read failed: invalid accessor");
        return false;
    }

    out_value = *data;
    return true;
}

template<typename T>
bool MaterialSSBODataAccessor<T>::Commit()
{
    if (!IsValid())
    {
        GLogError("[MaterialSSBODataAccessor] Commit failed: invalid accessor");
        return false;
    }

    return registry->CommitMaterialData(material_ssbo_type, data_id);
}

template<typename T>
void MaterialSSBODataAccessor<T>::Release()
{
    if (registry && data_id != InvalidDataID)
        registry->ReleaseMaterialDataID(material_ssbo_type, data_id);

    Invalidate();
}
} // namespace hgl::graph
