#pragma once

#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/vk/pipeline/VKGraphicsPipeline.h>
#include<hgl/vk/VKDescriptorSet.h>
#include<hgl/vk/VKShaderMaterialProgram.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKMaterialBindingInstance.h>
#include<hgl/vk/VertexAttrib.h>
#include<hgl/vk/VKVertexInputLayout.h>
#include<hgl/graph/mesh/GeometryDataBuffer.h>
#include<hgl/graph/mesh/GeometryDrawRange.h>

namespace hgl::graph{
/**
* 图元(渲染中的最小渲染单位，一个几何体配一个材质)
*/
class Primitive
{
    MaterialBindingInstance *  mat_inst;
    Geometry *          geometry;

    GeometryDataBuffer *data_buffer;
    GeometryDrawRange   draw_range;

    const VIL *         vil         = nullptr;    ///< VIL captured at construction; geometry sharing key

private:

    friend Primitive *DirectCreatePrimitive(Geometry *,MaterialBindingInstance *,GraphicsPipelinePreRaster *,const VIL *);

    Primitive(Geometry *,MaterialBindingInstance *,GraphicsPipelinePreRaster *,GeometryDataBuffer *,const VIL *);

public:

    virtual ~Primitive();

            MaterialBindingInstance *  GetResolvedBindingInstance(){return mat_inst;}
    const   VIL *               GetVIL              ()const{return vil;}
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

            bool                ChangeMaterialInstance(MaterialBindingInstance *mi)
            {
                if(!mi)
                    return(false);

                if(mat_inst&&mi->GetShaderMaterialProgram()!=mat_inst->GetShaderMaterialProgram())  //不能换母材质
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

Primitive *DirectCreatePrimitive(Geometry *,MaterialBindingInstance *,GraphicsPipelinePreRaster * = nullptr,const VIL * = nullptr);
}//namespace hgl::graph
