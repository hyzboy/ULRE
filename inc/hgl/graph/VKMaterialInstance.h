#ifndef HGL_GRAPH_VULKAN_MATERIAL_INSTANCE_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_INSTANCE_INCLUDE

#include<hgl/graph/VKMaterial.h>

VK_NAMESPACE_BEGIN
class MaterialInstance
{
    Material *material;

    VIL *vil;

    MaterialParameters *mp_per_mi;       ///<材质实例独有参数，对应PerMaterial合集

private:

    friend class GPUDevice;

    MaterialInstance(Material *,VIL *);

public:

    virtual ~MaterialInstance()=default;

    Material *GetMaterial(){return material;}

    const VIL *GetVIL()const{return vil;}
    MaterialParameters *GetMP(){return mp_per_mi;}
    MaterialParameters *GetMP(const DescriptorSetType &type)
    {
        if(type==DescriptorSetType::PerMaterial)
            return mp_per_mi;
        else
            return material->GetMP(type);
    }
    
    bool BindUBO(const DescriptorSetType &type,const AnsiString &name,DeviceBuffer *ubo,bool dynamic=false);
    bool BindSSBO(const DescriptorSetType &type,const AnsiString &name,DeviceBuffer *ubo,bool dynamic=false);
    bool BindImageSampler(const DescriptorSetType &type,const AnsiString &name,Texture *tex,Sampler *sampler);
};//class MaterialInstance
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_INSTANCE_INCLUDE
