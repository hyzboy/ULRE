#include <hgl/graph/module/MRDManager.h>
#include <hgl/vk/VKInstanceDataDomain.h>
#include <cstring>

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

uint32_t MRDManager::RegisterDomain(InstanceDataDomain *domain)
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

void MRDManager::UnregisterDomain(InstanceDataDomain *domain)
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

IDDHandle MRDManager::Create(mtl::InstanceDataLayout layout,
                              uint32_t                max_count,
                              uint8_t                 tex_array_slots)
{
    auto *domain = new InstanceDataDomain(layout, max_count, tex_array_slots);
    const uint32_t id = RegisterDomain(domain);
    return IDDHandle{id, domain_table_[id].generation};
}

InstanceDataDomain *MRDManager::Get(IDDHandle handle) const
{
    if (!handle.IsValid() || handle.id >= static_cast<uint32_t>(domain_table_.size()))
        return nullptr;
    const DomainEntry &e = domain_table_[handle.id];
    return (e.generation == handle.generation) ? e.domain : nullptr;
}

void MRDManager::Release(IDDHandle handle)
{
    InstanceDataDomain *domain = Get(handle);
    if (!domain)
        return;
    UnregisterDomain(domain);
    delete domain;
}

IDDHandle MRDManager::GetHandle(InstanceDataDomain *domain) const
{
    if (!domain)
        return InvalidIDDHandle;
    auto it = domain_id_map_.find(domain);
    if (it == domain_id_map_.end())
        return InvalidIDDHandle;
    const uint32_t id = it->second;
    return IDDHandle{id, domain_table_[id].generation};
}

// ---------------------------------------------------------------------------
// MI slot management (P7 — stubs returning safe defaults until full impl)
// ---------------------------------------------------------------------------

int MRDManager::AllocMISlot(IDDHandle handle, const void *init_data, uint32_t size)
{
    auto *d = Get(handle);
    if (!d)
        return -1;
    int mi_id = d->AllocMISlot();
    if (mi_id >= 0 && init_data && size > 0)
    {
        void *dst = d->GetMIData(mi_id);
        if (dst)
            std::memcpy(dst, init_data, size);
    }
    return mi_id;
}

void MRDManager::FreeMISlot(IDDHandle handle, int mi_id)
{
    auto *d = Get(handle);
    if (d)
        d->FreeMISlot(mi_id);
}

void *MRDManager::GetMIData(IDDHandle handle, int mi_id) const
{
    auto *d = Get(handle);
    return d ? d->GetMIData(mi_id) : nullptr;
}

bool MRDManager::WriteMIData(IDDHandle handle, int mi_id, const void *data, uint32_t size)
{
    if (!data || size == 0)
        return false;
    auto *d = Get(handle);
    if (!d)
        return false;
    void *dst = d->GetMIData(mi_id);
    if (!dst)
        return false;
    std::memcpy(dst, data, size);
    return true;
}

bool MRDManager::EnsureGPUBuffers(IDDHandle handle, BufferManager *bm)
{
    auto *d = Get(handle);
    if (!d || !d->hasMI())
        return false;
    return d->EnsureMIBuffer(bm, d->GetMIMaxCount());
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
