#pragma once

#include<hgl/vk/VKMaterialTemplate.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKInstanceDataDomain.h>
#include<hgl/common/DescriptorSetTypeDef.h>

namespace hgl::graph
{

class IGPUBuffer;
class Texture;
class Sampler;

/**
 * 域-材质绑定视图 (DomainMaterialBinding)
 *
 * 将一个 InstanceDataDomain 与一个 MaterialTemplate (Shader/GraphicsPipeline 模板) 绑定。
 * 仅持有 PerMaterial 描述符集——这是唯一域私有的集合；
 * Static/PerFrame/PerObject 集由 MaterialTemplate 统一持有、整体绑定一次。
 *
 * 典型用法:
 *   - 同一 billboard shader 绑定两个不同 Texture2DArray (UI 图标 vs 玩家头像)
 *   - 同一资源域绑定 Opaque + Masked 两个 MaterialTemplate (Phase 3)
 */
class DomainMaterialBinding
{
    InstanceDataDomain     *domain          = nullptr;
    MaterialTemplate           *material        = nullptr;
    MaterialParameters *mp_per_material = nullptr;

private:

    friend class MaterialManager;
    friend class MRDManager;

    /// MaterialManager 独占构造。mp_per_material 由 MaterialManager 分配后传入。
    DomainMaterialBinding(InstanceDataDomain *d, MaterialTemplate *m, MaterialParameters *mp);

public:

    virtual ~DomainMaterialBinding();

    // ----------------------------------------------------------------
    // 基础属性
    // ----------------------------------------------------------------

    InstanceDataDomain     *GetDomain        () const { return domain; }
    MaterialTemplate           *GetMaterial      () const { return material; }
    MaterialParameters *GetPerMaterialMP () const { return mp_per_material; }

    const VkPipelineLayout GetPipelineLayout() const { return material->GetPipelineLayout(); }

    // ----------------------------------------------------------------
    // 描述符绑定接口 (PerMaterial 集)
    // ----------------------------------------------------------------

    bool BindSSBO          (const mtl::SSBODescriptorSemantic semantic, const IGPUBuffer *gpu, bool dynamic = false);
    bool BindTexture       (const mtl::SamplerSlot slot, Texture *tex);
    bool BindTextureSampler(const mtl::SamplerSlot slot, Texture *tex, Sampler *sampler);

    /// 将已绑定描述符写入 Vulkan 驱动
    void Update();

}; // class DomainMaterialBinding

} // namespace hgl::graph

