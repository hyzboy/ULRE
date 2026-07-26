#pragma once

#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/vk/pipeline/VKPipeline.h>
#include<hgl/vk/VKDescriptorSet.h>
#include<hgl/vk/VKMaterialProgram.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/graph/DescriptorBindingSet.h>
#include<hgl/vk/VertexAttrib.h>
#include<hgl/graph/mesh/GeometryDataBuffer.h>
#include<hgl/graph/mesh/GeometryDrawRange.h>

namespace hgl::graph{
/**
* 图元(渲染中的最小渲染单位，一个几何体配一个材质)
*/
class Primitive
{
    Pipeline *          pipeline;
    MaterialInstance *  mat_inst;
    MaterialProgram *   material_program;
    DescriptorBindingSet *binding_set;
    const VIL *         binding_vil;
    bool                owns_binding_vil = false;
    Geometry *          geometry;

    GeometryDataBuffer *data_buffer;
    GeometryDrawRange   draw_range;

private:

    friend Primitive *DirectCreatePrimitive(Geometry *,MaterialInstance *,Pipeline *);
    friend Primitive *DirectCreatePrimitive(Geometry *,MaterialProgram *,DescriptorBindingSet *,Pipeline *);

    Primitive(Geometry *,MaterialInstance *,Pipeline *,GeometryDataBuffer *);
    Primitive(Geometry *,MaterialProgram *,DescriptorBindingSet *,const VIL *,bool,Pipeline *,GeometryDataBuffer *);

public:

    virtual ~Primitive()
    {
        //需要在这里添加删除pipeline/desc_sets/primitive引用计数的代码

        SAFE_CLEAR(data_buffer);
    }

            Pipeline *          GetPipeline         (){return pipeline;}
            VkPipelineLayout    GetPipelineLayout   (){return GetMaterialProgram()?GetMaterialProgram()->GetPipelineLayout():VK_NULL_HANDLE;}
            MaterialProgram *   GetMaterialProgram  (){return mat_inst?mat_inst->GetMaterialProgram():(binding_set?binding_set->GetMaterialProgram():material_program);}
            MaterialInstance *  GetMaterialInstance (){return mat_inst;}
            DescriptorBindingSet *GetDescriptorBindingSet(){return binding_set;}
    const   VIL *               GetVIL              ()const{return binding_vil ? binding_vil : (mat_inst ? mat_inst->GetVIL() : (binding_set ? binding_set->GetVIL() : (material_program ? material_program->GetDefaultVIL() : nullptr)));}
            Geometry *          GetGeometry         (){return geometry;}
            AnsiString          GetGeometryName     (){return geometry->GetName();}
    const   BoundingVolumes &   GetBoundingVolumes  ()const{return geometry->GetBoundingVolumes();}

    const   GeometryDataBuffer *GetDataBuffer       ()const{return data_buffer;}
    const   GeometryDrawRange * GetRenderData       ()const{return &draw_range;}

            VAB *               GetVAB              (const int index)const{return geometry->GetVAB(index);}
            VAB *               GetVAB              (const VertexSemantic semantic)const{return geometry->GetVAB(semantic);}
            IndexBuffer *       GetIBO              (){return geometry->GetIBO();}

    virtual bool                UpdateGeometry      ();     ///<更新Geometry,一般用于Geometry改变数据后需要通知Mesh的情况

public:

            // 设置绘制数量（vertex/index），若大于数据量会被裁剪至数据量
            bool                SetDrawCounts(uint32_t draw_vertex_count,uint32_t draw_index_count=0);

            // 设置绘制范围和数量
            bool                SetDrawRange(int32_t vertex_offset,uint32_t first_index,uint32_t draw_vertex_count,uint32_t draw_index_count=0);

            // 取得缓冲区实际数据数量
            uint32_t            GetDataVertexCount()const{return draw_range.data_vertex_count;}
            uint32_t            GetDataIndexCount()const{return draw_range.data_index_count;}
};//class Primitive

Primitive *DirectCreatePrimitive(Geometry *,MaterialInstance *,Pipeline * = nullptr);
Primitive *DirectCreatePrimitive(Geometry *,MaterialProgram *,DescriptorBindingSet * = nullptr,Pipeline * = nullptr);

// Pre-flight compatibility check: matches geometry vertex format against the
// material's VIL and returns the full failure summary without creating a primitive.
// Useful for callsites that want to emit context-rich diagnostics before or instead
// of calling DirectCreatePrimitive.
GeometryVertexFormatMatch QueryGeometryVertexCompatibility(const Geometry *geom, const MaterialInstance *mi);
}//namespace hgl::graph
