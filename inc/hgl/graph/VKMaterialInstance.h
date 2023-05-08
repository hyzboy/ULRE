#ifndef HGL_GRAPH_VULKAN_MATERIAL_INSTANCE_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_INSTANCE_INCLUDE

#include<hgl/graph/VKMaterial.h>

VK_NAMESPACE_BEGIN

/*
    layout(location=0) in vec4 Position;
    layout(location=1) in vec3 Normal;
 
    layout(location=?) in uint MaterialID

    #define MI_MAX_COUNT ???                //该值由引擎根据 UBORange/sizeof(MaterialInstance) 计算出来

    struct MaterialInstance                 //这部分数据，即为材质实例的具体数据，每一个材质实例类负责提供具体数据。由RenderList合并成一整个UBO
    {
        vec4 BaseColor;
        vec4 Emissive;
        vec4 ARM;
    };

    layout(set=?,binding=?) uniform Material
    {
        MaterialInstance mi[MI_MAX_COUNT]
    }mtl;

    void main()
    {
        MaterialInstance mi=mtl.mi[(MaterialID>=MI_MAX_COUNT)?:0:MaterialID];   //如果超出范围则使用0号材质实例数据

        vec4 BaseColor  =mi.BaseColor;
        vec4 Emissive   =mi.Emissive;

        float AO        =mi.ARM.x;
        float Roughness =mi.ARM.y;
        float Metallic  =mi.ARM.z;

*/

/**
* 材质实例类
*/
class MaterialInstance
{
    Material *material;

    VIL *vil;

private:

    friend class GPUDevice;

    MaterialInstance(Material *,VIL *);

public:

    virtual ~MaterialInstance()=default;

    Material *GetMaterial(){return material;}

    const VIL *GetVIL()const{return vil;}
    MaterialParameters *GetMP(const DescriptorSetType &type){return material->GetMP(type);}
    
    bool BindUBO(const DescriptorSetType &type,const AnsiString &name,DeviceBuffer *ubo,bool dynamic=false);
    bool BindSSBO(const DescriptorSetType &type,const AnsiString &name,DeviceBuffer *ubo,bool dynamic=false);
    bool BindImageSampler(const DescriptorSetType &type,const AnsiString &name,Texture *tex,Sampler *sampler);
};//class MaterialInstance
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_INSTANCE_INCLUDE
