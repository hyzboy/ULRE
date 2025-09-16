#pragma once

#include<hgl/graph/VKPrimitive.h>
#include<hgl/graph/pipeline/VKPipeline.h>
#include<hgl/graph/VKDescriptorSet.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VertexAttrib.h>
#include<hgl/graph/MeshDataBuffer.h>
#include<hgl/graph/MeshRenderData.h>

VK_NAMESPACE_BEGIN
/**
* 网格体(网格渲染中的最小渲染单位，只能是一个材质实例)
* 多材质的网格体使用StaticMesh，它内含多个Mesh
*/
class Mesh
{
    Pipeline *          pipeline;
    MaterialInstance *  mat_inst;
    Primitive *         primitive;

    MeshDataBuffer *    data_buffer;
    MeshRenderData      render_data;

private:

    friend Mesh *DirectCreateMesh(Primitive *,MaterialInstance *,Pipeline *);

    Mesh(Primitive *,MaterialInstance *,Pipeline *,MeshDataBuffer *);

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
            Primitive *         GetPrimitive        (){return primitive;}
    const   AABB &              GetBoundingBox      ()const{return primitive->GetBoundingBox();}

    const   MeshDataBuffer *    GetDataBuffer       ()const{return data_buffer;}
    const   MeshRenderData *    GetRenderData       ()const{return &render_data;}

            VAB *               GetVAB              (const int index)const{return primitive->GetVAB(index);}
            VAB *               GetVAB              (const AnsiString &name)const{return primitive->GetVAB(name);}
            VABMap *            GetVABMap           (const int vab_index){return primitive->GetVABMap(vab_index);}
            VABMap *            GetVABMap           (const AnsiString &name) { return primitive->GetVABMap(name); }
            IndexBuffer *       GetIBO              (){return primitive->GetIBO();}
            IBMap *             GetIBMap            (){return primitive->GetIBMap();}

    virtual bool                UpdatePrimitive     ();     ///<更新Primitive,一般用于primitive改变数据后需要通知Mesh的情况

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

Mesh *DirectCreateMesh(Primitive *,MaterialInstance *,Pipeline *);
VK_NAMESPACE_END
