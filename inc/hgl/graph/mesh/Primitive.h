#pragma once

#include<hgl/graph/module/RuntimeMaterialRequest.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/vk/pipeline/VKGraphicsPipeline.h>
#include<hgl/vk/VKDescriptorSet.h>
#include<hgl/vk/VKShaderProgram.h>
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
    MaterialInstance *  mat_inst;
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

            VkPipelineLayout    GetPipelineLayout   (){return mat_inst ? mat_inst->GetMaterial()->GetPipelineLayout() : VK_NULL_HANDLE;}
            ShaderProgram *          GetMaterial         (){return mat_inst ? mat_inst->GetMaterial() : nullptr;}
            MaterialInstance *  GetMaterialInstance (){return mat_inst;}
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

            bool                HasDeferredMI       ()const{return mat_inst==nullptr&&deferred_semantic_id!=0;}
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
                    return(true);
                }

                if(mi->GetMaterial()!=mat_inst->GetMaterial())      //不能换母材质
                    return(false);

                mat_inst=mi;
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
