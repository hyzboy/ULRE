#pragma once

#include<hgl/vk/VKShaderMaterialProgram.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKResourceDomain.h>
#include<hgl/common/DescriptorSetTypeDef.h>

namespace hgl::graph
{

class IGPUBuffer;
class Texture;
class Sampler;

/**
 * 域-材质绑定视图 (DomainResourceBinding)
 *
 * 将一个 ResourceDomain 与一个 ShaderMaterialProgram (Shader/GraphicsPipeline 模板) 绑定。
 * 仅持有 PerMaterial 描述符集——这是唯一域私有的集合；
 * Static/PerFrame/PerObject 集由 ShaderMaterialProgram 统一持有、整体绑定一次。
 *
 * 典型用法:
 *   - 同一 billboard shader 绑定两个不同 Texture2DArray (UI 图标 vs 玩家头像)
 *   - 同一资源域绑定 Opaque + Masked 两个 ShaderMaterialProgram (Phase 3)
 */
class DomainResourceBinding
{
    ResourceDomain     *domain          = nullptr;
    ShaderMaterialProgram           *material        = nullptr;
    MaterialParameters *mp_per_material = nullptr;

private:

    friend class ShaderMaterialProgramManager;

    /// ShaderMaterialProgramManager 独占构造。mp_per_material 由 ShaderMaterialProgramManager 分配后传入。
    DomainResourceBinding(ResourceDomain *d, ShaderMaterialProgram *m, MaterialParameters *mp);

public:

    virtual ~DomainResourceBinding();

    // ----------------------------------------------------------------
    // 基础属性
    // ----------------------------------------------------------------

    ResourceDomain     *GetDomain        () const { return domain; }
    ShaderMaterialProgram           *GetShaderMaterialProgram      () const { return material; }
    MaterialParameters *GetPerMaterialMP () const { return mp_per_material; }

    const VkPipelineLayout GetPipelineLayout() const { return material->GetPipelineLayout(); }

    // ----------------------------------------------------------------
    // 描述符绑定接口 (PerMaterial 集)
    // ----------------------------------------------------------------

    bool BindTexture       (const mtl::SamplerSlot slot, Texture *tex);
    bool BindTextureSampler(const mtl::SamplerSlot slot, Texture *tex, Sampler *sampler);

    /// 将已绑定描述符写入 Vulkan 驱动
    void Update();

}; // class DomainResourceBinding

} // namespace hgl::graph

