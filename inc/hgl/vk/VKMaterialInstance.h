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

    MaterialTemplate *      material          = nullptr;
    MaterialManager *       domain_resolver   = nullptr; ///< non-owning back-ref to manager, for domain lookup (Phase E)
    uint32_t                domain_id         = 0;       ///< index into MaterialManager::domain_table_ (0 = invalid)
    uint32_t                domain_generation = 0;       ///< generation at time of MI creation
    bool                    owns_slot         = false;   ///< true when this MI is responsible for releasing mi_id back to domain
    const VIL *             vil               = nullptr;
    int                     mi_id             = -1;
    GraphicsPipelinePreset  render_preset     = GraphicsPipelinePreset::Solid3D;

    int8_t                  mit_slot_offset[mtl::SamplerSlotCount];
    uint32_t                mit_packed_count = 0;
    uint32_t *              mit_packed       = nullptr;

public:

    MaterialInstance();
    MaterialInstance(const PrimitiveMaterialSlot &slot);
    ~MaterialInstance();

    // Legacy semantic getter: prefer carrying material_template in PrimitiveMaterialSlot/Primitive instead.
    MaterialTemplate *GetMaterial() const { return material; }
    MaterialResourceDomain *GetDomain() const;  ///< resolves domain_id+generation through domain_resolver (Phase E)
    uint32_t GetDomainID()         const { return domain_id; }
    uint32_t GetDomainGeneration() const { return domain_generation; }
    // Legacy semantic getter: prefer carrying VIL in PrimitiveMaterialSlot/Primitive instead.
    const VIL *GetVIL() const { return vil; }
    int GetMIID() const { return mi_id; }
    // Legacy semantic getters/setters: prefer carrying preset in PrimitiveMaterialSlot/Primitive instead.
    GraphicsPipelinePreset GetRenderPreset() const { return render_preset; }
    void SetRenderPreset(GraphicsPipelinePreset p) { render_preset = p; }

    /// 转换为 PrimitiveMaterialSlot，供 DirectCreatePrimitive/PrimitiveManager::CreatePrimitive 使用（Phase C）
    PrimitiveMaterialSlot ToSlot() const
    {
        return {
            material,
            GetDomain(),
            mi_id,
            vil,
            render_preset,
            uint8_t(mit_packed_count > 0 ? material ? material->GetTextureArraySlotFlags() : 0 : 0),
            mit_packed,
            mit_packed_count
        };
    }

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
