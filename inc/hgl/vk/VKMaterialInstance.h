#ifndef HGL_VK_MATERIAL_INSTANCE_H
#define HGL_VK_MATERIAL_INSTANCE_H

#include <cstring>

#include <hgl/graph/PrimitiveMaterialSlot.h>
#include <hgl/vk/VKMaterialTemplate.h>

namespace hgl::graph {

class MaterialManager;
class Primitive;
class MaterialAssetRegistry;

class MaterialInstance
{
    friend class Primitive;
    friend class MaterialManager;
    friend class MaterialAssetRegistry;

    MaterialTemplate *      material         = nullptr;
    MaterialResourceDomain *domain           = nullptr;
    const VIL *             vil              = nullptr;
    int                     mi_id            = -1;
    GraphicsPipelinePreset  render_preset    = GraphicsPipelinePreset::Solid3D;

    int8_t                  mit_slot_offset[mtl::SamplerSlotCount];
    uint32_t                mit_packed_count = 0;
    uint32_t *              mit_packed       = nullptr;

public:

    MaterialInstance();
    MaterialInstance(const PrimitiveMaterialSlot &slot);
    ~MaterialInstance();

    MaterialTemplate *GetMaterial() const { return material; }
    MaterialResourceDomain *GetDomain() const { return domain; }
    const VIL *GetVIL() const { return vil; }
    int GetMIID() const { return mi_id; }
    GraphicsPipelinePreset GetRenderPreset() const { return render_preset; }
    void SetRenderPreset(GraphicsPipelinePreset p) { render_preset = p; }

    /// 转换为 PrimitiveMaterialSlot，供 DirectCreatePrimitive/PrimitiveManager::CreatePrimitive 使用（Phase C）
    PrimitiveMaterialSlot ToSlot() const { return { material, domain, mi_id, vil, render_preset }; }

    void *GetMIData();
    void WriteMIData(const void *data, uint32_t size);

    template <typename T>
    void WriteMIData(const T &data) { WriteMIData(&data, sizeof(T)); }

    void InitMITLayout(uint8_t slot_flags);
    void SetTextureArrayLayer(mtl::SamplerSlot slot, uint32_t layer);
    uint32_t GetTextureArrayLayer(mtl::SamplerSlot slot) const;
};

} // namespace hgl::graph

#endif
