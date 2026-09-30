#pragma once

#include<hgl/ecs/components/RenderableComponent.h>
#include<hgl/ecs/support/PositionSourceSpec.h>
#include<hgl/ecs/support/TransformPolicySpec.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/ShaderProgramKey.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/render/RenderItemDescriptor.h>
#include<hgl/type/String.h>
#include<hgl/type/UnorderedMap.h>
#include<glm/glm.hpp>

// Forward declarations to avoid heavy includes
namespace hgl
{
    namespace math
    {
        class AABB;
    }

    namespace graph
    {
        class DeviceBuffer;
        struct GeometryDataBuffer;
        struct GeometryDrawRange;
        class Geometry;
        class ShaderProgram;
        class Pipeline;
        class RenderPass;
        class Sampler;
        class Texture;
    }
}

namespace hgl::ecs
{
    class RenderItemDataStorage;

    /**
     * PrimitiveComponent - Renderable component for static mesh rendering
     *
     * Manages a single PrimitiveAsset (geometry + recipe) for rendering.
     * Derived from RenderableComponent to provide rendering capabilities.
     *
     * Features:
     * - Holds reference to hgl::graph::PrimitiveAsset
     * - Provides access to ShaderProgram, Pipeline, and AABB data
     * - Compatible with RenderCollector for batched rendering
     */
    class PrimitiveComponent : public RenderableComponent
    {
    private:

        const hgl::graph::PrimitiveAsset* primitiveAsset = nullptr;  // Asset-level geometry+recipe pairing (not owned)
        uint32_t primitiveVariantIndex = 0;
        hgl::graph::PrimitiveVariantPurpose primitiveVariantPurpose =
            hgl::graph::PrimitiveVariantPurpose::Surface;
        hgl::graph::GeometryDataBuffer *runtime_data_buffer = nullptr;
        hgl::graph::GeometryDrawRange *runtime_draw_range = nullptr;
        hgl::graph::Geometry *runtime_geometry = nullptr;        hgl::graph::Pipeline* overridePipeline = nullptr;  // Optional pipeline override (not owned)
        // Late-resolve pipeline slot:
        // Populated at render-time if primitive has no pre-baked pipeline.
        // 每个 RenderPass（≈每个 RenderTarget）各自持有解析出的管线——同一世界
        // 被 RenderTo 到多个 RT（如 ShadowMap 的 depth-only 离屏 Pass）时，
        // 各 RT 使用各自格式匹配的管线，互不驱逐。Pipeline 归 RenderPass 所有。
        // 复用必须校验创建时的 program 身份——shader 源码/模板变化会生成新 program
        // （digest 不同），若仅按 RenderPass 键控会持续复用旧 program 的管线
        // （masked 阴影从未生效的根因）。
        // 身份用 ShaderProgramKey（结构化 digest）而**不是 program 指针**：program
        // 对象释放后，新对象可能被分配到同一地址，指针比较会把两个不同的 shader
        // 判成同一个 → 复用错误管线（与 pipeline 键用 VkShaderModule 句柄值同构，
        // 见 doc/backlog.md D2）。pipeline + program 身份放在同一条目里，不再维护
        // 两张手工同步的平行 map。
        struct ResolvedRuntimePipeline
        {
            hgl::graph::Pipeline *pipeline = nullptr;
            hgl::graph::mtl::ShaderProgramKey program_key;
            bool                  has_program_key = false;
        };

        hgl::UnorderedMap<hgl::graph::RenderPass *, ResolvedRuntimePipeline> resolvedRuntimePipelineMap;

        PositionSourceSpec positionSourceSpec;            // Unified position source ingress policy
        TransformPolicySpec transformPolicySpec;           // Unified transform policy ingress

    protected:
        // RenderItem 4-ID descriptor handle and storage binding
        graph::RenderItemHandle render_item_handle = graph::INVALID_RENDER_ITEM_HANDLE;
        RenderItemDataStorage *bound_render_item_storage = nullptr;
        graph::RenderItemDescriptor render_item_descriptor{};

        virtual void EnsureRenderItemStorageAllocated();

    public:

        explicit PrimitiveComponent(const std::string& name = "Primitive")
            : RenderableComponent(name)
            , overridePipeline(nullptr)
            , positionSourceSpec(PositionSourceSpec::MeshVertex)
            , transformPolicySpec{}
        {
        }

        virtual ~PrimitiveComponent() = default;

    public:

        // Primitive management
        const char* GetSystemGroupName() const override { return "Primitive"; }

        void SetPrimitiveAsset(const hgl::graph::PrimitiveAsset *asset);
        const hgl::graph::PrimitiveAsset *GetPrimitiveAsset() const { return primitiveAsset; }
        void ClearPrimitiveAsset() { SetPrimitiveAsset(nullptr); }
        void SetPrimitiveVariantIndex(const uint32_t index) { primitiveVariantIndex = index; }
        uint32_t GetPrimitiveVariantIndex() const { return primitiveVariantIndex; }
        void SetPrimitiveVariantPurpose(
            const hgl::graph::PrimitiveVariantPurpose purpose)
        {
            if (primitiveVariantPurpose == purpose)
                return;
            primitiveVariantPurpose = purpose;
            InvalidateResolvedRuntimePipeline();
        }
        hgl::graph::PrimitiveVariantPurpose GetPrimitiveVariantPurpose()
            const
        {
            return primitiveVariantPurpose;
        }
        bool EnsureRuntimeGeometryBinding(hgl::graph::ShaderProgram *material);
        void ClearRuntimeGeometryBinding();
        const hgl::graph::GeometryDataBuffer *GetRuntimeGeometryDataBuffer() const;
        const hgl::graph::GeometryDrawRange *GetRuntimeGeometryDrawRange() const;

        void SetOverridePipeline(hgl::graph::Pipeline* p) { overridePipeline = p; }
        hgl::graph::Pipeline* GetOverridePipeline() const { return overridePipeline; }
        void ClearOverridePipeline() { overridePipeline = nullptr; }

        // 写入/校验在 PrimitiveComponent.cpp（此处只有 ShaderProgram 前置声明，
        // 内联实现需要 GetProgramKey() 的完整定义）。
        void SetResolvedRuntimePipeline(hgl::graph::RenderPass *rp,
                                        hgl::graph::Pipeline *p,
                                        hgl::graph::ShaderProgram *program);

        // 复用校验：RenderPass 命中且创建时的 program 身份（结构化 digest）一致。
        // program 更换（shader 重编译/模板切换）时返回 false，让调用方重建管线。
        bool HasResolvedRuntimePipeline(hgl::graph::RenderPass *render_pass,
                                        hgl::graph::ShaderProgram *program) const;

        void SetTransformPolicySpec(const TransformPolicySpec& spec) { transformPolicySpec = spec; }
        const TransformPolicySpec& GetTransformPolicySpec() const { return transformPolicySpec; }
        void SetPositionSourceSpec(PositionSourceSpec spec) { positionSourceSpec = spec; }
        PositionSourceSpec GetPositionSourceSpec() const { return positionSourceSpec; }

        // Asset 里的默认配方——材质解析链的**基底来源**。作者把材质授权搬到
        // 数据层（MaterialData）之后，本访问器仍是 asset 归属的数据，由调用方
        // 作为 `MaterialData::BuildResolvedRecipe` 的 asset_default_recipe 传入。
        // A5 拆分 Geometry 时随 asset 一并迁移。
        const hgl::graph::mtl::MaterialRecipe *GetAssetMaterialRecipe() const;

        // 材质授权内容变化（数据层代数前进 / 配方内容变化）时调用：
        // 已解析的运行期管线不再可信（管线由 program 身份 + 规范化 recipe 共同决定）。
        void InvalidateResolvedRuntimePipeline();

        // ShaderProgram access (returns override if set, otherwise descriptor-bound material)
        hgl::graph::ShaderProgram* GetShaderProgram() const;

        // Pipeline access: override → runtime resolved (per render pass)
        hgl::graph::Pipeline* GetPipelineForRenderPass(hgl::graph::RenderPass *render_pass) const;

        // Bounding volume
        bool GetLocalAABB(hgl::math::AABB& outAABB) const;

        // Rendering capability check
        bool CanRender() const;

        // RenderItem 4-ID Descriptor & Handle
        graph::RenderItemHandle GetRenderItemHandle() const;
        const graph::RenderItemDescriptor &GetRenderItemDescriptor() const;

        void SetTransformID(uint32_t transform_id);
        void SetGeometryID(uint32_t geometry_id);
        void SetMaterialID(uint32_t material_id);
        void SetTextureID(uint32_t texture_id);
        void Set4ID(uint32_t transform_id, uint32_t geometry_id, uint32_t material_id, uint32_t texture_id);

        uint32_t GetTransformID() const { return render_item_descriptor.transform_id; }
        uint32_t GetGeometryID() const { return render_item_descriptor.geometry_id; }
        uint32_t GetMaterialID() const { return render_item_descriptor.material_id; }
        uint32_t GetTextureID() const { return render_item_descriptor.texture_id; }

    public:

        // Kept as a no-op compatibility override while some call sites/vtables still
        // expect a concrete PrimitiveComponent::Render symbol. ECS render path does
        // not use this entry for actual draw submission.

        // Component lifecycle
        void OnAttach() override;
        void OnDetach() override;
    };

    /// 槽位映射（**暂定**：`PrimitiveComponent` 承载几何来源，A5 拆分后由真正的 `Geometry` 组件接管；
    /// 材质授权数据已在 A2 迁至 `MaterialData`）
    template<> struct ComponentTypeOf<PrimitiveComponent> { static constexpr ComponentType value = ComponentType::Geometry; };
}//namespace hgl::ecs
