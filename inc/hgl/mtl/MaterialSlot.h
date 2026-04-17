#pragma once

/// MaterialSlot — 延迟 MaterialInstance 解析描述符
///
/// PrimitiveComponent 持有 MaterialSlot，ECS MaterialResolveSystem
/// 在渲染收集前根据 record + Geometry GVF 自动解析 MI。

#include<hgl/mtl/MaterialAssetRecord.h>
#include<cstdint>
#include<vector>

namespace hgl::graph
{
    class MaterialInstance;

    struct MaterialSlot
    {
        const mtl::MaterialAssetRecord *record = nullptr;
        std::vector<uint8_t> instance_data;             ///< MI uniform 初始数据（拷贝）
        MaterialInstance *resolved_mi = nullptr;        ///< 已解析的 MI（由 MaterialResolveSystem 写入）
        bool dirty = true;

        void SetRecord(const mtl::MaterialAssetRecord *rec)
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
            resolved_mi = nullptr;
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
