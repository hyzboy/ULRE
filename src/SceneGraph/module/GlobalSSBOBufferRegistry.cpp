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
    // 表本体一份、**按帧槽切成 kGlobalAddressesSlotCount 份**：每帧只写自己那一槽
    // （在途帧共享同一块表 ⇒ 单份会被覆盖写踩掉）。
    global_addresses_table_buffer =
        bm->CreateSSBO("GlobalAddressesTable",
                       VkDeviceSize(kGlobalAddressesSlotStride) * kGlobalAddressesSlotCount);
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

    global_addresses_global = GlobalAddresses{};
    global_addresses_global.addr_mesh_draw_params_pool = GetGPUBase(GlobalSSBOType::MeshDrawParams);
    global_addresses_global.addr_pbr_surface          = GetGPUBase(GlobalSSBOType::PBRSurface);
    global_addresses_global.addr_emissive_surface     = GetGPUBase(GlobalSSBOType::EmissiveSurface);
    global_addresses_global.addr_transmission_surface = GetGPUBase(GlobalSSBOType::TransmissionSurface);
    global_addresses_global.addr_global_render_items  = 0;
    global_addresses_global.addr_draw_item_ids        = 0;
    global_addresses_global.addr_camera_info          = GetGPUBase(GlobalSSBOType::CameraInfo);
    global_addresses_global.addr_color_palette        = 0;   // 由 ColorPaletteSystem 注册（UpdateColorPaletteAddress）

    // 每帧槽字段（sky / viewport / shadow）在各自 buffer 就绪后由 Set*Address 填；先整表清 0。
    for (uint32_t slot = 0; slot < kGlobalAddressesSlotCount; ++slot)
        global_addresses_slots[slot] = GlobalAddressesSlot{};

    CommitAllSlots();

    return true;
}

uint64_t GlobalSSBOBufferRegistry::GetGlobalAddressesAddress(uint32_t frame_slot) const
{
    if (global_addresses_addr == 0)
        return 0;

    return global_addresses_addr
         + uint64_t(frame_slot % kGlobalAddressesSlotCount) * uint64_t(kGlobalAddressesSlotStride);
}

bool GlobalSSBOBufferRegistry::CommitSlot(uint32_t frame_slot)
{
    if (!global_addresses_table)
        return false;

    auto *base = reinterpret_cast<uint8_t *>(global_addresses_table->Data());
    if (!base)
        return false;

    const uint32_t slot = frame_slot % kGlobalAddressesSlotCount;

    // 槽步长是上界（可能大于 sizeof）⇒ 按字节偏移取槽，不能写 slots[slot]。
    auto *dst = reinterpret_cast<GlobalAddresses *>(base + size_t(slot) * kGlobalAddressesSlotStride);

    GlobalAddresses want = global_addresses_global;
    want.addr_sky      = global_addresses_slots[slot].sky;
    want.addr_viewport = global_addresses_slots[slot].viewport;
    want.addr_shadow   = global_addresses_slots[slot].shadow;

    if (memcmp(dst, &want, sizeof(GlobalAddresses)) == 0)
        return false;

    *dst = want;
    global_addresses_table->Commit();
    return true;
}

bool GlobalSSBOBufferRegistry::CommitAllSlots()
{
    bool any = false;

    for (uint32_t slot = 0; slot < kGlobalAddressesSlotCount; ++slot)
        any = CommitSlot(slot) || any;

    return any;
}

void GlobalSSBOBufferRegistry::UpdateRenderItemAddresses(uint64_t addr_render_items, uint64_t addr_draw_item_ids)
{
    // 这两个是全局字段（所有帧槽相同）⇒ 改动后整表所有槽一起更新。
    if (global_addresses_global.addr_global_render_items == addr_render_items
     && global_addresses_global.addr_draw_item_ids == addr_draw_item_ids)
        return;

    global_addresses_global.addr_global_render_items = addr_render_items;
    global_addresses_global.addr_draw_item_ids       = addr_draw_item_ids;

    CommitAllSlots();
}

void GlobalSSBOBufferRegistry::UpdateColorPaletteAddress(uint64_t addr_color_palette)
{
    // 全局字段（构造期写入、长期有效）⇒ 整表所有槽一起更新。
    if (global_addresses_global.addr_color_palette == addr_color_palette)
        return;

    global_addresses_global.addr_color_palette = addr_color_palette;

    CommitAllSlots();
}

void GlobalSSBOBufferRegistry::SetSkyAddress(uint32_t frame_slot, uint64_t addr)
{
    GlobalAddressesSlot &slot = global_addresses_slots[frame_slot % kGlobalAddressesSlotCount];

    if (slot.sky == addr)
        return;

    slot.sky = addr;
    CommitSlot(frame_slot);
}

void GlobalSSBOBufferRegistry::SetSkyAddress(uint64_t addr)
{
    for (uint32_t slot = 0; slot < kGlobalAddressesSlotCount; ++slot)
        SetSkyAddress(slot, addr);
}

void GlobalSSBOBufferRegistry::SetViewportAddress(uint64_t addr)
{
    for (uint32_t slot = 0; slot < kGlobalAddressesSlotCount; ++slot)
    {
        GlobalAddressesSlot &s = global_addresses_slots[slot];

        if (s.viewport == addr)
            continue;

        s.viewport = addr;
        CommitSlot(slot);
    }
}

void GlobalSSBOBufferRegistry::SetShadowAddress(uint32_t frame_slot, uint64_t addr)
{
    GlobalAddressesSlot &slot = global_addresses_slots[frame_slot % kGlobalAddressesSlotCount];

    if (slot.shadow == addr)
        return;

    slot.shadow = addr;
    CommitSlot(frame_slot);
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
