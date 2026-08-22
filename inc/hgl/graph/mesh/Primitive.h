#pragma once

#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/vk/pipeline/VKPipeline.h>
#include<hgl/vk/VKDescriptorSet.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VertexAttrib.h>
#include<hgl/graph/mesh/GeometryDataBuffer.h>
#include<hgl/graph/mesh/GeometryDrawRange.h>

namespace hgl::graph{
class PrimitiveAsset;
/**
* 图元 — 运行时绘制单元（geometry + material_program + pipeline）
* 不持有 DescriptorBindingSet；所有资源绑定由 ECS recipe runtime 负责。
*/
class Primitive
{
    Pipeline *          pipeline;
    ShaderProgram *   material_program;
    Geometry *          geometry;

    GeometryDataBuffer *data_buffer;
    GeometryDrawRange   draw_range;

private:

    friend Primitive *DirectCreatePrimitive(Geometry *,ShaderProgram *,Pipeline *);

    Primitive(Geometry *,ShaderProgram *,Pipeline *,GeometryDataBuffer *);

public:

    virtual ~Primitive()
    {
        SAFE_CLEAR(data_buffer);
    }

            Pipeline *          GetPipeline         (){return pipeline;}
            VkPipelineLayout    GetPipelineLayout   (){return material_program ? material_program->GetPipelineLayout() : VK_NULL_HANDLE;}
            ShaderProgram *   GetShaderProgram  (){return material_program;}
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

Primitive *DirectCreatePrimitive(Geometry *,ShaderProgram *,Pipeline * = nullptr);
Primitive *CreatePrimitiveRuntime(Geometry *,ShaderProgram *,Pipeline * = nullptr);
}//namespace hgl::graph
