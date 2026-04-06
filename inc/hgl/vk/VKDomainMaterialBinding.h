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
 * 将一个 ResourceDomain 与一个 Material (Shader/GraphicsPipeline 模板) 绑定。
 * 仅持有 PerMaterial 描述符集——这是唯一域私有的集合；
 * Static/PerFrame/PerObject 集由 Material 统一持有、整体绑定一次。
 *
 * 典型用法:
 *   - 同一 billboard shader 绑定两个不同 Texture2DArray (UI 图标 vs 玩家头像)
 *   - 同一资源域绑定 Opaque + Masked 两个 Material (Phase 3)
 */
class DomainMaterialBinding
{
    ResourceDomain     *domain          = nullptr;
    Material           *material        = nullptr;
    MaterialParameters *mp_per_material = nullptr;

private:

    friend class MaterialManager;

    /// MaterialManager 独占构造。mp_per_material 由 MaterialManager 分配后传入。
    DomainMaterialBinding(ResourceDomain *d, Material *m, MaterialParameters *mp);

public:

    virtual ~DomainMaterialBinding();

    // ----------------------------------------------------------------
    // 基础属性
    // ----------------------------------------------------------------

    ResourceDomain     *GetDomain        () const { return domain; }
    Material           *GetMaterial      () const { return material; }
    MaterialParameters *GetPerMaterialMP () const { return mp_per_material; }

    const VkPipelineLayout GetPipelineLayout() const { return material->GetPipelineLayout(); }

    // ----------------------------------------------------------------
    // 描述符绑定接口 (PerMaterial 集)
    // ----------------------------------------------------------------

    bool BindTexture       (const mtl::SamplerSlot slot, Texture *tex);
    bool BindTextureSampler(const mtl::SamplerSlot slot, Texture *tex, Sampler *sampler);

    /// 将已绑定描述符写入 Vulkan 驱动
    void Update();

}; // class DomainMaterialBinding

} // namespace hgl::graph

