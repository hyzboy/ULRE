#pragma once

#include<hgl/vk/VKMaterial.h>

namespace hgl::graph{

/**
* <summary>
*
*    layout(location=?) in uint DataIndexID
*
*    #define MI_MAX_COUNT ???                //该值由引擎根据 UBORange/sizeof(MaterialInstance) 计算出来
*
*    struct MaterialInstance                 //这部分数据，即为材质实例的具体数据，每一个材质实例类负责提供具体数据。由RenderCollector合并成一整个UBO
*    {                                       //该类数据，由DescriptorSetType为PerMaterial的参数构成
*        vec4 BaseColor;
*        vec4 Emissive;
*        vec4 ARM;
*    };
*
*    layout(set=?,binding=?) uniform Material
*    {
*        MaterialInstance mi[MI_MAX_COUNT]
*    }mtl;
*
*    void main()
*    {
*        MaterialInstance mi=mtl.mi[(DataIndexID>=MI_MAX_COUNT)?:0:DataIndexID];   //如果超出范围则使用0号材质实例数据
*
*        vec4 BaseColor  =mi.BaseColor;
*        vec4 Emissive   =mi.Emissive;
*
*        float AO        =mi.ARM.x;
*        float Roughness =mi.ARM.y;
*        float Metallic  =mi.ARM.z;
*
* </summary>
*/

/**
* 材质实例类<br>
* 材质实例类本质只是提供一个数据区，供RenderCollector合并成一个大UBO。
*/
class MaterialInstance
{
protected:

    Material *material;

    const VIL *vil;

    int mi_id;

public:

            Material *  GetMaterial ()      {return material;}

    const   VIL *       GetVIL      ()const {return vil;}

private:

    friend class Material;

    MaterialInstance(Material *,const VIL *,const int);

public:

    virtual ~MaterialInstance()
    {
        // mi_id is always -1 in the new path (no Material-owned data store)
    }

    const   int     GetMIID     ()const{return mi_id;}              ///<材质实例槽号（新路径恒为-1）
            void *  GetMIData   (){ return nullptr; }               ///<已弃用：数据区已迁移到外部SSBO
            void    WriteMIData (const void *, const uint32){}      ///<已弃用：数据直接写入外部SSBO

        template<typename T>
            void    WriteMIData (const T &){}                       ///<已弃用
};//class MaterialInstance
}//namespace hgl::graph
