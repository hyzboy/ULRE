#include <hgl/graph/module/MRDManager.h>
#include <hgl/vk/VKMaterialResourceDomain.h>

namespace hgl::graph {

// ---------------------------------------------------------------------------
// Ctor / Dtor
// ---------------------------------------------------------------------------

MRDManager::MRDManager()
{
    // index 0 = invalid sentinel; IsValid() requires id != 0
    domain_table_.push_back(DomainEntry{nullptr, 0});
}

MRDManager::~MRDManager()
{
    // Release any still-alive domains
    for (size_t i = 1; i < domain_table_.size(); ++i)
    {
        if (domain_table_[i].domain)
        {
            delete domain_table_[i].domain;
            domain_table_[i].domain = nullptr;
        }
    }
    domain_table_.clear();
    domain_id_map_.clear();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

uint32_t MRDManager::RegisterDomain(MaterialResourceDomain *domain)
{
    if (!domain)
        return 0;

    // Idempotent: if already registered, return existing id
    auto it = domain_id_map_.find(domain);
    if (it != domain_id_map_.end())
        return it->second;

    // Find a free slot (reuse released entries where generation > 0)
    for (size_t i = 1; i < domain_table_.size(); ++i)
    {
        if (!domain_table_[i].domain)
        {
            domain_table_[i].domain = domain;
            domain_table_[i].generation += 1;  // bump generation on reuse
            if (domain_table_[i].generation == 0)
                domain_table_[i].generation = 1; // never stay at 0

            const uint32_t id = static_cast<uint32_t>(i);
            domain_id_map_[domain] = id;
            return id;
        }
    }

    // No free slot — append
    const uint32_t id = static_cast<uint32_t>(domain_table_.size());
    domain_table_.push_back(DomainEntry{domain, 1});
    domain_id_map_[domain] = id;
    return id;
}

void MRDManager::UnregisterDomain(MaterialResourceDomain *domain)
{
    auto it = domain_id_map_.find(domain);
    if (it == domain_id_map_.end())
        return;

    const uint32_t id = it->second;
    domain_table_[id].domain = nullptr;
    // generation preserved so outstanding stale handles resolve to nullptr
    domain_id_map_.erase(it);
}

// ---------------------------------------------------------------------------
// Public: Create / Get / Release / GetHandle
// ---------------------------------------------------------------------------

MRDHandle MRDManager::Create(mtl::InstanceDataLayout layout,
                              uint32_t                max_count,
                              uint8_t                 tex_array_slots)
{
    auto *domain = new MaterialResourceDomain(layout, max_count, tex_array_slots);
    const uint32_t id = RegisterDomain(domain);
    return MRDHandle{id, domain_table_[id].generation};
}

MaterialResourceDomain *MRDManager::Get(MRDHandle handle) const
{
    if (!handle.IsValid() || handle.id >= static_cast<uint32_t>(domain_table_.size()))
        return nullptr;
    const DomainEntry &e = domain_table_[handle.id];
    return (e.generation == handle.generation) ? e.domain : nullptr;
}

void MRDManager::Release(MRDHandle handle)
{
    MaterialResourceDomain *domain = Get(handle);
    if (!domain)
        return;
    UnregisterDomain(domain);
    delete domain;
}

MRDHandle MRDManager::GetHandle(MaterialResourceDomain *domain) const
{
    if (!domain)
        return InvalidMRDHandle;
    auto it = domain_id_map_.find(domain);
    if (it == domain_id_map_.end())
        return InvalidMRDHandle;
    const uint32_t id = it->second;
    return MRDHandle{id, domain_table_[id].generation};
}

// ---------------------------------------------------------------------------
// MI slot management (P7 — stubs returning safe defaults until full impl)
// ---------------------------------------------------------------------------

int MRDManager::AllocMISlot(MRDHandle handle, const void * /*init_data*/, uint32_t /*size*/)
{
    // TODO(P7): delegate to MaterialResourceDomain::AllocMISlot
    (void)handle;
    return -1;
}

void MRDManager::FreeMISlot(MRDHandle handle, int /*mi_id*/)
{
    // TODO(P7): delegate to MaterialResourceDomain::FreeMISlot
    (void)handle;
}

void *MRDManager::GetMIData(MRDHandle handle, int /*mi_id*/) const
{
    // TODO(P7)
    (void)handle;
    return nullptr;
}

bool MRDManager::WriteMIData(MRDHandle handle, int /*mi_id*/, const void * /*data*/, uint32_t /*size*/)
{
    // TODO(P7)
    (void)handle;
    return false;
}

bool MRDManager::EnsureGPUBuffers(MRDHandle handle, BufferManager * /*bm*/)
{
    // TODO(P7): delegate to MaterialResourceDomain::EnsureMIBuffer / EnsureMITBuffer
    (void)handle;
    return false;
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

uint32_t MRDManager::GetDomainCount() const
{
    uint32_t count = 0;
    for (size_t i = 1; i < domain_table_.size(); ++i)
    {
        if (domain_table_[i].domain)
            ++count;
    }
    return count;
}

} // namespace hgl::graph
