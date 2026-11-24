#pragma once

#include <hgl/type/ValueArray.h>
#include <hgl/type/PtrArray.h>
#include <hgl/type/UnorderedMap.h>
#include <hgl/ecs/support/material/MaterialRuntimeTable.h>
#include <cstdint>

namespace hgl::ecs
{
    class ECSContext;

    /**
     * 模板内一个 Shading Group 的渲染元数据描述
     */
    struct TemplateShadingGroupDesc
    {
        MaterialRuntimeRowID runtime_row = INVALID_MATERIAL_RUNTIME_ROW_ID; ///< 该组代表运行期材质行
        ValueArray<uint32_t> slot_geometry; ///< 组内各 bucket_slot 对应的 geometry_id
        ValueArray<uint32_t> slot_count;    ///< 组内各 bucket_slot 在单实例中出现的部件次数
        bool casts_shadow = true;           ///< 组内是否存在 CanCastShadow 部件
    };

    /**
     * 模板级渲染绑定：持有本模板全部部件的材质行引用与 Shading Group 划分
     */
    struct TemplateRenderBinding
    {
        uint32_t template_id = 0;
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
        UnorderedMap<uint32_t, TemplateRenderBinding *> bindings;

        void ReleaseBindingRows(const TemplateRenderBinding &binding);

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

        uint32_t GetCount() const { return static_cast<uint32_t>(bindings.GetCount()); }
        void Clear();
    };
}
