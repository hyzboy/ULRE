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
 *
 * 注意: PerObject 集通常由 ShaderMaterialProgram 统一持有，但当多个域共享同一
 *       ShaderMaterialProgram 时（例如共享 billboard shader），需要各域持有独立的
 *       PerObject MP，以隔离各域的 TransformData/TransformID 绑定。
 */
class DomainResourceBinding
{
    ResourceDomain     *domain          = nullptr;
    ShaderMaterialProgram           *material        = nullptr;
    MaterialParameters *mp_per_material = nullptr;
    MaterialParameters *mp_per_object   = nullptr;  // 域私有 PerObject MP，用于隔离 TransformData/TransformID

private:

    friend class ShaderMaterialProgramManager;

    /// ShaderMaterialProgramManager 独占构造。两个 MP 均由 ShaderMaterialProgramManager 分配后传入。
    DomainResourceBinding(ResourceDomain *d, ShaderMaterialProgram *m,
                          MaterialParameters *mp_material, MaterialParameters *mp_object = nullptr);

public:

    virtual ~DomainResourceBinding();

    // ----------------------------------------------------------------
    // 基础属性
    // ----------------------------------------------------------------

    ResourceDomain     *GetDomain        () const { return domain; }
    ShaderMaterialProgram           *GetShaderMaterialProgram      () const { return material; }
    MaterialParameters *GetPerMaterialMP () const { return mp_per_material; }
    MaterialParameters *GetPerObjectMP   () const { return mp_per_object; }

    const VkPipelineLayout GetPipelineLayout() const { return material->GetPipelineLayout(); }

    // ----------------------------------------------------------------
    // 描述符绑定接口 (PerMaterial 集)
    // ----------------------------------------------------------------

    bool BindTexture       (const mtl::SamplerSlot slot, Texture *tex);
    bool BindResourceSampler(const mtl::SamplerSlot slot, Texture *tex, Sampler *sampler);

    /// 将已绑定描述符写入 Vulkan 驱动
    void Update();

}; // class DomainResourceBinding

} // namespace hgl::graph

