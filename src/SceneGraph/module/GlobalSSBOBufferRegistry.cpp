#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/vk/buffer/DeviceBuffer.h>
#include <hgl/log/Log.h>

namespace hgl::graph
{

namespace
{
    static constexpr GlobalSSBOConfig kGlobalSSBOConfigs[GlobalSSBOTypeCount] =
    {
        { GlobalSSBOType::MeshDrawParams,       "MeshDrawParams",       sizeof(mtl::MeshDrawParams), 16384u, 1u },
        { GlobalSSBOType::PBRSurface,          "PBRSurface",          sizeof(ssbo::PBRSurfaceRow), 1024u, 1u },
        { GlobalSSBOType::EmissiveSurface,     "EmissiveSurface",     sizeof(ssbo::EmissiveSurfaceRow), 1024u, 1u },
        { GlobalSSBOType::TransmissionSurface, "TransmissionSurface", sizeof(ssbo::TransmissionSurfaceRow), 1024u, 1u },
    };
}

GRAPH_MODULE_CONSTRUCT(GlobalSSBOBufferRegistry)
{
}

void GlobalSSBOBufferRegistry::OnGraphicsContextChanged(GraphicsContext *gc)
{
    if (!gc || initialized)
        return;

    if (!InitializePools())
    {
        GLogError("[GlobalSSBOBufferRegistry] Failed to initialize global SSBO pools");
    }
}

void GlobalSSBOBufferRegistry::Release()
{
    if (global_addresses_ubo)
    {
        delete global_addresses_ubo;
        global_addresses_ubo = nullptr;
    }

    if (global_addresses_ubo_buffer)
    {
        if (auto *gc = GetGraphicsContext())
        {
            if (auto *bm = gc->GetBufferManager())
                bm->Release(global_addresses_ubo_buffer);
        }
        global_addresses_ubo_buffer = nullptr;
    }

    for (uint32_t index = 0; index < GlobalSSBOTypeCount; ++index)
        pools[index].Reset();

    initialized = false;
}

bool GlobalSSBOBufferRegistry::CreatePool(const GlobalSSBOConfig &config)
{
    auto *pool = GetPool(config.type);
    if (!pool || pool->GetBuffer())
        return false;

    const uint32_t ssbo_id = mtl::MakeRecipeSSBOId(static_cast<uint32_t>(config.type) + 1u);
    const AnsiString pool_name = AnsiString("Global:") + config.name;

    if (!pool->Create(GetDevice(),
                      pool_name,
                      config.row_bytes,
                      config.default_capacity,
                      ssbo_id,
                      config.reserve_rows))
    {
        GLogError("[GlobalSSBOBufferRegistry] Pool creation failed: %s", config.name);
        return false;
    }

    return true;
}

bool GlobalSSBOBufferRegistry::InitializeGlobalAddressesUBO()
{
    if (global_addresses_ubo)
        return true;

    auto *gc = GetGraphicsContext();
    if (!gc)
        return false;

    auto *bm = gc->GetBufferManager();
    if (!bm)
        return false;

    global_addresses_ubo_buffer = bm->CreateUBO("GlobalAddressesUBO", StructView<GlobalAddresses>::GetSize());
    if (!global_addresses_ubo_buffer)
    {
        GLogError("[GlobalSSBOBufferRegistry] Failed to create GlobalAddressesUBO buffer");
        return false;
    }

    global_addresses_ubo_buffer->SetUpdateClass(BufferUpdateClass::Default);
    global_addresses_ubo = StructView<GlobalAddresses>::Create(global_addresses_ubo_buffer, false);
    if (!global_addresses_ubo)
    {
        GLogError("[GlobalSSBOBufferRegistry] Failed to create StructView for GlobalAddressesUBO");
        return false;
    }

    GlobalAddresses ga{};
    ga.addr_mesh_draw_params     = GetGPUBase(GlobalSSBOType::MeshDrawParams);
    ga.addr_pbr_surface          = GetGPUBase(GlobalSSBOType::PBRSurface);
    ga.addr_emissive_surface     = GetGPUBase(GlobalSSBOType::EmissiveSurface);
    ga.addr_transmission_surface = GetGPUBase(GlobalSSBOType::TransmissionSurface);
    ga.addr_global_render_items  = 0;
    ga.addr_draw_item_ids        = 0;

    global_addresses_ubo->Update(ga);
    global_addresses_ubo->Commit();

    return true;
}

void GlobalSSBOBufferRegistry::UpdateRenderItemAddresses(uint64_t addr_render_items, uint64_t addr_draw_item_ids)
{
    if (!global_addresses_ubo)
        return;

    GlobalAddresses *ga = global_addresses_ubo->Data();
    if (!ga)
        return;

    if (ga->addr_global_render_items != addr_render_items || ga->addr_draw_item_ids != addr_draw_item_ids)
    {
        ga->addr_global_render_items = addr_render_items;
        ga->addr_draw_item_ids = addr_draw_item_ids;
        global_addresses_ubo->Commit();
    }
}

bool GlobalSSBOBufferRegistry::InitializePools()
{
    if (initialized)
        return true;

    for (uint32_t index = 0; index < GlobalSSBOTypeCount; ++index)
    {
        if (!CreatePool(kGlobalSSBOConfigs[index]))
        {
            Release();
            return false;
        }
    }

    if (!InitializeGlobalAddressesUBO())
    {
        Release();
        return false;
    }

    initialized = true;
    return true;
}

bool GlobalSSBOBufferRegistry::TryGetRowBuffer(
    const uint32_t ssbo_id,
    GlobalRowBufferInfo &out_info) const
{
    if (ssbo_id == 0)
        return false;

    for (uint32_t index = 0; index < GlobalSSBOTypeCount; ++index)
    {
        const auto &pool = pools[index];
        if (pool.GetSSBOId() != ssbo_id || !pool.GetBuffer())
            continue;

        const auto gtype = static_cast<GlobalSSBOType>(index);
        out_info.global_ssbo_type = gtype;
        out_info.cpu_base = pool.GetCPUBase();
        out_info.gpu_base = pool.GetGPUBase();
        out_info.row_bytes = pool.GetRowBytes();
        out_info.row_capacity = pool.GetRowCapacity();
        out_info.buffer = pool.GetBuffer();
        return true;
    }

    return false;
}

} // namespace hgl::graph
