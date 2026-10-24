#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/CameraInfo.h>
#include <hgl/vk/buffer/DeviceBuffer.h>
#include <hgl/common/DebugChannel.h>
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
        // 与 MeshDrawParams **同 ID 索引**的平行表（一号双表）：容量必须与主表一致（InitializePools 有断言），
        // 行号唯一来源 = MeshDrawParams 池 ⇒ 本池从不 Acquire，只被静态写入。
        // reserve_rows 必须为 **0**：ActivateAllRows 的前提就是「调用时行号空间为空」，
        // 预留行会让首批 512 号相对 start 偏移导致 fail（实测：reserve=1 时只激活 513 行）。
        // 行 0 的「始终可见」哨兵由写入侧显式写（B3-ii），不靠预留。
        { GlobalSSBOType::GeometryAABB,        "GeometryAABB",        sizeof(mtl::GeometryAABB),  16384u, 0u },
        // B4-iii-a：材质数据**组件行**（组件在设备级的家）。与上面三个「内容行」池分工不同：
        // 内容行 = 被引用的资源行（shader 按 MTL_ROW 读）；本池 = 组件行（我引用哪一行内容 + 授权标记）。
        // 容量与材质内容池同口径 1024；reserve_rows = 1 ⇒ 行 0 恒为「无材质数据」哨兵
        // （组件侧 material_row_id == 0 即无）。
        { GlobalSSBOType::MaterialData,        "MaterialData",        sizeof(ssbo::MaterialDataRow), 1024u, 1u },
    };

    /// 配置表覆盖性（遍历全枚举，不手列清单）：聚合数组缺一项**不会编译报错**（值初始化），
    /// 会以 row_bytes=0 建池失败 —— 那是运行期才显形，所以在这里编译期先钉死。
    constexpr bool GlobalSSBOConfigTableValid() noexcept
    {
        for (uint32_t i = 0; i < GlobalSSBOTypeCount; ++i)
        {
            const GlobalSSBOConfig &config = kGlobalSSBOConfigs[i];

            if (config.name == nullptr
             || config.row_bytes == 0
             || config.default_capacity == 0
             || static_cast<uint32_t>(config.type) != i)   // 表序必须 = 枚举序（GetPool(type) 按下标直取）
                return false;
        }

        return true;
    }
    static_assert(GlobalSSBOConfigTableValid(),
        "kGlobalSSBOConfigs 必须逐项覆盖 GlobalSSBOType（表序 = 枚举序、row_bytes/capacity 非零）");
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
    global_addresses_global.addr_geometry_aabb         = GetGPUBase(GlobalSSBOType::GeometryAABB);
    global_addresses_global.addr_pbr_surface          = GetGPUBase(GlobalSSBOType::PBRSurface);
    global_addresses_global.addr_emissive_surface     = GetGPUBase(GlobalSSBOType::EmissiveSurface);
    global_addresses_global.addr_transmission_surface = GetGPUBase(GlobalSSBOType::TransmissionSurface);
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
    want.addr_viewport = global_addresses_slots[slot].viewport;

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

void GlobalSSBOBufferRegistry::UpdateColorPaletteAddress(uint64_t addr_color_palette)
{
    // 全局字段（构造期写入、长期有效）⇒ 整表所有槽一起更新。
    if (global_addresses_global.addr_color_palette == addr_color_palette)
        return;

    global_addresses_global.addr_color_palette = addr_color_palette;

    CommitAllSlots();
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

    // 相机行已下沉世界级（CameraInfoStorage）：本 registry 不再有 CameraInfo 行池
    //（WorldAddresses 表随世界各自创建，见 ECSContext）。

    // ── 一号双表（B3-i）：GeometryAABB 与 MeshDrawParams 同 ID 索引 ──
    // 同构是**结构保证**，不是两个池碰巧一致：AABB 池从不发号（Acquire），只把 [0, capacity)
    // 整块置 Active，行号唯一来源 = MeshDrawParams 池 ⇒ 用 geometry_id 取两表必然同一行。
    auto *mdp_pool  = GetPool(GlobalSSBOType::MeshDrawParams);
    auto *aabb_pool = GetPool(GlobalSSBOType::GeometryAABB);

    if (!mdp_pool || !aabb_pool
     || !aabb_pool->ActivateAllRows()
     || mdp_pool->GetRowCapacity() != aabb_pool->GetRowCapacity()
     || aabb_pool->GetActiveCount() != aabb_pool->GetRowCapacity())
    {
        // 容量/行空间不一致 = 同号取到别人家的 AABB（GPU 剔除静默错剔除，画面上什么都看不出来）⇒ fail-fast
        GLogError("[GlobalSSBOBufferRegistry] 一号双表不变量失败：MeshDrawParams rows=%u AABB rows=%u active=%u",
                  mdp_pool  ? mdp_pool->GetRowCapacity()  : 0u,
                  aabb_pool ? aabb_pool->GetRowCapacity() : 0u,
                  aabb_pool ? aabb_pool->GetActiveCount() : 0u);
        Release();
        return false;
    }

    // 行 0 哨兵：radius < 0 = 始终可见（缺 AABB 数据不得导致物体被静默剔除）
    mtl::GeometryAABB sentinel{};
    sentinel.center[3] = kAlwaysVisibleRadius;

    if (!WriteGeometryAABB(0u,sentinel))
    {
        GLogError("[GlobalSSBOBufferRegistry] GeometryAABB 行 0 哨兵写入失败（一号双表未就位）");
        Release();
        return false;
    }

    GLogInfo("[GlobalSSBOBufferRegistry] 一号双表就位：GeometryAABB 与 MeshDrawParams 同 ID 索引 rows=%u（AABB %u B/行）aabb_bda=0x%llx",
             aabb_pool->GetRowCapacity(),
             aabb_pool->GetRowBytes(),
             static_cast<unsigned long long>(aabb_pool->GetGPUBase()));

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

bool GlobalSSBOBufferRegistry::WriteGeometryAABB(const uint32_t id,const mtl::GeometryAABB &aabb)
{
    auto *pool = GetPool(GlobalSSBOType::GeometryAABB);

    if (!pool || !pool->IsReady() || id >= pool->GetRowCapacity() || !pool->IsActive(id))
    {
        GLogError("[GlobalSSBOBufferRegistry] WriteGeometryAABB 拒绝：id=%u capacity=%u active=%d"
                  "（一号双表要求行号来自 MeshDrawParams 池）",
                  id,
                  pool ? pool->GetRowCapacity() : 0u,
                  (pool && pool->IsActive(id)) ? 1 : 0);
        return false;
    }

    auto *row = static_cast<mtl::GeometryAABB *>(pool->RowCPU(id));
    if (!row)
        return false;

    *row = aabb;

    // 每次行写入留痕（几何创建期各一次，量级 = 场景几何数）：一号双表的**运行期证据**
    // 默认静默，ULRE_DEBUG.toml 的 channels.upload = true 打开
    if (ULRE_DEBUG_ON(Upload))
        GLogInfo("[GlobalSSBOBufferRegistry] GeometryAABB 行写入 id=%u radius=%.4f center=(%.3f,%.3f,%.3f) extents=(%.3f,%.3f,%.3f)",
                 id, aabb.center[3],
                 aabb.center[0], aabb.center[1], aabb.center[2],
                 aabb.extents[0], aabb.extents[1], aabb.extents[2]);

    return pool->CommitRow(id);
}

const mtl::MeshDrawParams *GlobalSSBOBufferRegistry::GetMeshDrawParamsRow(const uint32_t geometry_id) const
{
    // B4-ii：三个专用读口=泛型口薄包装（行 0 是「无」哨兵 ⇒ 专用口显式挡掉，泛型口只挡越界）
    return geometry_id == 0
        ? nullptr
        : GetTypedRow<mtl::MeshDrawParams>(GlobalSSBOType::MeshDrawParams, geometry_id);
}

const ssbo::MaterialDataRow *GlobalSSBOBufferRegistry::GetMaterialDataRow(const uint32_t row_id) const
{
    // 行 0 = 「无材质数据」哨兵
    return row_id == 0
        ? nullptr
        : GetTypedRow<ssbo::MaterialDataRow>(GlobalSSBOType::MaterialData, row_id);
}

const mtl::GeometryAABB *GlobalSSBOBufferRegistry::GetGeometryAABB(const uint32_t id) const
{
    // GeometryAABB 池行 0 是「始终可见」负半径哨兵 ⇒ 不挡 0
    return GetTypedRow<mtl::GeometryAABB>(GlobalSSBOType::GeometryAABB, id);
}

} // namespace hgl::graph
