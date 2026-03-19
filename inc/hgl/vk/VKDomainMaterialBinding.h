#pragma once

namespace hgl::graph
{

class ResourceDomain;
class Material;

/**
 * 域-材质绑定视图 (DomainMaterialBinding) — Phase 0 存根
 *
 * 规划中最终将持有：
 *   - ResourceDomain* — 共享资源域（MI 数据、纹理、后续 SSBO）
 *   - Material*       — 目标渲染材质（同一域可绑多个，如 Opaque + Masked）
 *   - 本 pair 专属的资源绑定表（Texture/Sampler/SSBO per-binding）
 *
 * Phase 0 中暂无行为，仅提供类型声明与占位接口，不接入主流程。
 */
class DomainMaterialBinding
{
    ResourceDomain *domain   = nullptr;
    Material       *material = nullptr;

private:

    friend class MaterialManager;

    DomainMaterialBinding(ResourceDomain *d, Material *m) : domain(d), material(m) {}

public:

    virtual ~DomainMaterialBinding() = default;

    /**
     * 返回该绑定所属的资源域。
     */
    ResourceDomain *GetDomain()   const { return domain; }

    /**
     * 返回该绑定对应的渲染材质（Shader/Pipeline 模板）。
     */
    Material       *GetMaterial() const { return material; }

}; // class DomainMaterialBinding

} // namespace hgl::graph
