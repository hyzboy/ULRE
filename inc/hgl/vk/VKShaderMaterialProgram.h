#pragma once

#include<hgl/vk/VK.h>
#include<hgl/type/String.h>
#include<hgl/vk/VKShaderModuleMap.h>
#include<hgl/mtl/DescriptorSemanticRegistry.h>
#include<hgl/mtl/ShaderDataSchema.h>
#include<hgl/mtl/MaterialKey.h>
#include<hgl/log/Log.h>
#include<hgl/common/AttributeProvider.h>
#include<hgl/common/VertexAttribDef.h>
#include<unordered_set>


namespace hgl::graph{

class IGPUBuffer;
class UBOAccessorBase;
template<typename T,mtl::UBODescriptorSemantic Semantic> class UBOAccessor;

namespace mtl
{
    class MaterialCreateInfo;
}

class MaterialParameters;

using ShaderStageCreateInfoList=std::vector<VkPipelineShaderStageCreateInfo>;

/**
 * 材质类<br>
 * 用于管理shader，提供DescriptorSetLayoutCreater.
 * 在材质需要用到UBO.SSBO数据情况下，Material不能被用于渲染，需要一个MaterialInstance来提供数据才能进行渲染。所以一般情况下，不使用Material进行渲染。<br>
 */
class ShaderMaterialProgram
{
    OBJECT_LOGGER

    AnsiString name;

    PrimitiveType geometry;                       ///<图元类型

    VertexInput *vertex_input;

    ShaderModuleMap *shader_maps;

    MaterialDescriptorManager *desc_manager;
    mtl::DescriptorBindingSlots binding_contract;

    ShaderStageCreateInfoList shader_stage_list;

    GraphicsPipelineLayoutData *pipeline_layout_data;

    MaterialParameters *mp_array[DESCRIPTOR_SET_TYPE_COUNT];

    mtl::ShaderDataSchema mi_schema = mtl::ShaderDataSchema::None;

    bool has_l2w_matrix;                ///<是否有LocalToWorld矩阵

    uint8_t texture_array_slot_flags = 0; ///< bit N = SamplerSlot(N) uses TextureArray mode

    uint64_t effective_feature_mask = 0; ///< Effective feature mask from Phase 3 cache key resolution

    mtl::MaterialKey material_key_{}; ///< Step 3: MaterialKey — set after creation for fast keyed lookup

private:

    friend class ShaderMaterialProgramManager;

    ShaderMaterialProgram(const AnsiString &,const mtl::MaterialCreateInfo *);

public:

    virtual ~ShaderMaterialProgram();

    const   AnsiString &                        GetName                 ()const{return name;}
    const   mtl::DescriptorBindingSlots &       GetBindingContract      ()const{return binding_contract;}

    const   PrimitiveType &                     GetPrimitiveType        ()const{return geometry;}

    const   VertexInput *                       GetVertexInput          ()const{return vertex_input;}

            void                                SetPullingEnabled       (bool v);
            bool                                IsPullingEnabled        ()const;

    const   ShaderStageCreateInfoList &         GetStageList            ()const{return shader_stage_list;}

//    const   MaterialDescriptorManager *         GetDescriptorManager    ()const{return desc_manager;}
    const   VkPipelineLayout                    GetPipelineLayout       ()const;
    const   GraphicsPipelineLayoutData *        GetGraphicsPipelineLayoutData   ()const{return pipeline_layout_data;}

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

    bool BindResourceSampler(const mtl::SamplerSlot slot,Texture *tex,Sampler *sampler)
    {
        return BindResourceSampler(SET_TYPE_TEXTURE,slot,tex,sampler);
    }

    bool BindUBO(const mtl::UBODescriptorSemantic semantic,const IGPUBuffer *gpu,bool dynamic=false);
    bool BindUBO(const UBOAccessorBase *ubo,bool dynamic=false);
    bool BindSSBO(const mtl::SSBODescriptorSemantic semantic,const IGPUBuffer *gpu,bool dynamic=false);
    bool BindAttribStream(const AttributeSemantic semantic,const IGPUBuffer *buffer,uint32_t byte_offset=0,uint32_t stride=0);
    bool BindAttribStream(const VertexAttrib attrib,const IGPUBuffer *buffer,uint32_t byte_offset=0,uint32_t stride=0);
    bool BindVertexStreamBinding(const uint32_t binding,const IGPUBuffer *buffer,bool dynamic=false);

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
    bool BindResourceSampler(const DescriptorSetType &type,mtl::SamplerSlot slot,Texture *tex,Sampler *sampler);

public:

    const bool      hasLocalToWorld ()const{return has_l2w_matrix; }

            void    SetTextureArraySlotFlags(uint8_t f){texture_array_slot_flags=f;}
    const uint8_t   GetTextureArraySlotFlags()const{return texture_array_slot_flags;}

    const uint64_t  GetEffectiveFeatureMask()const{return effective_feature_mask;}

    const bool      hasMI           ()const{return mi_schema!=mtl::ShaderDataSchema::None;}
    const mtl::ShaderDataSchema GetShaderDataSchema() const { return mi_schema; }

public: // Step 3: MaterialKey cache support

    void SetMaterialKey(const mtl::MaterialKey &k) noexcept { material_key_ = k; }
    const mtl::MaterialKey &GetMaterialKey() const noexcept { return material_key_; }
    bool HasMaterialKey() const noexcept { return material_key_.Hash() != 0; }

};//class ShaderMaterialProgram

using MaterialSet=std::unordered_set<ShaderMaterialProgram *>;
}//namespace hgl::graph
