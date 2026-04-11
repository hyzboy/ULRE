#pragma once

#include<hgl/graph/module/RuntimeMaterialRequest.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/vk/pipeline/VKGraphicsPipeline.h>
#include<hgl/vk/VKDescriptorSet.h>
#include<hgl/vk/VKMaterialTemplate.h>
#include<hgl/vk/VKMaterialResourceDomain.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VertexAttrib.h>
#include<hgl/graph/mesh/GeometryDataBuffer.h>
#include<hgl/graph/mesh/GeometryDrawRange.h>
#include<hgl/graph/PrimitiveMaterialSlot.h>

namespace hgl::graph{
/**
* 图元(渲染中的最小渲染单位，一个几何体配一个材质)
*/
class Primitive
{
    /// Phase 2c — MI fields inlined directly; mat_inst bridge removed
    MaterialTemplate           *material_template = nullptr;  ///< primary render key
    MaterialResourceDomain     *domain            = nullptr;  ///< data pool domain
    int                         mi_id             = -1;       ///< slot index in domain
    const VIL                  *vil               = nullptr;  ///< vertex input layout
    GraphicsPipelinePreset      render_preset     = GraphicsPipelinePreset::Solid3D;
    mtl::MaterialPreset         material_preset   = mtl::MaterialPreset::Standard;
    int8_t                      mit_slot_offset[mtl::SamplerSlotCount]; ///< per-slot offset into mit_packed (-1 = not active)
    uint32_t                    mit_packed_count  = 0;
    uint32_t                   *mit_packed        = nullptr;

    Geometry *          geometry;

    GeometryDataBuffer *data_buffer;
    GeometryDrawRange   draw_range;

    SemanticMaterialId  deferred_semantic_id = 0;
    uint32_t            deferred_vil_hash    = 0;

private:

    friend Primitive *DirectCreatePrimitive(Geometry *,const PrimitiveMaterialSlot &);

    Primitive(Geometry *,const PrimitiveMaterialSlot &,GeometryDataBuffer *);  // slot-based ctor (no MI object)

    friend Primitive *DirectCreatePrimitive(Geometry *,SemanticMaterialId,uint32_t);

    Primitive(Geometry *,SemanticMaterialId,uint32_t);

public:

    virtual ~Primitive();

            VkPipelineLayout            GetPipelineLayout   ()      { return material_template ? material_template->GetPipelineLayout() : VK_NULL_HANDLE; }
            MaterialTemplate *          GetMaterial         ()      { return material_template; }  ///< @deprecated use GetMaterialTemplate()
            MaterialTemplate *          GetMaterialTemplate ()const { return material_template; }
            MaterialResourceDomain *    GetDomain           ()const { return domain; }
    const   VIL *                       GetVIL              ()const { return vil; }
            int                         GetMIID             ()const { return mi_id; }
            GraphicsPipelinePreset      GetRenderPreset     ()const { return render_preset; }
            void                        SetRenderPreset     (GraphicsPipelinePreset p){ render_preset = p; }
            mtl::MaterialPreset         GetMaterialPreset   ()const { return material_preset; }
            void *                      GetMIData           ()      { return (domain && mi_id >= 0) ? domain->GetMIData(mi_id) : nullptr; }
            void                        WriteMIData         (const void *data, uint32_t size);
    template<typename T>
            void                        WriteMIData         (const T &v){ WriteMIData(&v, sizeof(T)); }
    const   uint32_t                    GetMITDataBytes     ()const { return mit_packed_count * sizeof(uint32_t); }
            void *                      GetMITData          ()      { return mit_packed; }
            void                        InitMITLayout       (uint8_t slot_flags);
            void                        SetTextureArrayLayer(mtl::SamplerSlot slot, uint32_t layer);
            uint32_t                    GetTextureArrayLayer(mtl::SamplerSlot slot) const;
            Geometry *          GetGeometry         (){return geometry;}
            AnsiString          GetGeometryName     (){return geometry->GetName();}
    const   BoundingVolumes &   GetBoundingVolumes  ()const{return geometry->GetBoundingVolumes();}

    const   GeometryDataBuffer *GetDataBuffer       ()const{return data_buffer;}
    const   GeometryDrawRange * GetRenderData       ()const{return &draw_range;}

            VAB *               GetVAB              (const int index)const{return geometry->GetVAB(index);}
            VAB *               GetVAB              (const VertexAttrib attrib)const{return geometry->GetVAB(attrib);}
            IndexBuffer *       GetIBO              (){return geometry->GetIBO();}

    virtual bool                UpdateGeometry      ();     ///<更新Geometry,一般用于Geometry改变数据后需要通知Mesh的情况

public:

            bool                HasDeferredMI       ()const{return material_template==nullptr&&deferred_semantic_id!=0;}
            SemanticMaterialId  GetDeferredSemanticId()const{return deferred_semantic_id;}
            uint32_t            GetDeferredVILHash  ()const{return deferred_vil_hash;}

            /// 绑定材质槽（Phase 2c）：替代 BindMaterialInstance/ChangeMaterialInstance。
            /// 用于延迟绑定（HasDeferredMI()==true 时会创建 GeometryDataBuffer）
            /// 以及每帧的变体更新（透明度变化等）。
            bool                BindMaterialSlot(const PrimitiveMaterialSlot &slot,const char *source_tag=nullptr);

            // 设置绘制数量（vertex/index），若大于数据量会被裁剪至数据量
            bool                SetDrawCounts(uint32_t draw_vertex_count,uint32_t draw_index_count=0);

            // 设置绘制范围和数量
            bool                SetDrawRange(int32_t vertex_offset,uint32_t first_index,uint32_t draw_vertex_count,uint32_t draw_index_count=0);

            // 取得缓冲区实际数据数量
            uint32_t            GetDataVertexCount()const{return draw_range.data_vertex_count;}
            uint32_t            GetDataIndexCount()const{return draw_range.data_index_count;}
};//class Primitive

Primitive *DirectCreatePrimitive(Geometry *,const PrimitiveMaterialSlot &);
Primitive *DirectCreatePrimitive(Geometry *,SemanticMaterialId,uint32_t vil_hash);
}//namespace hgl::graph
