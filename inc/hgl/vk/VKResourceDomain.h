#pragma once

namespace hgl::graph
{

class Material;

/**
 * 资源域 (ResourceDomain) — Phase 0 存根
 *
 * 规划中最终将承载：
 *   - MaterialInstance 数据池（mi_data_manager）
 *   - 该域绑定的 Texture / Sampler 描述符
 *   - Array 纹理 Layer 索引 SSBO
 *
 * Phase 0 中暂无行为，仅提供类型声明与占位接口，不接入主流程。
 */
class ResourceDomain
{
    Material *source_material = nullptr;    ///< 该域基于的 Material 模板

private:

    friend class MaterialManager;

    explicit ResourceDomain(Material *mtl) : source_material(mtl) {}

public:

    virtual ~ResourceDomain() = default;

    /**
     * 返回创建本域时所用的 Material 模板（Shader/Layout 来源）。
     */
    Material *GetSourceMaterial() const { return source_material; }

}; // class ResourceDomain

} // namespace hgl::graph
