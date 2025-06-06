#ifndef HGL_GRAPH_VULKAN_MATERIAL_INSTANCE_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_INSTANCE_INCLUDE

#include<hgl/graph/VKMaterial.h>

VK_NAMESPACE_BEGIN

/**
* <summary>
* 
*    layout(location=?) in uint MaterialInstanceID
*
*    #define MI_MAX_COUNT ???                //该值由引擎根据 UBORange/sizeof(MaterialInstance) 计算出来
*
*    struct MaterialInstance                 //这部分数据，即为材质实例的具体数据，每一个材质实例类负责提供具体数据。由RenderList合并成一整个UBO
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
*        MaterialInstance mi=mtl.mi[(MaterialInstanceID>=MI_MAX_COUNT)?:0:MaterialInstanceID];   //如果超出范围则使用0号材质实例数据
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
* 材质实例类本质只是提供一个数据区，供RenderList合并成一个大UBO。
*/
class MaterialInstance
{
protected:

    Material *material;

    VIL *vil;

    int mi_id;

public:

            Material *  GetMaterial ()      {return material;}

    const   VIL *       GetVIL      ()const {return vil;}

private:

    friend class Material;

    MaterialInstance(Material *,VIL *,const int);

public:

    virtual ~MaterialInstance()
    {
        material->ReleaseMI(mi_id);
    }

    const   int     GetMIID     ()const{return mi_id;}                          ///<取得材质实例ID
            void *  GetMIData   (){return material->GetMIData(mi_id);}          ///<取得材质实例数据
            void    WriteMIData (const void *data,const uint32 size);           ///<写入材质实例数据

        template<typename T>
            void    WriteMIData (const T &data){WriteMIData(&data,sizeof(T));}  ///<写入材质实例数据
};//class MaterialInstance
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_INSTANCE_INCLUDE
