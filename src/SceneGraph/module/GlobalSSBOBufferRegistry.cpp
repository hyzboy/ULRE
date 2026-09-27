#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/CameraInfo.h>
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
        { GlobalSSBOType::CameraInfo,          "CameraInfo",          sizeof(CameraInfo), 64u, 0u },
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
    if (global_addresses_table)
    {
        delete global_addresses_table;
        global_addresses_table = nullptr;
    }

    if (global_addresses_table_buffer)
    {
        if (auto *gc = GetGraphicsContext())
        {
            if (auto *bm = gc->GetBufferManager())
                bm->Release(global_addresses_table_buffer);
        }
        global_addresses_table_buffer = nullptr;
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

bool GlobalSSBOBufferRegistry::InitializeGlobalAddressesTable()
{
    if (global_addresses_table)
        return true;

    auto *gc = GetGraphicsContext();
    if (!gc)
        return false;

    auto *bm = gc->GetBufferManager();
    if (!bm)
        return false;

    // BDA：必须以 SHADER_DEVICE_ADDRESS usage 创建才拿得到设备地址（CreateUBO 的额外
    // usage 位是 0）；分配策略（ReBAR/暂存）由 usage 位派生，StructView 的写入路径不变。
    global_addresses_table_buffer = bm->CreateSSBO("GlobalAddressesTable",
                                                 StructView<GlobalAddresses>::GetSize());
    if (!global_addresses_table_buffer)
    {
        GLogError("[GlobalSSBOBufferRegistry] Failed to create GlobalAddressesTable buffer");
        return false;
    }

    global_addresses_table_buffer->SetUpdateClass(BufferUpdateClass::Default);
    global_addresses_table = StructView<GlobalAddresses>::Create(global_addresses_table_buffer, false);
    if (!global_addresses_table)
    {
        GLogError("[GlobalSSBOBufferRegistry] Failed to create StructView for GlobalAddressesTable");
        return false;
    }

    global_addresses_addr =
        gc->GetDevice()->GetBufferDeviceAddressAligned16(global_addresses_table_buffer->GetBuffer());
    if (global_addresses_addr == 0)
    {
        // 地址缺失 = shader 解引用 0 基址 = UB / 设备丢失（0 VUID 判据抓不到）⇒ fail-fast
        GLogError("[GlobalSSBOBufferRegistry] GlobalAddressesTable 取不到设备地址（16B 对齐 / usage 检查）");
        return false;
    }

    GlobalAddresses ga{};
    ga.addr_mesh_draw_params     = GetGPUBase(GlobalSSBOType::MeshDrawParams);
    ga.addr_pbr_surface          = GetGPUBase(GlobalSSBOType::PBRSurface);
    ga.addr_emissive_surface     = GetGPUBase(GlobalSSBOType::EmissiveSurface);
    ga.addr_transmission_surface = GetGPUBase(GlobalSSBOType::TransmissionSurface);
    ga.addr_global_render_items  = 0;
    ga.addr_draw_item_ids        = 0;
    ga.addr_camera_info          = GetGPUBase(GlobalSSBOType::CameraInfo);
    ga.addr_color_palette        = 0;   // 由 ColorPaletteSystem 创建后注册（UpdateColorPaletteAddress）

    global_addresses_table->Update(ga);
    global_addresses_table->Commit();

    return true;
}

void GlobalSSBOBufferRegistry::UpdateRenderItemAddresses(uint64_t addr_render_items, uint64_t addr_draw_item_ids)
{
    if (!global_addresses_table)
        return;

    GlobalAddresses *ga = global_addresses_table->Data();
    if (!ga)
        return;

    if (ga->addr_global_render_items != addr_render_items || ga->addr_draw_item_ids != addr_draw_item_ids)
    {
        ga->addr_global_render_items = addr_render_items;
        ga->addr_draw_item_ids = addr_draw_item_ids;
        global_addresses_table->Commit();
    }
}

void GlobalSSBOBufferRegistry::UpdateColorPaletteAddress(uint64_t addr_color_palette)
{
    if (!global_addresses_table)
        return;

    GlobalAddresses *ga = global_addresses_table->Data();
    if (!ga)
        return;

    if (ga->addr_color_palette != addr_color_palette)
    {
        ga->addr_color_palette = addr_color_palette;
        global_addresses_table->Commit();
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

    // CameraInfo 的行空间由「相机序号 × per-frame 帧槽」静态划分（GlobalSSBOBufferRegistry::CameraRow），
    // 行号不经 Acquire/Release ⇒ 整块预激活：CommitRow 的行校验恒成立，相机数据不会被静默拒绝写入。
    // 因此该池的 reserve_rows 必须是 0（预留会让行号从 1 起、与 CameraRow 错位）。
    if (auto *camera_pool = GetPool(GlobalSSBOType::CameraInfo))
    {
        if (!camera_pool->ActivateAllRows())
        {
            GLogError("[GlobalSSBOBufferRegistry] CameraInfo 行空间预激活失败");
            Release();
            return false;
        }
    }

    if (!InitializeGlobalAddressesTable())
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
