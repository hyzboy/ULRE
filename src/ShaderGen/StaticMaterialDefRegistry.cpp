#include <hgl/mtl/StaticMaterialDefRegistry.h>
#include <hgl/type/FNV1a.h>
#include <mutex>
#include <vector>
#include <utility>

// ---- owned storage for set/map contents ----------------------------------
// StaticMaterialDef holds raw pointers to externally-owned collections.
// For static-lifetime builtin defs the originals live forever, so we just
// store the original pointer alongside its content hash.  The registry never
// takes ownership of the pointed-to data.
// --------------------------------------------------------------------------

namespace hgl::graph::mtl
{

namespace
{

// Compute a content hash that covers everything except the `name` pointer.
static uint64_t ComputeDefContentHash(const StaticMaterialDef &def)
{
    using namespace hgl::hash;

    uint64_t h = FNV1aInit<uint64_t>();

    // primitive_type
    h = FNV1aAppendValueBytes(h, def.primitive_type);

    // vertex_entries array
    if (def.vertex_entries && def.vertex_entry_count > 0)
        h = FNV1aAppendBytes(h, def.vertex_entries,
                             sizeof(FixedVertexEntry) * def.vertex_entry_count);
    else
        h = FNV1aAppend(h, uint8_t(0)); // sentinel for empty

    // ubo_descriptors set
    if (def.ubo_descriptors)
    {
        for (const auto &elem : *def.ubo_descriptors)
            h = FNV1aAppendValueBytes(h, elem);
    }
    else
        h = FNV1aAppend(h, uint8_t(1));

    // ssbo_descriptors set
    if (def.ssbo_descriptors)
    {
        for (const auto &elem : *def.ssbo_descriptors)
            h = FNV1aAppendValueBytes(h, elem);
    }
    else
        h = FNV1aAppend(h, uint8_t(2));

    // texture_samplers map (key + value)
    if (def.texture_samplers)
    {
        for (const auto &[slot, desc] : *def.texture_samplers)
        {
            h = FNV1aAppendValueBytes(h, slot);
            h = FNV1aAppendValueBytes(h, desc.sampler_type);
            h = FNV1aAppendValueBytes(h, desc.atlas_cols);
            h = FNV1aAppendValueBytes(h, desc.atlas_rows);
            h = FNV1aAppendValueBytes(h, desc.channel_hint);
        }
    }
    else
        h = FNV1aAppend(h, uint8_t(3));

    // shader_data_schema
    h = FNV1aAppendValueBytes(h, def.shader_data_schema);

    return h;
}

// Registry state lives entirely in this anonymous namespace.
struct RegistryImpl
{
    mutable std::mutex                               mutex;
    std::vector<std::pair<uint64_t, StaticMaterialDef>> entries; // index+1 == ID
};

static RegistryImpl &GetImpl()
{
    static RegistryImpl s_impl;
    return s_impl;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// StaticMaterialDefRegistry
// ---------------------------------------------------------------------------

StaticMaterialDefRegistry &StaticMaterialDefRegistry::Instance()
{
    static StaticMaterialDefRegistry s_instance;
    return s_instance;
}

StaticMaterialDefId StaticMaterialDefRegistry::Register(const StaticMaterialDef &def)
{
    uint64_t content_hash = ComputeDefContentHash(def);

    auto &impl = GetImpl();
    std::lock_guard<std::mutex> lock(impl.mutex);

    // Linear scan is acceptable: only a handful of defs are ever registered.
    for (size_t i = 0; i < impl.entries.size(); ++i)
    {
        if (impl.entries[i].first == content_hash)
            return static_cast<StaticMaterialDefId>(i + 1);
    }

    impl.entries.emplace_back(content_hash, def);
    return static_cast<StaticMaterialDefId>(impl.entries.size()); // 1-based
}

const StaticMaterialDef *StaticMaterialDefRegistry::Get(StaticMaterialDefId id) const
{
    auto &impl = GetImpl();
    std::lock_guard<std::mutex> lock(impl.mutex);

    if (id == kInvalidStaticMaterialDefId)
        return nullptr;
    size_t idx = static_cast<size_t>(id) - 1;
    if (idx >= impl.entries.size())
        return nullptr;
    return &impl.entries[idx].second;
}

// ---------------------------------------------------------------------------
// Free function
// ---------------------------------------------------------------------------

StaticMaterialDefId AcquireStaticMaterialDefId(const StaticMaterialDef &def)
{
    return StaticMaterialDefRegistry::Instance().Register(def);
}

} // namespace hgl::graph::mtl
