#include <hgl/graph/module/IDDManager.h>
#include <hgl/vk/VKInstanceDataDomain.h>
#include <hgl/vk/MITBuffer.h>
#include <cstring>

namespace hgl::graph {

// ---------------------------------------------------------------------------
// Ctor / Dtor
// ---------------------------------------------------------------------------

IDDManager::IDDManager()
{
    // index 0 = invalid sentinel; IsValid() requires id != 0
    domain_table_.push_back(DomainEntry{nullptr, nullptr, 0});
}

IDDManager::~IDDManager()
{
    // Release any still-alive domains
    for (size_t i = 1; i < domain_table_.size(); ++i)
    {
        delete domain_table_[i].mit_buffer;
        domain_table_[i].mit_buffer = nullptr;

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

uint32_t IDDManager::RegisterDomain(InstanceDataDomain *domain)
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
    domain_table_.push_back(DomainEntry{domain, nullptr, 1});
    domain_id_map_[domain] = id;
    return id;
}

void IDDManager::UnregisterDomain(InstanceDataDomain *domain)
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

IDDHandle IDDManager::Create(mtl::InstanceDataLayout layout,
                              uint32_t                max_count,
                              uint8_t                 tex_array_slots)
{
    auto *domain = new InstanceDataDomain(layout, max_count, tex_array_slots);
    const uint32_t id = RegisterDomain(domain);

    // 当域声明了纹理槽位时，同步创建 MIT buffer host
    if (tex_array_slots != 0)
        domain_table_[id].mit_buffer = new MITBuffer();

    return IDDHandle{id, domain_table_[id].generation};
}

InstanceDataDomain *IDDManager::Get(IDDHandle handle) const
{
    if (!handle.IsValid() || handle.id >= static_cast<uint32_t>(domain_table_.size()))
        return nullptr;
    const DomainEntry &e = domain_table_[handle.id];
    return (e.generation == handle.generation) ? e.domain : nullptr;
}

void IDDManager::Release(IDDHandle handle)
{
    InstanceDataDomain *domain = Get(handle);
    if (!domain)
        return;

    // Clean up MIT buffer before unregistering
    if (handle.id < static_cast<uint32_t>(domain_table_.size()))
    {
        delete domain_table_[handle.id].mit_buffer;
        domain_table_[handle.id].mit_buffer = nullptr;
    }

    UnregisterDomain(domain);
    delete domain;
}

IDDHandle IDDManager::GetHandle(InstanceDataDomain *domain) const
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

int IDDManager::AllocSlot(IDDHandle handle, const void *init_data, uint32_t size)
{
    auto *d = Get(handle);
    if (!d)
        return -1;
    int slot_id = d->AllocSlot();
    if (slot_id >= 0 && init_data && size > 0)
    {
        void *dst = d->GetSlotData(slot_id);
        if (dst)
            std::memcpy(dst, init_data, size);
    }
    return slot_id;
}

void IDDManager::FreeSlot(IDDHandle handle, int slot_id)
{
    auto *d = Get(handle);
    if (d)
        d->FreeSlot(slot_id);
}

void *IDDManager::GetSlotData(IDDHandle handle, int slot_id) const
{
    auto *d = Get(handle);
    return d ? d->GetSlotData(slot_id) : nullptr;
}

bool IDDManager::WriteSlotData(IDDHandle handle, int slot_id, const void *data, uint32_t size)
{
    if (!data || size == 0)
        return false;
    auto *d = Get(handle);
    if (!d)
        return false;
    void *dst = d->GetSlotData(slot_id);
    if (!dst)
        return false;
    std::memcpy(dst, data, size);
    return true;
}

bool IDDManager::EnsureGPUBuffers(IDDHandle handle, BufferManager *bm)
{
    auto *d = Get(handle);
    if (!d || !d->HasLayout())
        return false;
    return d->EnsureGPUBuffer(bm, d->GetMaxCount());
}

// ---------------------------------------------------------------------------
// MIT buffer access
// ---------------------------------------------------------------------------

MITBuffer *IDDManager::GetMITBuffer(IDDHandle handle) const
{
    if (!handle.IsValid() || handle.id >= static_cast<uint32_t>(domain_table_.size()))
        return nullptr;
    const DomainEntry &e = domain_table_[handle.id];
    return (e.generation == handle.generation) ? e.mit_buffer : nullptr;
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

uint32_t IDDManager::GetDomainCount() const
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
