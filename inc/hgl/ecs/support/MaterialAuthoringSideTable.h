#pragma once

#include<hgl/CoreType.h>
#include<hgl/log/Log.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/type/String.h>
#include<hgl/type/UnorderedMap.h>
#include<cstdint>
#include<string>

namespace hgl::graph
{
    class Sampler;
    class Texture;
}//namespace hgl::graph

namespace hgl::ecs
{
    /// 具名纹理资源的**选择器**类型（B4-iii-b：原为 `MaterialData` 的嵌套枚举，随重类型移出）。
    /// 在 `hgl::ecs` 域内保持同名 ⇒ 组件与调用点的不限定写法零改动。
    enum class MaterialTextureResourceKind : uint8_t
    {
        Texture2D = 0,
        Texture2DArray
    };

    /// **材质行 0 哨兵**（唯一具名权威）：作者侧旁表的 `row_id` 0 不表示任何材质（`MaterialData` 侧的 0 行
    /// 同为"无材质"）。原先多处用裸 `0` 复述该约定（审计 §4.2）；2026-10-14 具名。
    inline constexpr uint32_t kNoMaterialRow = 0;

    static_assert(kNoMaterialRow == 0u, "0 行 = 无材质数据哨兵");

    /// 具名纹理授权资源（B4-iii-b：原为 `MaterialData` 的嵌套类型；随重类型整体移入旁表，
    /// 组件不再持有它 —— 类型名与限定名在 `hgl::ecs` 内保持不变，调用点零改动）。
    struct MaterialTextureAuthoringResource
    {
        std::string resource_id;
        hgl::graph::Texture *texture = nullptr;
        hgl::graph::Sampler *sampler = nullptr;
        /// 注：kind/array_layer/required 是**选择器**（不是每实体 GPU 数据）——
        /// 留在旁表条目里即可，不进设备级行。
        MaterialTextureResourceKind kind = MaterialTextureResourceKind::Texture2D;
        uint32_t array_layer = 0;
        bool required = false;
    };

    /**
     * 材质授权**旁表**（B4-iii-b，B4 拍板「路 B」的第三步）。
     *
     * 为什么存在：设备级组件行（`graph::ssbo::MaterialDataRow`，16B）**只能放 POD**，
     * 而配方覆盖（`mtl::MaterialRecipe`）与具名纹理资源（`UnorderedMap<AnsiString, …>`）
     * 是重类型 ⇒ 按「行内只留 generation + side_row，重类型落旁表**按行号索引**」的规则
     * 落在这里：**键 = 材质组件行行号**（`MaterialData::material_row_id`，设备级组件行）。
     *
     * 归属：**世界级**（`ECSContext` 持有，与 MaterialVariantTable / MaterialRuntimeTable 同域）——
     * 行的行号是设备级（跨世界可解析），但重类型数据只被本世界的组件与渲染路径消费；
     * 键仍用行号 ⇒ 与 B8 的 `EntityGPU::row[16]` 是同一套寻址。
     *
     * 生命周期：**随组件分离而摘除**（`MaterialData::OnDetach`）；行本身不归还（句柄语义）。
     */
    class MaterialAuthoringSideTable
    {
    public:

        /// 一行材质数据的重类型授权数据（键 = 材质组件行行号）
        struct Entry
        {
            bool has_recipe_override = false;
            hgl::graph::mtl::MaterialRecipe recipe_override;
            hgl::UnorderedMap<hgl::AnsiString, MaterialTextureAuthoringResource>
                named_texture_resources;
        };

    public:

        /// 取（无则建）行号对应的条目；`row_id == 0`（无材质行哨兵）⇒ nullptr
        Entry *GetOrCreate(uint32_t row_id);

        /// 查条目（无则 nullptr）
        Entry *Find(uint32_t row_id);
        const Entry *Find(uint32_t row_id) const;

        /// 摘除条目（组件分离时调用）；返回是否真的摘到
        bool Remove(uint32_t row_id);

    private:

        hgl::UnorderedMap<uint32_t, Entry> entries;
    };

    inline MaterialAuthoringSideTable::Entry *
        MaterialAuthoringSideTable::GetOrCreate(const uint32_t row_id)
    {
        if (row_id == kNoMaterialRow)
        {
            GLogError("[MaterialAuthoringSideTable] 拒绝行 %u（= 「无材质数据」哨兵 kNoMaterialRow）",
                      kNoMaterialRow);
            return nullptr;
        }

        if (Entry *exist = entries.GetValuePointer(row_id))
            return exist;

        if (!entries.Add(row_id, Entry{}))
        {
            GLogError("[MaterialAuthoringSideTable] 旁表分配失败 row=%u（内存不足？）", row_id);
            return nullptr;
        }

        return entries.GetValuePointer(row_id);
    }

    inline MaterialAuthoringSideTable::Entry *
        MaterialAuthoringSideTable::Find(const uint32_t row_id)
    {
        return row_id == 0 ? nullptr : entries.GetValuePointer(row_id);
    }

    inline const MaterialAuthoringSideTable::Entry *
        MaterialAuthoringSideTable::Find(const uint32_t row_id) const
    {
        return row_id == 0 ? nullptr : entries.GetValuePointer(row_id);
    }

    inline bool MaterialAuthoringSideTable::Remove(const uint32_t row_id)
    {
        return row_id == 0 ? false : entries.DeleteByKey(row_id);
    }
}//namespace hgl::ecs
