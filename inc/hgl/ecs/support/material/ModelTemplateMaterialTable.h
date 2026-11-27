#pragma once

#include <hgl/type/ValueArray.h>
#include <hgl/type/PtrArray.h>
#include <hgl/type/ManagedArray.h>
#include <hgl/type/UnorderedMap.h>
#include <hgl/ecs/support/material/MaterialRuntimeTable.h>
#include <hgl/common/PrimitiveType.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstdint>

namespace hgl::graph { class GeometryVertexFormat; }

namespace hgl::ecs
{
    class ECSContext;

    /**
     * 单图元模型模板缓存键
     */
    struct SinglePrimitiveTemplateKey
    {
        uint32_t geometry_id = 0;
        bool can_cast_shadow = true;
        bool is_opaque = true;
        graph::PrimitiveType primitive_type = graph::PrimitiveType::Triangles;

        bool operator==(const SinglePrimitiveTemplateKey &rhs) const
        {
            return geometry_id == rhs.geometry_id
                && can_cast_shadow == rhs.can_cast_shadow
                && is_opaque == rhs.is_opaque
                && primitive_type == rhs.primitive_type;
        }
    };

    struct SinglePrimitiveTemplateKeyHash
    {
        std::size_t operator()(const SinglePrimitiveTemplateKey &k) const
        {
            hgl::hash::FNV1aHasher64 h;
            h << k.geometry_id << (k.can_cast_shadow ? 1u : 0u) << (k.is_opaque ? 1u : 0u);
            h << static_cast<uint32_t>(k.primitive_type);
            return static_cast<std::size_t>(h.Result());
        }
    };

    /**
     * 模板内一个 Shading Group 的渲染元数据描述
     */
    struct TemplateShadingGroupDesc
    {
        MaterialRuntimeRowID runtime_row = INVALID_MATERIAL_RUNTIME_ROW_ID; ///< 该组代表运行期材质行
        ValueArray<uint32_t> slot_geometry; ///< 组内各 bucket_slot 对应的 geometry_id
        ValueArray<uint32_t> slot_count;    ///< 组内各 bucket_slot 在单实例中出现的部件次数
        bool casts_shadow = true;           ///< 组内是否存在 CanCastShadow 部件
        const graph::GeometryVertexFormat *vertex_format = nullptr; ///< Borrowed from the retained geometry asset.
        graph::PrimitiveType primitive_type = graph::PrimitiveType::Triangles;
    };

    /**
     * 模板级渲染绑定：持有本模板全部部件的材质行引用与 Shading Group 划分
     */
    struct TemplateRenderBinding
    {
        uint32_t template_id = 0;
        bool owns_gpu_rows = false;
        ValueArray<MaterialRuntimeRowID> owned_part_rows; ///< 本模板持有的全部部件 MaterialRuntimeRowID（管理 AddRef/Release）
        PtrArray<TemplateShadingGroupDesc> groups;        ///< 长度 == ModelTemplateRow.shading_group_count
    };

    /**
     * ModelTemplateMaterialTable
     *
     * 管理 template_id -> TemplateRenderBinding 的生命周期映射。
     * 由 ECSContext 持有。注册绑定时对所引用的 MaterialRuntimeRow 加引用，
     * 注销或世界销毁时统一释放引用，使模板材质常驻于 GPU 材质池中。
     */
    class ModelTemplateMaterialTable
    {
    private:
        ECSContext *context = nullptr;
        ManagedArray<TemplateRenderBinding> binding_storage;
        ValueArray<TemplateRenderBinding *> retired_bindings;
        UnorderedMap<uint32_t, TemplateRenderBinding *> bindings;
        UnorderedMap<SinglePrimitiveTemplateKey, uint32_t, SinglePrimitiveTemplateKeyHash> single_primitive_cache;
        UnorderedMap<uint32_t, SinglePrimitiveTemplateKey> template_to_single_key;

        void ReleaseBindingRows(const TemplateRenderBinding &binding);
        bool IsGPUQuiescent() const;

    public:
        explicit ModelTemplateMaterialTable(ECSContext *ctx = nullptr) : context(ctx) {}
        ~ModelTemplateMaterialTable();

        ModelTemplateMaterialTable(const ModelTemplateMaterialTable &) = delete;
        ModelTemplateMaterialTable &operator=(const ModelTemplateMaterialTable &) = delete;

        void SetContext(ECSContext *ctx) { context = ctx; }

        bool RegisterBinding(const TemplateRenderBinding &binding);
        bool UnregisterBinding(uint32_t template_id);

        const TemplateRenderBinding *FindBinding(uint32_t template_id) const;
        bool HasBinding(uint32_t template_id) const;

        uint32_t FindSinglePrimitiveTemplate(const SinglePrimitiveTemplateKey &key) const;
        void RegisterSinglePrimitiveTemplate(const SinglePrimitiveTemplateKey &key, uint32_t template_id);

        uint32_t GetCount() const { return static_cast<uint32_t>(bindings.GetCount()); }
        void CollectRetiredTemplates();
        void Clear();
    };
}
