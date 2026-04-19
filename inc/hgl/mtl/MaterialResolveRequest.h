#pragma once

/// MaterialResolveRequest — 延迟 MaterialBindingInstance 解析描述符
///
/// PrimitiveComponent 持有 MaterialResolveRequest，ECS MaterialResolveSystem
/// 在渲染收集前根据 record + Geometry GVF 自动解析 MI。

#include<hgl/mtl/MaterialRecipe.h>
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
        const mtl::MaterialRecipe *record = nullptr;
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

        bool NeedsResolve() const { return dirty && record != nullptr; }

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
