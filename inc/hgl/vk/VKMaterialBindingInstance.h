#pragma once

#include<hgl/vk/VKShaderMaterialProgram.h>
#include<hgl/vk/VKResourceDomain.h>
#include<hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include<hgl/mtl/SamplerSlot.h>

namespace hgl::graph{

/**
* 材质实例类<br>
* 材质实例类本质只是提供一个数据区，供RenderCollector合并成一个大UBO。
*
* ResourceDomain 为显式必需：
*   - MI 数据统一由指定 ResourceDomain 管理。
*   - 无 MI schema 的材质也会绑定一个 schema=None 的 domain，用于统一创建入口。
*/
class MaterialBindingInstance
{
protected:

    ShaderMaterialProgram *material;

    ResourceDomain *domain;     ///< 显式资源域（统一 MI 创建入口）

    uint32_t domain_id = 0xFFFFFFFFu;

    const VIL *vil;

    int mi_id;

    GraphicsPipelinePreset render_preset = GraphicsPipelinePreset::Solid3D;  ///< PreRaster 配置（Phase 0: batch 阶段自动解析用）

    int8_t   mit_slot_offset[mtl::SamplerSlotCount]; ///< per-slot offset into mit_packed (-1 = not active)
    uint32_t mit_packed_count = 0;              ///< number of active Array slots
    uint32_t *mit_packed      = nullptr;        ///< packed layer indices (one uint32 per active slot)

public:

            ShaderMaterialProgram *      GetShaderMaterialProgram()      { return material; }
            ResourceDomain *GetDomain   ()      { return domain; }
    const   uint32_t        GetDomainID ()const { return domain_id; }

    const   VIL *           GetVIL      ()const { return vil; }

private:

    friend class ShaderMaterialProgram;
    friend class ResourceDomain;
    friend class ShaderMaterialProgramManager;

    /// 新路径构造（Phase 1，经由 ResourceDomain 分配槽位）
    MaterialBindingInstance(ShaderMaterialProgram *, ResourceDomain *, const VIL *, const int);

public:

    virtual ~MaterialBindingInstance();

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
};//class MaterialBindingInstanceData
}//namespace hgl::graph

