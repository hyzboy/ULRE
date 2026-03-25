#pragma once

#include<hgl/vk/VK.h>
#include<hgl/type/String.h>
#include<hgl/vk/VKShaderModuleMap.h>
#include<hgl/mtl/DescriptorBindingContract.h>
#include<hgl/log/Log.h>
#include<unordered_set>


namespace hgl::graph{

class IGPUBuffer;
class ResourceDomain;   ///< Phase 5: forward decl
class UBOAccessorBase;
template<typename T,mtl::UBODescriptorSemantic Semantic> class UBOAccessor;

namespace mtl
{
    class MaterialCreateInfo;
}

class MaterialParameters;

using ShaderStageCreateInfoList=ValueArray<VkPipelineShaderStageCreateInfo>;

/**
 * 材质类<br>
 * 用于管理shader，提供DescriptorSetLayoutCreater.
 * 在材质需要用到UBO.SSBO数据情况下，Material不能被用于渲染，需要一个MaterialInstance来提供数据才能进行渲染。所以一般情况下，不使用Material进行渲染。<br>
 */
class Material
{
    OBJECT_LOGGER

    AnsiString name;

    PrimitiveType geometry;                       ///<图元类型

    VertexInput *vertex_input;

    ShaderModuleMap *shader_maps;

    MaterialDescriptorManager *desc_manager;
    mtl::BindingContract binding_contract;

    ShaderStageCreateInfoList shader_stage_list;

    PipelineLayoutData *pipeline_layout_data;

    MaterialParameters *mp_array[DESCRIPTOR_SET_TYPE_COUNT];

    uint32_t mi_data_bytes;             ///<实例数据大小
    uint32_t mi_max_count;              ///<实例一次渲染最大数量限制

    ResourceDomain *default_domain = nullptr;   ///< Phase 5: 懒初始化默认域（旧 CreateMI 路径自动创建）

    bool has_l2w_matrix;                ///<是否有LocalToWorld矩阵

    uint8_t texture_array_slot_flags = 0; ///< bit N = SamplerSlot(N) uses TextureArray mode

private:

    friend class MaterialManager;

    Material(const AnsiString &,const mtl::MaterialCreateInfo *);

public:

    virtual ~Material();

    const   AnsiString &                        GetName                 ()const{return name;}
    const   mtl::BindingContract &              GetBindingContract      ()const{return binding_contract;}

    const   PrimitiveType &                     GetPrimitiveType        ()const{return geometry;}

    const   VertexInput *                       GetVertexInput          ()const{return vertex_input;}

    const   ShaderStageCreateInfoList &         GetStageList            ()const{return shader_stage_list;}

//    const   MaterialDescriptorManager *         GetDescriptorManager    ()const{return desc_manager;}
    const   VkPipelineLayout                    GetPipelineLayout       ()const;
    const   PipelineLayoutData *                GetPipelineLayoutData   ()const{return pipeline_layout_data;}

public:

            MaterialParameters *                GetMP                   (const DescriptorSetType &type)
            {
                RANGE_CHECK_RETURN_NULLPTR(type)

                return mp_array[size_t(type)];
            }

    const   bool                                hasSet                  (const DescriptorSetType &type)const;

    const   VIL *                               GetDefaultVIL()const;
            VIL *                               CreateVIL(const VILConfig *format_map=nullptr);
            bool                                Release(VIL *);
    const   uint                                GetVILCount();

public:

    bool BindTexture(const mtl::SamplerSlot slot,Texture *tex)
    {
        return BindTexture(SET_TYPE_TEXTURE,slot,tex);
    }

    bool BindTextureSampler(const mtl::SamplerSlot slot,Texture *tex,Sampler *sampler)
    {
        return BindTextureSampler(SET_TYPE_TEXTURE,slot,tex,sampler);
    }

    bool BindUBO(const mtl::UBODescriptorSemantic semantic,const IGPUBuffer *gpu,bool dynamic=false);
    bool BindUBO(const UBOAccessorBase *ubo,bool dynamic=false);
    bool BindSSBO(const mtl::SSBODescriptorSemantic semantic,const IGPUBuffer *gpu,bool dynamic=false);

    template<typename T,mtl::UBODescriptorSemantic Semantic>
    bool BindUBO(const UBOAccessor<T,Semantic> *ubo,bool dynamic=false)
    {
        if(!ubo)
            return false;

        return BindUBO(Semantic,
                       ubo->GetGPUBuffer(),
                       dynamic);
    }

    void Update();

protected:

    bool BindTexture(const DescriptorSetType &type,mtl::SamplerSlot slot,Texture *tex);
    bool BindTextureSampler(const DescriptorSetType &type,mtl::SamplerSlot slot,Texture *tex,Sampler *sampler);

public:

    const bool      hasLocalToWorld ()const{return has_l2w_matrix; }

            void    SetTextureArraySlotFlags(uint8_t f){texture_array_slot_flags=f;}
    const uint8_t   GetTextureArraySlotFlags()const{return texture_array_slot_flags;}

    const bool      hasMI           ()const{return mi_data_bytes>0;}
    const uint32_t  GetMIDataBytes  ()const{return mi_data_bytes;}
    const uint32_t  GetMIMaxCount   ()const{return mi_max_count;}

    void ReleaseMI(int);    ///<释放材质实例
    void *GetMIData(int);   ///<取得指定ID号的材质实例数据访问指针

    MaterialInstance *CreateMI(const VIL *);
    MaterialInstance *CreateMI(const VILConfig *vil_cfg=nullptr);
};//class Material

using MaterialSet=std::unordered_set<Material *>;
}//namespace hgl::graph
