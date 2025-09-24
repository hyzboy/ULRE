#pragma once

#include<hgl/graph/VKGeometry.h>
#include<hgl/graph/pipeline/VKPipeline.h>
#include<hgl/graph/VKDescriptorSet.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VertexAttrib.h>
#include<hgl/graph/mesh/GeometryDataBuffer.h>
#include<hgl/graph/mesh/GeometryDrawRange.h>

VK_NAMESPACE_BEGIN
/**
* 网格体(网格渲染中的最小渲染单位，只能是一个材质实例)
*/
class Mesh
{
    Pipeline *          pipeline;
    MaterialInstance *  mat_inst;
    Geometry *          geometry;

    GeometryDataBuffer *    data_buffer;
    GeometryDrawRange      render_data;

private:

    friend Mesh *DirectCreateMesh(Geometry *,MaterialInstance *,Pipeline *);

    Mesh(Geometry *,MaterialInstance *,Pipeline *,GeometryDataBuffer *);

public:

    virtual ~Mesh()
    {
        //需要在这里添加删除pipeline/desc_sets/primitive引用计数的代码

        SAFE_CLEAR(data_buffer);
    }

            void                UpdatePipeline      (Pipeline *p){pipeline=p;}

            Pipeline *          GetPipeline         (){return pipeline;}
            VkPipelineLayout    GetPipelineLayout   (){return mat_inst->GetMaterial()->GetPipelineLayout();}
            Material *          GetMaterial         (){return mat_inst->GetMaterial();}
            MaterialInstance *  GetMaterialInstance (){return mat_inst;}
            Geometry *          GetGeometry         (){return geometry;}
    const   AABB &              GetBoundingBox      ()const{return geometry->GetBoundingBox();}

    const   GeometryDataBuffer *    GetDataBuffer       ()const{return data_buffer;}
    const   GeometryDrawRange *    GetRenderData       ()const{return &render_data;}

            VAB *               GetVAB              (const int index)const{return geometry->GetVAB(index);}
            VAB *               GetVAB              (const AnsiString &name)const{return geometry->GetVAB(name);}
            VABMap *            GetVABMap           (const int vab_index){return geometry->GetVABMap(vab_index);}
            VABMap *            GetVABMap           (const AnsiString &name) { return geometry->GetVABMap(name); }
            IndexBuffer *       GetIBO              (){return geometry->GetIBO();}
            IBMap *             GetIBMap            (){return geometry->GetIBMap();}

    virtual bool                UpdateGeometry      ();     ///<更新Geometry,一般用于Geometry改变数据后需要通知Mesh的情况

public:

            bool                ChangeMaterialInstance(MaterialInstance *mi)
            {
                if(!mi)
                    return(false);

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
            uint32_t            GetDataVertexCount()const{return render_data.data_vertex_count;}
            uint32_t            GetDataIndexCount()const{return render_data.data_index_count;}
};//class Mesh

Mesh *DirectCreateMesh(Geometry *,MaterialInstance *,Pipeline *);
VK_NAMESPACE_END
