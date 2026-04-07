#pragma once

#include<hgl/graph/module/RuntimeMaterialRequest.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/vk/pipeline/VKGraphicsPipeline.h>
#include<hgl/vk/VKDescriptorSet.h>
#include<hgl/vk/VKMaterialTemplate.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/vk/VertexAttrib.h>
#include<hgl/graph/mesh/GeometryDataBuffer.h>
#include<hgl/graph/mesh/GeometryDrawRange.h>

namespace hgl::graph{
/**
* 图元(渲染中的最小渲染单位，一个几何体配一个材质)
*/
class Primitive
{
    /// Phase 2a — MI 字段直接内联（Phase 2c 前 mat_inst 保留作桥接）
    MaterialTemplate           *material_template = nullptr;  ///< primary render key
    MaterialResourceDomain     *domain            = nullptr;  ///< data pool domain
    int                         mi_id             = -1;       ///< slot index in domain
    const VIL                  *vil               = nullptr;  ///< vertex input layout
    GraphicsPipelinePreset      render_preset     = GraphicsPipelinePreset::Solid3D;
    int8_t                      mit_slot_offset[mtl::SamplerSlotCount]; ///< per-slot offset into mit_packed (-1 = not active)
    uint32_t                    mit_packed_count  = 0;
    uint32_t                   *mit_packed        = nullptr;

    MaterialInstance *          mat_inst          = nullptr;  ///< @deprecated Phase 2c bridge
    Geometry *          geometry;

    GeometryDataBuffer *data_buffer;
    GeometryDrawRange   draw_range;

    SemanticMaterialId  deferred_semantic_id = 0;
    uint32_t            deferred_vil_hash    = 0;

private:

    friend Primitive *DirectCreatePrimitive(Geometry *,MaterialInstance *,GraphicsPipelinePreRaster *);

    Primitive(Geometry *,MaterialInstance *,GraphicsPipelinePreRaster *,GeometryDataBuffer *);

    friend Primitive *DirectCreatePrimitive(Geometry *,SemanticMaterialId,uint32_t);

    Primitive(Geometry *,SemanticMaterialId,uint32_t);

public:

    virtual ~Primitive();

            VkPipelineLayout            GetPipelineLayout   ()      { return material_template ? material_template->GetPipelineLayout() : VK_NULL_HANDLE; }
            MaterialTemplate *          GetMaterial         ()      { return material_template; }  ///< @deprecated Phase 2c: use GetMaterialTemplate()
            MaterialInstance *          GetMaterialInstance ()      { return mat_inst; }           ///< @deprecated Phase 2c
            MaterialTemplate *          GetMaterialTemplate ()const { return material_template; }
            MaterialResourceDomain *    GetDomain           ()const { return domain; }
    const   VIL *                       GetVIL              ()const { return vil; }
            int                         GetMIID             ()const { return mi_id; }
            GraphicsPipelinePreset      GetRenderPreset     ()const { return render_preset; }
            void                        SetRenderPreset     (GraphicsPipelinePreset p){ render_preset = p; }
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

            /// 延迟绑定：仅在 mat_inst==nullptr 时有效，由 ECS 收集系统在 ResolveMI 后调用
            bool                BindMaterialInstance(MaterialInstance *mi);

            bool                ChangeMaterialInstance(MaterialInstance *mi)
            {
                if(!mi)
                    return(false);

                if(!mat_inst)
                {
                    mat_inst=mi;
                    material_template=mi->GetMaterial();
                    domain=mi->GetDomain();
                    mi_id=mi->GetMIID();
                    vil=mi->GetVIL();
                    render_preset=mi->GetRenderPreset();
                    return(true);
                }

                if(mi->GetMaterial()!=mat_inst->GetMaterial())      //不能换母材质
                    return(false);

                mat_inst=mi;
                material_template=mi->GetMaterial();
                domain=mi->GetDomain();
                mi_id=mi->GetMIID();
                vil=mi->GetVIL();
                render_preset=mi->GetRenderPreset();
                return(true);
            }

            // 设置绘制数量（vertex/index），若大于数据量会被裁剪至数据量
            bool                SetDrawCounts(uint32_t draw_vertex_count,uint32_t draw_index_count=0);

            // 设置绘制范围和数量
            bool                SetDrawRange(int32_t vertex_offset,uint32_t first_index,uint32_t draw_vertex_count,uint32_t draw_index_count=0);

            // 取得缓冲区实际数据数量
            uint32_t            GetDataVertexCount()const{return draw_range.data_vertex_count;}
            uint32_t            GetDataIndexCount()const{return draw_range.data_index_count;}
};//class Primitive

Primitive *DirectCreatePrimitive(Geometry *,MaterialInstance *,GraphicsPipelinePreRaster * = nullptr);
Primitive *DirectCreatePrimitive(Geometry *,SemanticMaterialId,uint32_t vil_hash);
}//namespace hgl::graph
