#pragma once

#include<hgl/vk/VKMaterialTemplate.h>
#include<hgl/vk/VKMaterialResourceDomain.h>
#include<hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include<hgl/mtl/SamplerSlot.h>

namespace hgl::graph{

class MaterialManager;

/**
* 材质实例类<br>
* 材质实例类本质只是提供一个数据区，供RenderCollector合并成一个大UBO。
*
* Phase 1: 支持可选 MaterialResourceDomain。
*   - domain == nullptr : MI 数据由 MaterialTemplate 自身的数据池管理（旧路径）。
*   - domain != nullptr : MI 数据由指定 MaterialResourceDomain 的独立数据池管理（新路径）。
*/
class MaterialInstance
{
protected:

    MaterialManager *material_manager;

    MaterialResourceDomain *domain;     ///< (Phase 1) 可选资源域；为 nullptr 时使用旧路径

    const VIL *vil;

    int mi_id;

    GraphicsPipelinePreset render_preset = GraphicsPipelinePreset::Solid3D;  ///< PreRaster 配置（Phase 0: batch 阶段自动解析用）

    int8_t   mit_slot_offset[mtl::SamplerSlotCount]; ///< per-slot offset into mit_packed (-1 = not active)
    uint32_t mit_packed_count = 0;              ///< number of active Array slots
    uint32_t *mit_packed      = nullptr;        ///< packed layer indices (one uint32 per active slot)

public:

            MaterialTemplate *      GetMaterial ()const;
            MaterialResourceDomain *GetDomain   ()      { return domain; }

    const   VIL *           GetVIL      ()const { return vil; }

private:

    friend class MaterialTemplate;
    friend class MaterialResourceDomain;
    friend class MaterialManager;
    friend class Primitive;

    /// 旧路径构造（domain = nullptr）
    MaterialInstance(MaterialManager *, const VIL *, const int);

    /// 新路径构造（Phase 1，经由 MaterialResourceDomain 分配槽位）
    MaterialInstance(MaterialManager *, MaterialResourceDomain *, const VIL *, const int);

public:

    virtual ~MaterialInstance();

    const   int     GetMIID     ()const{return mi_id;}

            void                SetRenderPreset(GraphicsPipelinePreset p){render_preset=p;}
            GraphicsPipelinePreset GetRenderPreset()const{return render_preset;}

            void *  GetMIData   ();                                         ///<取得材质实例数据
            void    WriteMIData (const void *data,const uint32 size);       ///<写入材质实例数据

        template<typename T>
            void    WriteMIData (const T &data){WriteMIData(&data,sizeof(T));}

    const   uint32_t    GetMITDataBytes() const { return mit_packed_count * sizeof(uint32_t); }
            void*       GetMITData()            { return mit_packed; }

            void        InitMITLayout(uint8_t slot_flags);                  ///<根据slot_flags初始化per-slot偏移表并分配packed数组
            void        SetTextureArrayLayer(mtl::SamplerSlot slot, uint32_t layer);
            uint32_t    GetTextureArrayLayer(mtl::SamplerSlot slot) const;
};//class MaterialInstanceData
}//namespace hgl::graph

