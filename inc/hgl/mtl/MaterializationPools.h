#pragma once

#include <hgl/mtl/MaterializationResolver.h>
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace hgl::graph::mtl
{
    // Bindless 纹理池中的一条记录：
    // resource_id -> (bindless_handle, texture_layer)
    struct BindlessTextureEntry
    {
        std::string resource_id;
        uint32_t bindless_handle = 0; // 供 shader 直接采样
        uint32_t texture_layer = 0;   // 供 TextureLayer 间接表写入
    };

    // 全局 Bindless 纹理句柄分配器（Phase 3 最小骨架）。
    class BindlessTexturePool
    {
    private:
        std::unordered_map<std::string, uint32_t> handle_by_resource;
        std::vector<BindlessTextureEntry> entries;

    public:
        void Clear()
        {
            handle_by_resource.clear();
            entries.clear();
        }

        size_t GetCount() const { return entries.size(); }

        bool TryGet(const std::string &resource_id, BindlessTextureEntry &out_entry) const
        {
            auto it = handle_by_resource.find(resource_id);
            if (it == handle_by_resource.end() || it->second == 0)
                return false;

            const uint32_t handle = it->second;
            out_entry = entries[handle - 1];
            return true;
        }

        const BindlessTextureEntry &Acquire(const std::string &resource_id)
        {
            auto it = handle_by_resource.find(resource_id);
            if (it != handle_by_resource.end() && it->second > 0)
                return entries[it->second - 1];

            const uint32_t handle = static_cast<uint32_t>(entries.size() + 1); // 0 保留为无效
            BindlessTextureEntry entry{};
            entry.resource_id = resource_id;
            entry.bindless_handle = handle;
            entry.texture_layer = handle - 1;

            entries.emplace_back(std::move(entry));
            handle_by_resource[resource_id] = handle;
            return entries.back();
        }
    };

    // 结构体池布局声明（一个 struct_name 对应一种布局）。
    struct StructPoolLayout
    {
        SSBOType ssbo_type = SSBOType::UserDefined;
        uint32_t resource_domain_id = 0;
        std::string struct_name;
        uint32_t byte_stride = 0;
    };

    // 一次结构体池分配结果（用于填充 ResolvedStructRef）。
    struct StructPoolAllocation
    {
        SSBOType ssbo_type = SSBOType::UserDefined;
        uint32_t resource_domain_id = 0;
        std::string struct_name;
        uint32_t struct_index = 0;
        uint32_t byte_offset = 0;
        uint32_t byte_stride = 0;
    };

    // 结构体池索引分配器（Phase 3 最小骨架）。
    class StructDataPool
    {
    private:
        struct LayoutState
        {
            StructPoolLayout layout;
            uint32_t next_index = 0;
        };

        std::unordered_map<uint64_t, LayoutState> states;
        std::unordered_map<std::string, uint64_t> legacy_name_to_key;

        static uint64_t BuildLayoutKey(const SSBOType ssbo_type, const uint32_t resource_domain_id) noexcept
        {
            return (static_cast<uint64_t>(ssbo_type) << 32) | static_cast<uint64_t>(resource_domain_id);
        }

        bool AllocateByKey(const uint64_t key, StructPoolAllocation &out_alloc)
        {
            auto it = states.find(key);
            if (it == states.end())
                return false;

            auto &state = it->second;
            const uint32_t index = state.next_index++;

            out_alloc.ssbo_type = state.layout.ssbo_type;
            out_alloc.resource_domain_id = state.layout.resource_domain_id;
            out_alloc.struct_name = state.layout.struct_name;
            out_alloc.struct_index = index;
            out_alloc.byte_stride = state.layout.byte_stride;
            out_alloc.byte_offset = index * state.layout.byte_stride;
            return true;
        }

    public:
        bool RegisterLayout(const SSBOType ssbo_type,
                            const uint32_t resource_domain_id,
                            const uint32_t byte_stride,
                            const std::string &struct_name = {})
        {
            if (byte_stride == 0)
                return false;

            const uint64_t key = BuildLayoutKey(ssbo_type, resource_domain_id);
            auto it = states.find(key);
            if (it == states.end())
            {
                LayoutState state{};
                state.layout.ssbo_type = ssbo_type;
                state.layout.resource_domain_id = resource_domain_id;
                state.layout.struct_name = struct_name;
                state.layout.byte_stride = byte_stride;
                states.emplace(key, std::move(state));
                if (!struct_name.empty())
                    legacy_name_to_key[struct_name] = key;
                return true;
            }

            // 已存在时只允许相同布局重复注册。
            if (!struct_name.empty() && it->second.layout.struct_name.empty())
            {
                it->second.layout.struct_name = struct_name;
                legacy_name_to_key[struct_name] = key;
            }

            return it->second.layout.byte_stride == byte_stride;
        }

        bool RegisterLayout(const std::string &struct_name,
                            const SSBOType ssbo_type,
                            const uint32_t byte_stride)
        {
            if (struct_name.empty())
                return false;

            return RegisterLayout(ssbo_type, 0, byte_stride, struct_name);
        }

        bool HasLayout(const SSBOType ssbo_type, const uint32_t resource_domain_id) const
        {
            const uint64_t key = BuildLayoutKey(ssbo_type, resource_domain_id);
            return states.find(key) != states.end();
        }

        bool HasLayout(const std::string &struct_name) const
        {
            return legacy_name_to_key.find(struct_name) != legacy_name_to_key.end();
        }

        size_t GetLayoutCount() const
        {
            return states.size();
        }

        bool TryAllocate(const SSBOType ssbo_type,
                         const uint32_t resource_domain_id,
                         StructPoolAllocation &out_alloc)
        {
            return AllocateByKey(BuildLayoutKey(ssbo_type, resource_domain_id), out_alloc);
        }

        bool TryAllocate(const std::string &struct_name, StructPoolAllocation &out_alloc)
        {
            auto it = legacy_name_to_key.find(struct_name);
            if (it == legacy_name_to_key.end())
                return false;

            return AllocateByKey(it->second, out_alloc);
        }

        void ResetAllocations()
        {
            for (auto &kv : states)
                kv.second.next_index = 0;
        }

        void Clear()
        {
            states.clear();
            legacy_name_to_key.clear();
        }
    };

    // TextureLayer SSBO 的单实例行（每个语义槽一个 layer 索引）。
    struct TextureLayerRow
    {
        std::array<uint32_t, static_cast<size_t>(TextureSlot::RANGE_SIZE)> values{};
    };

    // DataIndex SSBO 的单实例行（每个语义槽一个 struct_index）。
    struct DataIndexRow
    {
        std::array<uint32_t, static_cast<size_t>(DataSlot::RANGE_SIZE)> values{};
    };

    // TextureLayer/DataIndex 间接表容器（Phase 3 最小骨架）。
    class MaterializationIndexTables
    {
    private:
        std::vector<TextureLayerRow> texture_layer_rows;
        std::vector<DataIndexRow> data_index_rows;

    public:
        void Clear()
        {
            texture_layer_rows.clear();
            data_index_rows.clear();
        }

        uint32_t PushTextureLayerRow(const TextureLayerRow &row)
        {
            texture_layer_rows.emplace_back(row);
            return static_cast<uint32_t>(texture_layer_rows.size() - 1);
        }

        uint32_t PushDataIndexRow(const DataIndexRow &row)
        {
            data_index_rows.emplace_back(row);
            return static_cast<uint32_t>(data_index_rows.size() - 1);
        }

        const TextureLayerRow *GetTextureLayerRow(const uint32_t index) const
        {
            if (index >= texture_layer_rows.size())
                return nullptr;
            return &texture_layer_rows[index];
        }

        const DataIndexRow *GetDataIndexRow(const uint32_t index) const
        {
            if (index >= data_index_rows.size())
                return nullptr;
            return &data_index_rows[index];
        }

        size_t GetTextureLayerRowCount() const { return texture_layer_rows.size(); }
        size_t GetDataIndexRowCount() const { return data_index_rows.size(); }
    };

    // 将 DataSlot 映射到默认 SSBO 分类（可在后续阶段替换为可配置策略）。
    inline SSBOType DefaultSSBOTypeForDataSlot(const DataSlot slot) noexcept
    {
        switch (slot)
        {
        case DataSlot::PBRSurface: return SSBOType::PBRSurface;
        case DataSlot::EmissiveSurface: return SSBOType::EmissiveSurface;
        case DataSlot::ClearCoatSurface: return SSBOType::ClearCoatSurface;
        case DataSlot::TransmissionSurface: return SSBOType::TransmissionSurface;
        default: return SSBOType::UserDefined;
        }
    }

    inline SSBOCategory DefaultCategoryForDataSlot(const DataSlot slot) noexcept
    {
        return DefaultSSBOTypeForDataSlot(slot);
    }

    // 用池对象构建 Recipe->Spec 解析回调。
    inline MaterializationResolveCallbacks MakePoolResolveCallbacks(BindlessTexturePool &texture_pool,
                                                                    StructDataPool &struct_pool)
    {
        MaterializationResolveCallbacks callbacks{};

        callbacks.resolve_texture = [&texture_pool](const RecipeTextureBinding &input, ResolvedResource &output)
        {
            if (input.resource_id.empty())
                return false;

            const auto &entry = texture_pool.Acquire(input.resource_id);
            output.slot = input.slot;
            output.bindless_handle = entry.bindless_handle;
            output.texture_layer = entry.texture_layer;
            return true;
        };

        callbacks.resolve_struct = [&struct_pool](const RecipeStructBinding &input, ResolvedStructRef &output)
        {
            StructPoolAllocation alloc{};
            bool allocated = false;

            if (input.ssbo_type != SSBOType::UserDefined || input.resource_domain_id != 0)
                allocated = struct_pool.TryAllocate(input.ssbo_type, input.resource_domain_id, alloc);

            if (!allocated && !input.struct_name.empty())
                allocated = struct_pool.TryAllocate(input.struct_name, alloc);

            if (!allocated)
                return false;

            output.slot = input.slot;
            output.ssbo_type = alloc.ssbo_type;
            output.resource_domain_id = alloc.resource_domain_id;
            output.ssbo_binding = static_cast<uint32_t>(alloc.ssbo_type); // 临时默认映射，后续由布局系统接管
            output.struct_index = alloc.struct_index;
            output.byte_offset = alloc.byte_offset;
            output.byte_stride = alloc.byte_stride;
            return true;
        };

        return callbacks;
    }

    // 从已解析 spec 构建单实例 TextureLayer/DataIndex 行。
    inline TextureLayerRow BuildTextureLayerRow(const MaterializationSpec &spec)
    {
        TextureLayerRow row{};
        for (const auto &res : spec.resources)
        {
            const size_t idx = static_cast<size_t>(res.slot);
            if (idx < row.values.size())
                row.values[idx] = res.texture_layer;
        }
        return row;
    }

    inline DataIndexRow BuildDataIndexRow(const MaterializationSpec &spec)
    {
        DataIndexRow row{};
        for (const auto &ref : spec.struct_refs)
        {
            const size_t idx = static_cast<size_t>(ref.slot);
            if (idx < row.values.size())
                row.values[idx] = ref.struct_index;
        }
        return row;
    }

    // 将 spec 写入间接表并返回行索引（用于实例绑定）。
    inline bool WriteSpecToIndexTables(const MaterializationSpec &spec,
                                       MaterializationIndexTables &tables,
                                       uint32_t &out_texture_layer_row,
                                       uint32_t &out_data_index_row)
    {
        out_texture_layer_row = tables.PushTextureLayerRow(BuildTextureLayerRow(spec));
        out_data_index_row = tables.PushDataIndexRow(BuildDataIndexRow(spec));
        return true;
    }
}
