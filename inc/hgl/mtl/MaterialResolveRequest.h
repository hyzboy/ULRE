#pragma once

/// MaterialResolveRequest — 延迟 MaterialBindingInstance 解析描述符
///
/// PrimitiveComponent 持有 MaterialResolveRequest，ECS MaterialResolveSystem
/// 在渲染收集前根据 record + Geometry GVF 自动解析 MI。
///
/// 解析路径优先级：
///   1. recipe_id != kInvalidMaterialRecipeID → 通过 MaterialRecipeStore 查 recipe
///   2. record != nullptr                     → 直接使用指针（向后兼容）

#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/MaterialRecipeID.h>
#include<hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include<cstdint>
#include<vector>

namespace hgl::graph
{
    class MaterialBindingInstance;
    class ShaderMaterialProgram;
    class ResourceDomain;
    class VertexInputLayout;

    struct MaterialResolveRequest
    {
        mtl::MaterialRecipeID recipe_id = mtl::kInvalidMaterialRecipeID; ///< ID 路径（优先）
        const mtl::MaterialRecipe *record = nullptr;                     ///< 指针路径（向后兼容）
        std::vector<uint8_t> instance_data;             ///< MI uniform 初始数据（拷贝）
        MaterialBindingInstance *resolved_binding_instance = nullptr;        ///< 已解析的 MI（由 MaterialResolveSystem 写入）
        ShaderMaterialProgram *resolved_material = nullptr;                  ///< 已解析材质（阶段5：运行时消费优先走该缓存）
        ResourceDomain *resolved_domain = nullptr;
        uint32_t resolved_domain_id = 0xFFFFFFFFu;
        const VertexInputLayout *resolved_vil = nullptr;
        int resolved_mi_id = -1;
        GraphicsPipelinePreset resolved_preset{};
        bool dirty = true;

        void SetRecord(const mtl::MaterialRecipe *rec)
        {
            record = rec;
            recipe_id = mtl::kInvalidMaterialRecipeID;
            Invalidate();
        }

        /// 通过 ID 路径设置配方（推荐新代码使用）。
        /// 调用方需确保 store 的生命周期覆盖解析阶段；
        /// store 仅用于在 MaterialResolveSystem 中查 record，此处不存指针。
        void SetRecipeID(mtl::MaterialRecipeID id)
        {
            recipe_id = id;
            record    = nullptr;
            Invalidate();
        }

        void SetInstanceData(const void *data, uint32_t size)
        {
            if (data && size > 0)
                instance_data.assign(static_cast<const uint8_t*>(data),
                                     static_cast<const uint8_t*>(data) + size);
            else
                instance_data.clear();

            Invalidate();
        }

        void Invalidate()
        {
            dirty = true;
            resolved_binding_instance = nullptr;
            resolved_material = nullptr;
            resolved_domain = nullptr;
            resolved_domain_id = 0xFFFFFFFFu;
            resolved_vil = nullptr;
            resolved_mi_id = -1;
            resolved_preset = GraphicsPipelinePreset::Solid3D;
        }

        bool NeedsResolve() const
        {
            return dirty && (record != nullptr || recipe_id != mtl::kInvalidMaterialRecipeID);
        }

        const void *GetInstanceDataPtr() const
        {
            return instance_data.empty() ? nullptr : instance_data.data();
        }

        uint32_t GetInstanceDataSize() const
        {
            return static_cast<uint32_t>(instance_data.size());
        }
    };
}//namespace hgl::graph
