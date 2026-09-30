#include<hgl/ecs/support/MaterialVariantTable.h>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    MaterialVariantID MaterialVariantTable::Intern(const MaterialVariantKey &key)
    {
        // 键 = (build context, recipe 身份) **两维全同**才算同变体；索引直接以整个
        // 结构做键（全维度 operator==），哈希碰撞不会让两个不同键共用一条记录。
        MaterialVariantID existing = INVALID_MATERIAL_VARIANT_ID;
        if (index_by_key.Get(key, existing))
            return existing;

        if (records.size() >= kMaterialVariantCapacityLimit)
        {
            // 预算制：超限 fail-fast，只告警一次（不静默扩容）。
            // 触发条件 = 键维度组合数爆炸，通常意味着有每帧/每实例维度被误加进
            // MaterialVariantKey（见头文件"键的规矩"）。
            if (overflow_warn_count == 0)
            {
                ++overflow_warn_count;
                GLogError("[MaterialVariantTable] 变体表超过容量上限 %u（当前记录 %u，新键 build_context=%llu recipe=%llu）"
                          "——拒绝登记并返回 INVALID；请检查是否有每帧/每实例维度被误加入 MaterialVariantKey",
                          kMaterialVariantCapacityLimit,
                          static_cast<uint32_t>(records.size()),
                          static_cast<unsigned long long>(key.program_build_context),
                          static_cast<unsigned long long>(key.recipe_hash));
            }

            return INVALID_MATERIAL_VARIANT_ID;
        }

        const MaterialVariantID id = static_cast<MaterialVariantID>(records.size());

        records.push_back(MaterialVariantRecord{});
        records.back().key = key;

        index_by_key.Add(key, id);

        return id;
    }

    const MaterialVariantRecord *MaterialVariantTable::Get(const MaterialVariantID id) const
    {
        return id < records.size() ? &records[id] : nullptr;
    }

    MaterialVariantRecord *MaterialVariantTable::GetMutable(const MaterialVariantID id)
    {
        return id < records.size() ? &records[id] : nullptr;
    }

    void MaterialVariantTable::Clear()
    {
        records.clear();
        index_by_key.Clear();
        overflow_warn_count = 0;
    }
}//namespace hgl::ecs
