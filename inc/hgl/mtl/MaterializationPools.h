#pragma once

#include <hgl/mtl/MaterializationResolver.h>
#include <hgl/log/Log.h>
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

        /**
         * 用外部已分配好的 handle 预注册一个 resource_id。
         * 用于将 BindlessTextureManager 分配的 Vulkan 侧 handle 和 Recipe 侧 pool handle 对齐。
         * @return 注册成功返回 handle，若 resource_id 已存在且 handle 不同则返回 0（冲突）。
         */
        uint32_t RegisterWithHandle(const std::string &resource_id, const uint32_t handle)
        {
            if (resource_id.empty() || handle == 0)
                return 0;

            auto it = handle_by_resource.find(resource_id);
            if (it != handle_by_resource.end())
                return (it->second == handle) ? handle : 0; // 已存在，验证一致性

            BindlessTextureEntry entry{};
            entry.resource_id = resource_id;
            entry.bindless_handle = handle;
            entry.texture_layer = handle - 1;

            // 确保 entries 数组足够大
            if (handle > static_cast<uint32_t>(entries.size()))
                entries.resize(handle);

            entries[handle - 1] = entry;
            handle_by_resource[resource_id] = handle;
            return handle;
        }
    };

    // 结构体池布局声明。运行时主键是 (ssbo_type, ssbo_id)；
    // struct_name 只保留在 ShaderGen/调试兼容层，不再作为运行时路由键。
    struct StructPoolLayout
    {
        SSBOType ssbo_type = SSBOType::UserDefined;
        uint32_t ssbo_id = 0;
        uint32_t byte_stride = 0;
    };

    // 一次结构体池分配结果（用于填充 ResolvedStructRef）。
    struct StructPoolAllocation
    {
        SSBOType ssbo_type = SSBOType::UserDefined;
        uint32_t ssbo_id = 0;
        uint32_t ssbo_element_index = 0;
        uint32_t byte_stride = 0;
    };

    // 结构体池索引分配器（Phase 3 最小骨架）。
    class StructDataPool
    {
    private:
        struct LayoutState
        {
            StructPoolLayout layout;
        };

        std::unordered_map<uint64_t, LayoutState> states;

        static uint64_t BuildLayoutKey(const SSBOType ssbo_type, const uint32_t ssbo_id) noexcept
        {
            return (static_cast<uint64_t>(ssbo_type) << 32) | static_cast<uint64_t>(ssbo_id);
        }

        bool ResolveByKey(const uint64_t key, StructPoolAllocation &out_alloc) const
        {
            auto it = states.find(key);
            if (it == states.end())
                return false;

            const auto &state = it->second;
            // Default to row 0 for domain-managed per-id buffers.
            // Explicit row selection must be provided by authoring (use_ssbo_element_index=true).
            const uint32_t index = 0;

            out_alloc.ssbo_type = state.layout.ssbo_type;
            out_alloc.ssbo_id = state.layout.ssbo_id;
            out_alloc.ssbo_element_index = index;
            out_alloc.byte_stride = state.layout.byte_stride;
            return true;
        }

    public:
        bool RegisterLayout(const SSBOType ssbo_type,
                            const uint32_t ssbo_id,
                            const uint32_t byte_stride)
        {
            if (byte_stride == 0)
                return false;

            const uint32_t expected_version = GetSSBOTypeStructVersion(ssbo_type);
            const uint32_t expected_stride = GetSSBOTypeStructStride(ssbo_type);
            if (expected_version > 0 && expected_stride > 0 && byte_stride != expected_stride)
                return false;

            const uint64_t key = BuildLayoutKey(ssbo_type, ssbo_id);
            auto it = states.find(key);
            if (it == states.end())
            {
                LayoutState state{};
                state.layout.ssbo_type = ssbo_type;
                state.layout.ssbo_id = ssbo_id;
                state.layout.byte_stride = byte_stride;
                states.emplace(key, std::move(state));
                return true;
            }

            return it->second.layout.byte_stride == byte_stride;
        }

        bool HasLayout(const SSBOType ssbo_type, const uint32_t ssbo_id) const
        {
            const uint64_t key = BuildLayoutKey(ssbo_type, ssbo_id);
            return states.find(key) != states.end();
        }

        size_t GetLayoutCount() const
        {
            return states.size();
        }

        bool TryResolve(const SSBOType ssbo_type,
                        const uint32_t ssbo_id,
                        StructPoolAllocation &out_alloc) const
        {
            return ResolveByKey(BuildLayoutKey(ssbo_type, ssbo_id), out_alloc);
        }

        void Clear()
        {
            states.clear();
        }
    };

    // TextureLayer SSBO 的单实例行（每个语义槽一个 layer 索引）。
    struct TextureLayerRow
    {
        std::array<uint32_t, static_cast<size_t>(TextureSlot::RANGE_SIZE)> values{};
    };

    // MaterialSSBOIndexTable SSBO 的单实例行（每个结构体槽位一个 ssbo_element_index）。
    struct MaterialSSBOIndexRow
    {
        std::array<uint32_t, static_cast<size_t>(MaterialSSBOSlotCount)> values{};
    };
    static_assert(sizeof(MaterialSSBOIndexRow::values[0]) == sizeof(uint32_t),
                  "MaterialSSBOIndexRow values must remain uint32_t for SSBO row format stability.");

    // TextureLayer/DataIndex 间接表容器（Phase 3 最小骨架）。
    class MaterializationIndexTables
    {
    private:
        std::vector<TextureLayerRow> texture_layer_rows;
        std::vector<MaterialSSBOIndexRow> data_index_rows;

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

        uint32_t PushMaterialSSBOIndexRow(const MaterialSSBOIndexRow &row)
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

        const MaterialSSBOIndexRow *GetMaterialSSBOIndexRow(const uint32_t index) const
        {
            if (index >= data_index_rows.size())
                return nullptr;
            return &data_index_rows[index];
        }

        size_t GetTextureLayerRowCount() const { return texture_layer_rows.size(); }
        size_t GetMaterialSSBOIndexRowCount() const { return data_index_rows.size(); }
    };

    // 用池对象构建 Recipe->Spec 解析回调。
    inline MaterializationResolveCallbacks MakePoolResolveCallbacks(BindlessTexturePool &texture_pool,
                                                                    StructDataPool &struct_pool)
    {
        MaterializationResolveCallbacks callbacks{};

        callbacks.resolve_texture = [&texture_pool](const RecipeTextureBinding &input, ResolvedResource &output)
        {
            if (input.use_direct_value)
            {
                output.slot = input.slot;
                output.bindless_handle = input.direct_value;
                output.texture_layer = input.direct_value;
                return true;
            }

            if (input.resource_id.empty())
            {
                GLogWarning("[TexTrace] resolve_texture: slot=%u resource_id EMPTY -> fail",
                            static_cast<uint32_t>(input.slot));
                return false;
            }

            BindlessTextureEntry existing_entry{};
            const bool already_registered = texture_pool.TryGet(input.resource_id, existing_entry);
            if (!already_registered)
            {
                GLogWarning("[TexTrace] resolve_texture: slot=%u resource_id=%s NOT in pool (Acquire will create placeholder handle)",
                            static_cast<uint32_t>(input.slot), input.resource_id.c_str());
            }

            const auto &entry = texture_pool.Acquire(input.resource_id);
            output.slot = input.slot;
            output.bindless_handle = entry.bindless_handle;
            output.texture_layer = entry.texture_layer;
            GLogInfo("[TexTrace] resolve_texture: slot=%u resource_id=%s handle=%u layer=%u (pool_had_entry=%d)",
                     static_cast<uint32_t>(input.slot), input.resource_id.c_str(),
                     entry.bindless_handle, entry.texture_layer, already_registered ? 1 : 0);
            return true;
        };

        callbacks.resolve_struct = [&struct_pool](const RecipeStructBinding &input, ResolvedStructRef &output)
        {
            StructPoolAllocation alloc{};
            if (!struct_pool.TryResolve(input.ssbo_type, input.ssbo_id, alloc))
                return false;

            output.ssbo_slot = input.ssbo_slot;
            output.ssbo_type = alloc.ssbo_type;
            output.ssbo_id = alloc.ssbo_id;
            output.ssbo_binding = static_cast<uint32_t>(alloc.ssbo_type); // 临时默认映射，后续由布局系统接管
            output.ssbo_element_index = input.use_ssbo_element_index ? input.ssbo_element_index : alloc.ssbo_element_index;
            output.byte_stride = alloc.byte_stride;
            return true;
        };

        return callbacks;
    }

    // 从已解析 spec 构建单实例 TextureLayer 行。
    // 注意：bindless 模式下，row.values[slot] = bindless_handle（非 texture_layer）。
    inline TextureLayerRow BuildTextureLayerRow(const MaterializationSpec &spec)
    {
        TextureLayerRow row{};
        for (const auto &res : spec.resources)
        {
            const size_t idx = static_cast<size_t>(res.slot);
            if (idx < row.values.size())
                row.values[idx] = res.bindless_handle;   // bindless: 直接存 handle（1-based）
        }
        return row;
    }

    inline MaterialSSBOIndexRow BuildMaterialSSBOIndexRow(const MaterializationSpec &spec)
    {
        MaterialSSBOIndexRow row{};
        for (const auto &ref : spec.struct_refs)
        {
            const size_t idx = static_cast<size_t>(ref.ssbo_slot);
            if (idx < row.values.size())
                row.values[idx] = ref.ssbo_element_index;
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
        out_data_index_row = tables.PushMaterialSSBOIndexRow(BuildMaterialSSBOIndexRow(spec));
        return true;
    }
}
