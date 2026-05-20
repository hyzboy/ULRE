#include <hgl/mtl/MaterialRecipeStore.h>
#include <hgl/type/FNV1a.h>

namespace hgl::graph::mtl
{

// ─────────────────────────────────────────────────────────────────────────────
// 内容哈希实现
// ─────────────────────────────────────────────────────────────────────────────

/*static*/ uint64_t MaterialRecipeStore::ComputeContentHash(const MaterialRecipe &r)
{
    uint64_t h = hgl::hash::FNV1aInit<uint64_t>();

    // ── 字符串字段 ──────────────────────────────────────────────────────────
    h = hgl::hash::FNV1aAppendBytes(h, r.id.data(),        r.id.size());
    h = hgl::hash::FNV1aAppend(h, '\0');
    h = hgl::hash::FNV1aAppendBytes(h, r.domain_id.data(), r.domain_id.size());
    h = hgl::hash::FNV1aAppend(h, '\0');

    // ── 枚举 / POD 字段 ─────────────────────────────────────────────────────
    h = hgl::hash::FNV1aAppendValueBytes(h, r.preset);
    h = hgl::hash::FNV1aAppendValueBytes(h, r.intent_features);
    h = hgl::hash::FNV1aAppend(h, static_cast<uint8_t>(r.strict_intent_features ? 1u : 0u));
    h = hgl::hash::FNV1aAppendValueBytes(h, r.dim);
    h = hgl::hash::FNV1aAppendValueBytes(h, r.prim);
    h = hgl::hash::FNV1aAppendValueBytes(h, r.pos_format);
    h = hgl::hash::FNV1aAppendValueBytes(h, r.vertex_policy);
    h = hgl::hash::FNV1aAppendValueBytes(h, r.shading_model);

    h = hgl::hash::FNV1aAppend(h, static_cast<uint8_t>(r.has_explicit_resources ? 1u : 0u));
    if (r.has_explicit_resources)
    {
        h = hgl::hash::FNV1aAppend(h, static_cast<uint8_t>(r.resources.needs_viewport ? 1u : 0u));
        h = hgl::hash::FNV1aAppend(h, static_cast<uint8_t>(r.resources.needs_camera ? 1u : 0u));
        h = hgl::hash::FNV1aAppend(h, static_cast<uint8_t>(r.resources.needs_transform ? 1u : 0u));
        h = hgl::hash::FNV1aAppend(h, static_cast<uint8_t>(r.resources.needs_material_instance ? 1u : 0u));
        h = hgl::hash::FNV1aAppend(h, static_cast<uint8_t>(r.resources.needs_material_texture_index ? 1u : 0u));
        h = hgl::hash::FNV1aAppend(h, static_cast<uint8_t>(r.resources.enable_lighting ? 1u : 0u));
    }

    h = hgl::hash::FNV1aAppend(h, static_cast<uint8_t>(r.has_explicit_schema ? 1u : 0u));
    if (r.has_explicit_schema)
        h = hgl::hash::FNV1aAppendValueBytes(h, r.schema);

    h = hgl::hash::FNV1aAppendValueBytes(h, r.coord_2d);
    h = hgl::hash::FNV1aAppendValueBytes(h, r.sky_ambient);
    h = hgl::hash::FNV1aAppendValueBytes(h, r.pipeline);

    // ── 纹理资产引用列表（slot+path）────────────────────────────────────────
    const uint32_t tex_count = static_cast<uint32_t>(r.textures.size());
    h = hgl::hash::FNV1aAppendValueBytes(h, tex_count);
    for (const auto &tc : r.textures)
    {
        h = hgl::hash::FNV1aAppendValueBytes(h, tc.slot);
        h = hgl::hash::FNV1aAppendBytes(h, tc.path.data(), tc.path.size());
        h = hgl::hash::FNV1aAppend(h, '\0');
    }

    return h;
}

// ─────────────────────────────────────────────────────────────────────────────
// 写操作
// ─────────────────────────────────────────────────────────────────────────────

MaterialRecipeID MaterialRecipeStore::RegisterRecipe(const MaterialRecipe &recipe)
{
    const uint64_t hash = ComputeContentHash(recipe);

    // 先以读锁检查是否已有相同内容
    {
        std::shared_lock<std::shared_mutex> rd(mutex_);
        const auto it = hash_map_.find(hash);
        if (it != hash_map_.end())
            return it->second;
    }

    // 升为写锁，double-check 后插入
    std::unique_lock<std::shared_mutex> wr(mutex_);
    const auto it = hash_map_.find(hash);
    if (it != hash_map_.end())
        return it->second;

    const MaterialRecipeID new_id = static_cast<MaterialRecipeID>(recipes_.size()) + 1u;
    recipes_.push_back(recipe);
    hash_map_.emplace(hash, new_id);
    return new_id;
}

bool MaterialRecipeStore::UpdateRecipe(MaterialRecipeID id, const MaterialRecipe &new_recipe)
{
    if (id == kInvalidMaterialRecipeID)
        return false;

    const uint64_t new_hash = ComputeContentHash(new_recipe);

    std::unique_lock<std::shared_mutex> wr(mutex_);

    const size_t index = static_cast<size_t>(id) - 1u;
    if (index >= recipes_.size())
        return false;

    // 移除旧哈希映射
    const uint64_t old_hash = ComputeContentHash(recipes_[index]);
    const auto old_it = hash_map_.find(old_hash);
    if (old_it != hash_map_.end() && old_it->second == id)
        hash_map_.erase(old_it);

    // 写入新内容
    recipes_[index] = new_recipe;

    // 建立新哈希映射（如已被其他 ID 占用则跳过，避免覆盖）
    hash_map_.emplace(new_hash, id);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 读操作
// ─────────────────────────────────────────────────────────────────────────────

const MaterialRecipe *MaterialRecipeStore::GetRecipe(MaterialRecipeID id) const
{
    if (id == kInvalidMaterialRecipeID)
        return nullptr;

    std::shared_lock<std::shared_mutex> rd(mutex_);

    const size_t index = static_cast<size_t>(id) - 1u;
    if (index >= recipes_.size())
        return nullptr;

    return &recipes_[index];
}

MaterialRecipeID MaterialRecipeStore::FindByContentHash(uint64_t hash) const
{
    std::shared_lock<std::shared_mutex> rd(mutex_);

    const auto it = hash_map_.find(hash);
    return (it != hash_map_.end()) ? it->second : kInvalidMaterialRecipeID;
}

size_t MaterialRecipeStore::Size() const
{
    std::shared_lock<std::shared_mutex> rd(mutex_);
    return recipes_.size();
}

} // namespace hgl::graph::mtl
