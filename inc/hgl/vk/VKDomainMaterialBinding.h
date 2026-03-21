#pragma once

#include<hgl/vk/VKMaterial.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKResourceDomain.h>
#include<hgl/common/DescriptorSetTypeDef.h>

namespace hgl::graph
{

class IGPUBuffer;
class Texture;
class Sampler;

/**
 * 域-材质绑定视图 (DomainMaterialBinding)
 *
 * 将一个 ResourceDomain 与一个 Material (Shader/Pipeline 模板) 绑定。
 * 持有该 pair 专属的 VkDescriptorSet 集合（各 DescriptorSetType 独立分配），
 * 使同一套 Shader 可以在不同资源域（不同纹理组）下独立渲染。
 *
 * Phase 2: 支持完整的 Texture/Sampler/UBO/SSBO 描述符绑定。
 *
 * 典型用法:
 *   - 同一 billboard shader 绑定两个不同 Texture2DArray (UI 图标 vs 玩家头像)
 *   - 同一资源域绑定 Opaque + Masked 两个 Material (Phase 3)
 */
class DomainMaterialBinding
{
    ResourceDomain    *domain   = nullptr;
    Material          *material = nullptr;

    MaterialParameters *mp_array[DESCRIPTOR_SET_TYPE_COUNT] = {};

private:

    friend class MaterialManager;

    /// MaterialManager 独占构造。mp[] 由 MaterialManager 分配后传入。
    DomainMaterialBinding(ResourceDomain *d, Material *m,
                          MaterialParameters *mp[DESCRIPTOR_SET_TYPE_COUNT]);

public:

    virtual ~DomainMaterialBinding();

    // ----------------------------------------------------------------
    // 基础属性
    // ----------------------------------------------------------------

    ResourceDomain    *GetDomain   () const { return domain; }
    Material          *GetMaterial () const { return material; }

    const VkPipelineLayout GetPipelineLayout() const { return material->GetPipelineLayout(); }

    MaterialParameters *GetMP(const DescriptorSetType &type)
    {
        RANGE_CHECK_RETURN_NULLPTR(type)
        return mp_array[size_t(type)];
    }

    // ----------------------------------------------------------------
    // 描述符绑定接口
    // ----------------------------------------------------------------

    bool BindTexture(const mtl::SamplerName::SamplerSlot slot, Texture *tex)
    {
        return BindTexture(SET_TYPE_TEXTURE, slot, tex);
    }

    bool BindTextureSampler(const mtl::SamplerName::SamplerSlot slot, Texture *tex, Sampler *sampler)
    {
        return BindTextureSampler(SET_TYPE_TEXTURE, slot, tex, sampler);
    }

    bool BindUBO           (const DescriptorSetType &type, const AnsiString &name, const IGPUBuffer *gpu, bool dynamic = false);
    bool BindSSBO          (const DescriptorSetType &type, const AnsiString &name, const IGPUBuffer *gpu, bool dynamic = false);

    /// 将所有已绑定描述符写入 Vulkan 驱动（Update 各 MaterialParameters）
    void Update();

protected:

    bool BindTexture       (const DescriptorSetType &type, mtl::SamplerName::SamplerSlot slot, Texture *tex);
    bool BindTextureSampler(const DescriptorSetType &type, mtl::SamplerName::SamplerSlot slot, Texture *tex, Sampler *sampler);

}; // class DomainMaterialBinding

} // namespace hgl::graph

