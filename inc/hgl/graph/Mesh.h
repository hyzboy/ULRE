#pragma once

#include<hgl/graph/VKPrimitive.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKDescriptorSet.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VertexAttrib.h>
VK_NAMESPACE_BEGIN
/**
* 原始图元数据缓冲区<Br>
* 提供在渲染之前的数据绑定信息
*/
struct MeshDataBuffer:public Comparator<MeshDataBuffer>
{
    uint32_t        vab_count;
    VkBuffer *      vab_list;

    // 理论上讲，每个VAB绑定时都是可以指定byte offsets的。但是随后Draw时，又可以指定vertexOffset。
    // 在我们支持的两种draw模式中，一种是每个模型一批VAB，所有VAB的offset都是0。
    // 另一种是使用VDM，为了批量渲染，所有的VAB又必须对齐，所以每个VAB单独指定offset也不可行。

    VkDeviceSize *  vab_offset;         //注意：这里的offset是字节偏移，不是顶点偏移

    IndexBuffer *   ibo;

    VertexDataManager *vdm;             //只是用来区分和比较的，不实际使用

public:

    MeshDataBuffer(const uint32_t,IndexBuffer *,VertexDataManager *_v=nullptr);
    ~MeshDataBuffer();

    const int compare(const MeshDataBuffer &pdb)const override;

    bool Update(const Primitive *,const VIL *);
};//struct MeshDataBuffer

/**
* 原始图元渲染数据<Br>
* 提供在渲染时的数据
*/
struct MeshRenderData:public ComparatorData<MeshRenderData>
{
    //因为要VAB是流式访问，所以我们这个结构会被用做排序依据
    //也因此，把vertex_offset放在最前面

    int32_t         vertex_offset;  //注意：这里的offset是相对于vertex的，代表第几个顶点，不是字节偏移
    uint32_t        first_index;

    uint32_t        vertex_count;
    uint32_t        index_count;

public:

    void Set(const Primitive *);
};

/**
* 网格体(网格渲染中的最小渲染单位，只能是一个材质实例)
* 多材质的网格体使用StaticMesh，它内含多个Mesh
*/
class Mesh
{
    Pipeline *          pipeline;
    MaterialInstance *  mat_inst;
    Primitive *         primitive;

    MeshDataBuffer * data_buffer;
    MeshRenderData render_data;

private:

    friend Mesh *CreateMesh(Primitive *,MaterialInstance *,Pipeline *);

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
};//class Mesh

Mesh *CreateMesh(Primitive *,MaterialInstance *,Pipeline *);
VK_NAMESPACE_END
