#include<hgl/common/RenderOptions.h>   // 编译期开关台帐（§2：对拍挂在 ULRE_ECS_DEBUG_API 下）
#include<hgl/ecs/support/MaterialRuntimeTable.h>
#include<hgl/ecs/support/MaterialRuntimePipelineTable.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/SSBOBufferRegistry.h>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    MaterialRuntimeRowID MaterialRuntimeTable::AllocateRow()
    {
        if (!free_rows.empty())
        {
            const MaterialRuntimeRowID id = free_rows.back();
            free_rows.pop_back();

            MaterialRuntimeRow &row = rows[id];

            // 世代约定（v2 §2）：0 = 死/未分配，正奇数 = 活；**复用必须 +2 保持奇数**
            // （ABA 免疫，直到 2^31 次复用）。回收时保留了上一次的世代值，这里在它之上 +2。
            row.generation += 2;
            row.alive = true;
            row.refcount = 0;
            row.owned = false;
            row.key = {};
            return id;
        }

        if (rows.size() >= kMaterialRuntimeCapacityLimit)
            return INVALID_MATERIAL_RUNTIME_ROW_ID;

        const MaterialRuntimeRowID id = static_cast<MaterialRuntimeRowID>(rows.size());

        rows.push_back(MaterialRuntimeRow{});
        rows[id].generation = 1;        // 新行从 1 起（正奇数 = 活）
        rows[id].alive = true;
        return id;
    }

    void MaterialRuntimeTable::RetireRowTextureConfiguration(MaterialRuntimeRow &row)
    {
        if (!row.material_texture_configuration.IsValid())
            return;

        // GPU 侧绑定随行存亡：行归零回收（或就地重置物化绑定）时把池行退休。
        // 世界不存在（单元测试）/注册表不可用时跳过 —— 此时本表也不持有真实 GPU 资源。
        auto *graphics_context = context ? context->GetGraphicsContext() : nullptr;
        auto *registry = graphics_context
            ? graphics_context->GetSSBOBufferRegistry()
            : nullptr;

        if (registry
         && registry->IsMaterialTextureConfigurationValid(
                row.material_texture_configuration))
        {
            registry->RetireMaterialTextureConfiguration(
                row.material_texture_configuration,
                static_cast<uint64_t>(context->GetRenderSubmissionSerial())
                    + graph::MaterialTextureConfigurationRetireEpochDelay);
        }

        row.material_texture_configuration = {};
    }

    void MaterialRuntimeTable::RecycleRow(const MaterialRuntimeRowID id)
    {
        MaterialRuntimeRow &row = rows[id];

        RetireRowTextureConfiguration(row);

        // 共享行摘掉键索引（自有行本来不在索引里）
        if (!row.owned)
            index_by_key.DeleteByKey(row.key);

        const uint32_t previous_generation = row.generation;

        row = MaterialRuntimeRow{};
        row.generation = previous_generation;   // 保留世代值：复用按 +2 保持奇数
        row.alive = false;

        free_rows.push_back(id);
        --live_row_count;
    }

    MaterialRuntimeRowID MaterialRuntimeTable::Intern(const MaterialRuntimeKey &key)
    {
        // 键 = (program 身份, 配方身份, 材质数据行) **三维全同**才算同一行；索引直接以整个
        // 结构做键（全维度 operator==），哈希碰撞不会让两个不同键共用一行。
        MaterialRuntimeRowID existing = INVALID_MATERIAL_RUNTIME_ROW_ID;
        if (index_by_key.Get(key, existing)
         && existing < rows.size()
         && rows[existing].alive)
        {
            ++rows[existing].refcount;
            return existing;
        }

        const MaterialRuntimeRowID id = AllocateRow();
        if (id == INVALID_MATERIAL_RUNTIME_ROW_ID)
        {
            // 预算制：超限 fail-fast，只告警一次（不静默扩容）。
            // 触发条件 = 键维度组合数爆炸，通常意味着有每实例/每帧维度被误加进
            // MaterialRuntimeKey（见头文件"键的规矩"）。
            if (overflow_warn_count == 0)
            {
                ++overflow_warn_count;
                GLogError("[MaterialRuntimeTable] 运行期行表超过容量上限 %u（当前活行 %u，空闲行 %u，新键 build_context=%llu recipe=%llu data_index=%u）"
                          "——拒绝登记并返回 INVALID；请检查是否有每帧/每实例维度被误加入 MaterialRuntimeKey",
                          kMaterialRuntimeCapacityLimit,
                          live_row_count,
                          static_cast<uint32_t>(free_rows.size()),
                          static_cast<unsigned long long>(key.program_build_context),
                          static_cast<unsigned long long>(key.recipe_hash),
                          key.data_index_row);
            }

            return INVALID_MATERIAL_RUNTIME_ROW_ID;
        }

        MaterialRuntimeRow &row = rows[id];

        row.key      = key;
        row.refcount = 1;
        row.owned    = false;
        row.alive    = true;
        // 注意：行上的 `data_index_row` 是**物化标记**（uint32_t(-1) = 尚未物化），
        // 与键里的授权态行号（`key.data_index_row`）不是同一个物：物化路径写回后者。

        index_by_key.Add(key, id);
        ++live_row_count;
        return id;
    }

    MaterialRuntimeRowID MaterialRuntimeTable::AddRef(const MaterialRuntimeRowID id)
    {
        if (id >= rows.size() || !rows[id].alive)
            return INVALID_MATERIAL_RUNTIME_ROW_ID;

        ++rows[id].refcount;
        return id;
    }

    bool MaterialRuntimeTable::Release(const MaterialRuntimeRowID id)
    {
        if (id >= rows.size() || !rows[id].alive)
            return false;

        MaterialRuntimeRow &row = rows[id];

        if (row.refcount == 0)
            return false;

        --row.refcount;

        if (row.refcount != 0)
            return false;

        RecycleRow(id);
        return true;
    }

    MaterialRuntimeRowID MaterialRuntimeTable::DetachOwned(const MaterialRuntimeRowID id)
    {
        if (id >= rows.size() || !rows[id].alive)
            return INVALID_MATERIAL_RUNTIME_ROW_ID;

        // 已是自有行 ⇒ 幂等
        if (rows[id].owned)
            return id;

        // 独占的共享行：**就地**转自有（摘键索引 ⇒ 不再被其它实例 intern 命中）
        if (rows[id].refcount <= 1)
        {
            index_by_key.DeleteByKey(rows[id].key);
            rows[id].owned = true;
            return id;
        }

        // 被多处引用：复制出一条自有行。注意 `AllocateRow` 可能让 `rows` 扩容搬家，
        // 之后一律按**下标**访问，不复用取出的引用。
        const MaterialRuntimeRowID owned_id = AllocateRow();
        if (owned_id == INVALID_MATERIAL_RUNTIME_ROW_ID)
        {
            if (overflow_warn_count == 0)
            {
                ++overflow_warn_count;
                GLogError("[MaterialRuntimeTable] CoW 分裂失败：行表超过容量上限 %u"
                          "（当前活行 %u，空闲行 %u）",
                          kMaterialRuntimeCapacityLimit,
                          live_row_count,
                          static_cast<uint32_t>(free_rows.size()));
            }

            return INVALID_MATERIAL_RUNTIME_ROW_ID;
        }

        MaterialRuntimeRow &owned_row = rows[owned_id];

        owned_row = rows[id];       // 内容拷贝（含绑定状态）
        owned_row.key      = {};    // 自有行**不入键索引**
        owned_row.owned    = true;
        owned_row.refcount = 1;
        owned_row.alive    = true;

        ++live_row_count;

        // 源行减一次引用（仍被其它实例引用 ⇒ 不会回收）
        Release(id);

        return owned_id;
    }

    const MaterialRuntimeRow *MaterialRuntimeTable::Get(const MaterialRuntimeRowID id) const
    {
        if (id >= rows.size() || !rows[id].alive)
            return nullptr;

        return &rows[id];
    }

    MaterialRuntimeRow *MaterialRuntimeTable::GetMutable(const MaterialRuntimeRowID id)
    {
        if (id >= rows.size() || !rows[id].alive)
            return nullptr;

        return &rows[id];
    }

    uint32_t MaterialRuntimeTable::GetRefCount(const MaterialRuntimeRowID id) const
    {
        if (id >= rows.size() || !rows[id].alive)
            return 0;

        return rows[id].refcount;
    }

    uint32_t MaterialRuntimeTable::GetGeneration(const MaterialRuntimeRowID id) const
    {
        if (id >= rows.size() || !rows[id].alive)
            return 0;

        return rows[id].generation;
    }

    uint32_t MaterialRuntimeTable::GetSharedRowCount() const
    {
        uint32_t count = 0;

        for (const MaterialRuntimeRow &row : rows)
            if (row.alive && !row.owned)
                ++count;

        return count;
    }

    uint32_t MaterialRuntimeTable::GetOwnedRowCount() const
    {
        uint32_t count = 0;

        for (const MaterialRuntimeRow &row : rows)
            if (row.alive && row.owned)
                ++count;

        return count;
    }

    MaterialRuntimeSlot &MaterialRuntimeTable::GetOrCreateSlot(const EntityID owner)
    {
        const auto it = slots.find(owner);

        if (it != slots.end())
            return it->second;

        return slots.emplace(owner, MaterialRuntimeSlot{}).first->second;
    }

    MaterialRuntimeSlot *MaterialRuntimeTable::GetSlot(const EntityID owner)
    {
        const auto it = slots.find(owner);

        return (it != slots.end()) ? &it->second : nullptr;
    }

    const MaterialRuntimeSlot *MaterialRuntimeTable::GetSlot(const EntityID owner) const
    {
        const auto it = slots.find(owner);

        return (it != slots.end()) ? &it->second : nullptr;
    }

    bool MaterialRuntimeTable::DestroySlot(const EntityID owner)
    {
        const auto it = slots.find(owner);

        if (it == slots.end())
            return false;

        // slot 持有的行引用随 slot 一起释放（行归零则回收并退休 GPU 绑定）
        Release(it->second.row);


        // B5-b：该实例的每 pass 管线缓存也要摘除（与 slot 同生命周期）
        if (context)
            context->GetMaterialRuntimePipelineTable().Clear(owner);

        slots.erase(it);
        return true;
    }

    void MaterialRuntimeTable::Clear()
    {
        for (MaterialRuntimeRow &row : rows)
            if (row.alive)
                RetireRowTextureConfiguration(row);

        rows.clear();
        free_rows.clear();
        index_by_key.Clear();
        slots.clear();
        live_row_count = 0;
        overflow_warn_count = 0;
    }
    // ── 自检（问题 8 选项 (b)）：行字段与身份键的一致性 ────────────────────────────
    //  语义与调用点见头注；**定义在 .cpp**（不是头内 inline）⇒ 破坏/还原 A/B 不受
    //  MSBuild "头内 inline 体扩散到间接调用者" 的陈旧 obj 陷阱影响（见技能记录）。
    void MaterialRuntimeTable::VerifyWrittenRecipeHash(const MaterialRuntimeRow &row,
                                                       const uint64_t written_recipe_hash,
                                                       const char *what)
    {
    #if ULRE_ECS_DEBUG_API
        if (row.owned)              // 自有行：key 为空（不入键索引）⇒ 以 row.* 为准，无一致性可查
            return;

        if (row.key.recipe_hash == written_recipe_hash)
            return;

        GLogError(u8"[MaterialRuntimeTable] 行 recipe_hash 与身份键不一致（写入点=%s）："
                  u8"key.recipe_hash=%llu 写入值=%llu（共享行的两者必须同值；自有行不参与）",
                  what,
                  static_cast<unsigned long long>(row.key.recipe_hash),
                  static_cast<unsigned long long>(written_recipe_hash));
    #else
        (void)row; (void)written_recipe_hash; (void)what;
    #endif//ULRE_ECS_DEBUG_API
    }

    void MaterialRuntimeTable::VerifyWrittenDataIndexRow(const MaterialRuntimeRow &row, const char *what)
    {
    #if ULRE_ECS_DEBUG_API
        if (row.owned)                              // 自有行：无键可比
            return;

        if (row.data_index_row == uint32_t(-1))     // 未物化哨兵：此时不与身份行号可比（**勿改成裸等值**）
            return;

        if (row.data_index_row == row.key.data_index_row)
            return;

        GLogError(u8"[MaterialRuntimeTable] 行 data_index_row 与身份键不一致（写入点=%s）："
                  u8"key.data_index_row=%u row.data_index_row=%u"
                  u8"（共享行已物化时必须同值；自有行/未物化行不参与）",
                  what, row.key.data_index_row, row.data_index_row);
    #else
        (void)row; (void)what;
    #endif//ULRE_ECS_DEBUG_API
    }


}//namespace hgl::ecs
