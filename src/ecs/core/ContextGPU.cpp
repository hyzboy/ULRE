#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/support/CameraInfoStorage.h>
#include <hgl/ecs/support/RenderItemDataStorage.h>
#include <hgl/ecs/support/DrawItemIDStorage.h>
#include <hgl/ecs/support/TransformAssignmentBuffer.h>
#include <hgl/ecs/systems/tick/TransformSystem.h>
#include <hgl/graph/render/RenderContext.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/module/EnvironmentManager.h>
#include <hgl/graph/ubo/WorldAddresses.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/VKRenderTarget.h>
#include <hgl/vk/buffer/StructView.h>
#include <hgl/vk/buffer/DeviceBuffer.h>
#include <hgl/log/Log.h>
#include <cstring>
#include <cstdio>
#include <vector>

namespace hgl::ecs
{
        namespace
        {
            /// 通用脏行上传辅助函数：收敛 EntityGPU / EntityRenderID / LightRow 的逐字同构上传循环
            template<typename Table, typename RowType, typename View, typename RowGetter>
            void UploadDirtyRowsHelper(Table &table, View *view, uint32_t capacity_per_copy, RowGetter &&getter)
            {
                if (!view || table.GetDirtyCount() == 0)
                    return;

                static thread_local std::vector<uint32_t> dirty_rows;
                table.CollectDirtyRows(dirty_rows);

                auto *base = reinterpret_cast<RowType *>(view->Data());
                if (!base)
                    return;

                constexpr uint32_t kCopies = uint32_t(graph::kWorldAddressesSlotCount);

                for (uint32_t copy = 0; copy < kCopies; ++copy)
                {
                    RowType *dst = base + size_t(copy) * capacity_per_copy;

                    for (const uint32_t index : dirty_rows)
                    {
                        const RowType *src = getter(table, index);
                        if (src)
                            dst[index] = *src;
                    }
                }

                view->Commit();
                table.ClearDirty();
            }

            /// 世界 GPU 行表建表的公共骨架：CreateSSBO → SetUpdateClass → StructView → BDA 校验。
            ///
            /// 5 张世界表（WorldAddresses / EntityGPU / EntityRenderID / LightRow / ShadowInfo）
            /// 此前把这段逐字复制了 5 遍。抽出来的**只有「建 + 校验」**：各表专属的部分仍留在各自的
            /// `Initialize*Table` 里 —— 容量算式、清零策略（按份 / 单份 / 特殊初值）与日志文案
            /// （每张表都要说清自己的消费者）。本函数**不 Commit**：调用方清完零再 Commit
            /// （与改造前顺序一致）。
            ///
            /// 用 `GLogError`（自由函数取不到成员 `Log`）；失败文案保持原样，`tag` 即原消息前缀。
            template<typename RowType>
            bool BuildWorldRowTable(const char *tag,const char *name_suffix,VkDeviceSize bytes,
                                    hgl::graph::GraphicsContext *gc,
                                    hgl::graph::VulkanDevice *gpu_device,
                                    const char *world_name,
                                    graph::BufferUpdateClass update_class,
                                    graph::DeviceBuffer *&out_buffer,
                                    graph::StructView<RowType> *&out_view,
                                    uint64_t &out_addr)
            {
                if (!gc)
                {
                    GLogError("[%s] graphics context is null",tag);
                    return false;
                }

                auto *bm = gc->GetBufferManager();
                if (!bm)
                {
                    GLogError("[%s] buffer manager is null",tag);
                    return false;
                }

                char name_buf[192];
                std::snprintf(name_buf,sizeof(name_buf),"World:%s:%s",world_name,name_suffix);

                out_buffer = bm->CreateSSBO(name_buf,bytes);
                if (!out_buffer)
                {
                    GLogError("[%s] CreateSSBO failed",tag);
                    return false;
                }

                out_buffer->SetUpdateClass(update_class);

                out_view = graph::StructView<RowType>::Create(out_buffer,false);
                if (!out_view)
                {
                    GLogError("[%s] StructView create failed",tag);
                    return false;
                }

                out_addr = gpu_device
                    ? gpu_device->GetBufferDeviceAddressAligned16(out_buffer->GetBuffer())
                    : 0;
                if (out_addr == 0)
                {
                    // 地址缺失 = shader 解引用 0 基址 = UB / 设备丢失（0 VUID 抓不到）⇒ fail-fast
                    GLogError("[%s] 取不到设备地址（16B 对齐 / SHADER_DEVICE_ADDRESS usage 检查）",tag);
                    return false;
                }

                return true;
            }
        }

        bool ECSContext::InitializeWorldAddressesTable()
        {
            if (world_addresses_table)
                return true;

            // BDA 要求：必须以 SHADER_DEVICE_ADDRESS usage 创建（CreateUBO 的额外 usage 位是 0）。
            // 表本体一份、**按帧槽切成 kWorldAddressesSlotCount 份**：每帧只写自己那一槽。
            if (!BuildWorldRowTable<graph::WorldAddresses>(
                    "ECSContext::InitializeWorldAddressesTable","AddressesTable",
                    VkDeviceSize(graph::kWorldAddressesSlotStride) * graph::kWorldAddressesSlotCount,
                    GetGraphicsContext(),gpu_device,GetName().c_str(),
                    graph::BufferUpdateClass::Default,
                    world_addresses_buffer,world_addresses_table,world_addresses_addr))
                return false;

            // 整表清零（每槽）：未发布的字段保持 0，由消费者自行判定"无数据"。
            if (auto *base = reinterpret_cast<uint8_t *>(world_addresses_table->Data()))
            {
                for (uint32_t slot = 0; slot < graph::kWorldAddressesSlotCount; ++slot)
                    *reinterpret_cast<graph::WorldAddresses *>(
                        base + size_t(slot) * graph::kWorldAddressesSlotStride) = graph::WorldAddresses{};
            }

            world_addresses_table->Commit();

            // T3c：实体行表与地址表**同批**建（同一个 BDA 前提；地址要写进本表 ⇒ 必须先有）
            if (!InitializeEntityGPUTable())
                return false;

            // T3d-3a：实体 4-ID 表与前两表同批（同一 BDA 前提、同一生命周期）
            if (!InitializeEntityRenderIDTable())
                return false;

            // T6b：光源行表与前几表同批（同一个 BDA 前提 + 同一个「世界 GPU 行数据」生命周期）
            if (!InitializeLightRowTable())
                return false;

            // T6b/P4：影子数据表与前几表同批（世界私有；多世界同帧各写各的）
            if (!InitializeShadowInfoTable())
                return false;

            SyncWorldAddresses();
            return true;
        }

        /// T3c-2：把实体行表里**脏了的那几行**写进设备镜像（不整表 512 KB 传）。
        void ECSContext::UploadEntityGPUDirtyRows(const uint32_t /*frame_slot*/)
        {
            if (!entity_gpu_buffer)
                return;

            UploadDirtyRowsHelper<EntityGPUTable, EntityGPU>(
                entity_gpu_table, entity_gpu_view, EntityGPUTable::kCapacity,
                [](const EntityGPUTable &t, uint32_t idx) { return t.Row(idx); });
        }

        /// T3d-3a：实体 4-ID 表脏行上传。
        void ECSContext::UploadEntityRenderIDDirtyRows()
        {
            if (!entity_render_id_buffer)
                return;

            UploadDirtyRowsHelper<EntityRenderIDTable, graph::RenderItemDescriptor>(
                entity_render_id_table, entity_render_id_view, EntityRenderIDTable::kRowStride,
                [](const EntityRenderIDTable &t, uint32_t idx) { return t.FlatRow(idx); });
        }

        /// T6b：光源行表脏行上传。
        void ECSContext::UploadLightRowDirtyRows()
        {
            if (!light_row_buffer)
                return;

            UploadDirtyRowsHelper<LightRowTable, graph::LightRow>(
                light_row_table, light_row_view, LightRowTable::kCapacity,
                [](const LightRowTable &t, uint32_t idx) { return t.Row(idx); });
        }

        /// T3d-3a：建实体 4-ID 表的设备镜像（与实体行表同骨架，共用 `BuildWorldRowTable`；
        /// 行 16 B ⇒ 8 份 × 4096 行 × 16 B = 512 KB）。地址缺失 = 消费者解引用 0 基址 ⇒ fail-fast。
        bool ECSContext::InitializeEntityRenderIDTable()
        {
            if (entity_render_id_buffer)
                return true;

            if (!BuildWorldRowTable<graph::RenderItemDescriptor>(
                    "ECSContext::InitializeEntityRenderIDTable","EntityRenderIDTable",
                    VkDeviceSize(graph::kWorldAddressesSlotCount)
                        * VkDeviceSize(EntityRenderIDTable::kRowStride)   // T4c3a：每槽 = 每 pass 一半
                        * VkDeviceSize(sizeof(graph::RenderItemDescriptor)),
                    GetGraphicsContext(),gpu_device,GetName().c_str(),
                    graph::BufferUpdateClass::Default,
                    entity_render_id_buffer,entity_render_id_view,entity_render_id_table_addr))
                return false;

            // 整表清零：未上行 = 全 0 ⇒ 消费者按 `geometry_id == 0` 判无（行 0 哨兵惯例）
            if (auto *rows = reinterpret_cast<graph::RenderItemDescriptor *>(
                    entity_render_id_view->Data()))
            {
                for (uint32_t i = 0; i < EntityRenderIDTable::kRowStride * graph::kWorldAddressesSlotCount; ++i)
                    rows[i] = graph::RenderItemDescriptor{};
            }

            entity_render_id_view->Commit();

            LogInfo("[ECSContext] 实体 4-ID 表就绪：%s  %u 行 × %zu B = %zu KB／份 × %u 份（帧槽）= %zu KB，"
                    "帧槽 0 基址=0x%llX（消费者：GPU 剔除 / CPU 参考对拍 / 形态 B gather；"
                    "每帧地址=基址+帧槽×每份字节，经 WorldAddresses.addr_entity_render_ids 下发）",
                    GetName().c_str(), EntityRenderIDTable::kRowStride,
                    sizeof(graph::RenderItemDescriptor),
                    size_t(EntityRenderIDTable::kRowStride) * sizeof(graph::RenderItemDescriptor) / 1024,
                    uint32_t(graph::kWorldAddressesSlotCount),
                    size_t(graph::kWorldAddressesSlotCount) * EntityRenderIDTable::kRowStride
                        * sizeof(graph::RenderItemDescriptor) / 1024,
                    (unsigned long long)entity_render_id_table_addr);

            return true;
        }

        bool ECSContext::InitializeEntityGPUTable()
        {
            if (entity_gpu_buffer)
                return true;

            // 分配 = kWorldAddressesSlotCount 份 × kCapacity 行（与 `UploadEntityGPUDirtyRows` 的
            // 逐份上传口径一致）：行内容按实体槽位索引，与 CPU 侧 `entity_gpu_table`（T3b）逐行对应。
            if (!BuildWorldRowTable<EntityGPU>(
                    "ECSContext::InitializeEntityGPUTable","EntityGPUTable",
                    VkDeviceSize(graph::kWorldAddressesSlotCount)
                        * VkDeviceSize(EntityGPUTable::kCapacity) * VkDeviceSize(sizeof(EntityGPU)),
                    GetGraphicsContext(),gpu_device,GetName().c_str(),
                    graph::BufferUpdateClass::Default,
                    entity_gpu_buffer,entity_gpu_view,entity_gpu_table_addr))
                return false;

            // 整表清零（**每一份**）：未激活的槽 `flags.Active == 0`，消费者据此跳过（T3b 的槽位语义）。
            // （改造前只清了第 0 份，其余 7 份留驱动初值——与"分配 8 份 + 逐份上传"的口径不一致。）
            if (auto *base = reinterpret_cast<EntityGPU *>(entity_gpu_view->Data()))
            {
                for (uint32_t copy = 0; copy < graph::kWorldAddressesSlotCount; ++copy)
                {
                    EntityGPU *rows = base + size_t(copy) * EntityGPUTable::kCapacity;

                    for (uint32_t i = 0; i < EntityGPUTable::kCapacity; ++i)
                        rows[i] = EntityGPU{};
                }
            }

            entity_gpu_view->Commit();

            LogInfo("[ECSContext] 实体行表就绪：%s  %u 行 × %zu B = %zu KB／份 × %u 份（帧槽）= %zu KB，"
                    "帧槽 0 基址=0x%llX（消费者：GPU 剔除读实体行；每帧地址=基址+帧槽×每份字节，"
                    "经 WorldAddresses.addr_entity_gpu 下发）",
                    GetName().c_str(), EntityGPUTable::kCapacity, sizeof(EntityGPU),
                    size_t(EntityGPUTable::kCapacity) * sizeof(EntityGPU) / 1024,
                    uint32_t(graph::kWorldAddressesSlotCount),
                    size_t(graph::kWorldAddressesSlotCount) * EntityGPUTable::kCapacity
                        * sizeof(EntityGPU) / 1024,
                    (unsigned long long)entity_gpu_table_addr);

            return true;
        }

        /// T6b：建光源行表的设备镜像（与实体两表逐字同构；16 行 × 64 B = 1 KB／份）。
        ///
        /// 行 0 = **主方向光**（`kMainLightRowIndex`；光源定义自 `SkyInfo` 迁出后的唯一真源）；
        /// 其余行为 0（`flags.enabled == 0` ⇒ 消费者判「未启用」跳过）—— 不预置未实现的光源。
        /// 按帧槽切份 + 脏行上传：与 `EntityGPUTable` 同判据（行内容随帧变就必须分份）。
        /// 地址缺失 = 读光行的 shader 解引用 0 基址（UB）⇒ fail-fast，与另两表同款判据。
        bool ECSContext::InitializeLightRowTable()
        {
            if (light_row_buffer)
                return true;

            light_row_table.Ensure();

            const VkDeviceSize one_copy = VkDeviceSize(LightRowTable::kCapacity)
                                        * VkDeviceSize(sizeof(graph::LightRow));

            if (!BuildWorldRowTable<graph::LightRow>(
                    "ECSContext::InitializeLightRowTable","LightRows",
                    one_copy * VkDeviceSize(graph::kWorldAddressesSlotCount),
                    GetGraphicsContext(),gpu_device,GetName().c_str(),
                    graph::BufferUpdateClass::Default,
                    light_row_buffer,light_row_view,light_row_table_addr))
                return false;

            // 整表按「行 0 = 默认主光 / 其余 0（未启用）」写满**每一份**：这些初值对所有帧槽一致
            // ⇒ 写完即与 CPU 权威一致，不需要再走脏行上传（ClearDirty）。
            if (auto *base = reinterpret_cast<graph::LightRow *>(light_row_view->Data()))
            {
                for (uint32_t copy = 0; copy < graph::kWorldAddressesSlotCount; ++copy)
                {
                    graph::LightRow *rows = base + size_t(copy) * LightRowTable::kCapacity;

                    for (uint32_t i = 0; i < LightRowTable::kCapacity; ++i)
                        rows[i] = (i == graph::kMainLightRowIndex)
                                ? graph::MakeDefaultMainLight()
                                : graph::LightRow{};
                }
            }

            light_row_view->Commit();
            light_row_table.ClearDirty();

            LogInfo("[ECSContext] 光源行表就绪：%s  %u 行 × %zu B = %zu B／份 × %u 份（帧槽），"
                    "帧槽 0 基址=0x%llX（行 0 = 主方向光；每帧地址=基址+帧槽×每份字节，"
                    "经 WorldAddresses.addr_lights 下发）",
                    GetName().c_str(), LightRowTable::kCapacity, sizeof(graph::LightRow),
                    size_t(one_copy),
                    uint32_t(graph::kWorldAddressesSlotCount),
                    (unsigned long long)light_row_table_addr);

            return true;
        }

        /// T6b/P4：建影子数据（`graph::ShadowInfo`）的**世界私有**设备镜像。
        ///
        /// 为什么随世界：它以前挂在 env profile（`EnvironmentInfo::shadow`），而 CSM 的 RT/控制器
        /// 挂世界 ⇒ **两个世界共享 `kEnvProfileDefault` 时会互相覆写同一份影子数据**（P4 修的就是
        /// 这个归属错位）。现在：CPU 权威 = `ECSContext::shadow_info`（写者 = `MainLightShadowSystem`），
        /// 设备镜像 = 本表，**按帧槽各一份**（地址 = 基址 + 帧槽 × `sizeof(ShadowInfo)`，与
        /// `WorldAddresses.addr_shadow` 的下发口径一致）。
        /// 地址缺失 = 采样阴影的 shader 解引用 0 基址（UB）⇒ fail-fast，与其余世界表同款判据。
        bool ECSContext::InitializeShadowInfoTable()
        {
            if (shadow_info_buffer)
                return true;

            const VkDeviceSize one_copy = VkDeviceSize(sizeof(graph::ShadowInfo));

            // 更新类别与旧 profile 侧同口径（staged 映射窗口：Update 写窗口、Commit 标脏交 L2）
            if (!BuildWorldRowTable<graph::ShadowInfo>(
                    "ECSContext::InitializeShadowInfoTable","ShadowInfo",
                    one_copy * VkDeviceSize(graph::kWorldAddressesSlotCount),
                    GetGraphicsContext(),gpu_device,GetName().c_str(),
                    graph::BufferUpdateClass::Deferred,
                    shadow_info_buffer,shadow_info_view,shadow_info_addr))
                return false;

            // 每份清零：未提交前 shader 读到「无阴影」的零值（cascade_count=0 / 句柄 0）
            if (auto *base = reinterpret_cast<graph::ShadowInfo *>(shadow_info_view->Data()))
            {
                for (uint32_t copy = 0; copy < graph::kWorldAddressesSlotCount; ++copy)
                    base[copy] = graph::ShadowInfo{};
            }

            shadow_info_view->Commit();

            LogInfo("[ECSContext] 影子数据表就绪：%s  %zu B × %u 份（帧槽）= %zu KB，帧槽 0 基址=0x%llX"
                    "（世界私有；写者 = MainLightShadowSystem 的 EditShadowInfo + 提交点 CommitShadowInfo；"
                    "每帧地址=基址+帧槽×每份字节，经 WorldAddresses.addr_shadow 下发）",
                    GetName().c_str(), size_t(one_copy),
                    uint32_t(graph::kWorldAddressesSlotCount),
                    size_t(graph::kWorldAddressesSlotCount) * size_t(one_copy) / 1024,
                    (unsigned long long)shadow_info_addr);

            return true;
        }

        /// T6b/P4：把世界 CPU 权威的影子数据写进**本帧槽**的设备副本。
        ///
        /// 调用点 = 交换链帧 acquire **之后**（`ViewUBOCommitSystem`）：此时本帧槽（该交换链图像）
        /// 的上一帧已在 NextFrame 等完，写它不会踩仍在飞的其它帧（它们读自己的槽）。
        /// 离屏 pass 不采样影子数据、且发生在 acquire 之前 ⇒ 那里不提交。
        void ECSContext::CommitShadowInfo(const uint32_t frame_slot)
        {
            if (!shadow_info_view)
                return;

            auto *base = reinterpret_cast<graph::ShadowInfo *>(shadow_info_view->Data());
            if (!base)
                return;

            const uint32_t slot = frame_slot % graph::kWorldAddressesSlotCount;

            base[slot] = shadow_info;
            shadow_info_view->Commit();
        }

        uint64_t ECSContext::GetWorldAddressesAddress(const uint32_t frame_slot) const
        {
            if (world_addresses_addr == 0)
                return 0;

            return world_addresses_addr
                 + uint64_t(frame_slot % graph::kWorldAddressesSlotCount)
                 * uint64_t(graph::kWorldAddressesSlotStride);
        }

        graph::EnvProfileID ECSContext::GetEnvProfileID() const
        {
            if (env_profile_explicit)
                return env_profile;

            // 未显式设置 ⇒ 跟随**本世界 RT**（作者侧 SetEnvironmentProfile 可能发生在世界创建之后，
            // 所以这里每帧解析而不是初始化时取一次）
            if (render_target)
                return render_target->GetEnvironmentProfile();

            return graph::kEnvProfileDefault;
        }

        void ECSContext::SetEnvProfileID(graph::EnvProfileID id, bool take_ownership)
        {
            // 换 profile 前先把**旧的、本世界拥有的**那个归还（否则换一次就漏一个 profile）
            if (env_profile_owned && env_profile != id)
                if (auto *manager = ResolveEnvManager())
                    manager->Release(env_profile);

            env_profile = id;
            env_profile_explicit = true;
            env_profile_owned = take_ownership;

            // 立刻重算本世界表的 sky / shadow 地址（不等到下一帧）：换 profile 的那一帧就生效
            SyncWorldAddresses();
        }

        graph::EnvProfileID ECSContext::CreateEnvProfile(const AnsiString &name,
                                                         const graph::EnvironmentInfo &init)
        {
            auto *manager = ResolveEnvManager();
            if (!manager)
                return graph::kEnvProfileInvalid;

            const graph::EnvProfileID id = manager->Create(name, init);
            if (id == graph::kEnvProfileInvalid)
                return id;

            SetEnvProfileID(id, true);      // 本世界创建的 ⇒ Shutdown 时归还
            return id;
        }

        graph::EnvironmentManager* ECSContext::ResolveEnvManager() const
        {
            graph::GraphicsContext *gc = nullptr;
            if (auto *rc = const_cast<ECSContext *>(this)->GetRenderContext())
                gc = rc->GetGraphicsContext();
            if (!gc)
                gc = const_cast<ECSContext *>(this)->GetGraphicsContext();

            return gc ? gc->GetEnvironmentManager() : nullptr;
        }

        bool ECSContext::SyncWorldAddresses()
        {
            if (!world_addresses_table)
                return false;

            auto *base = reinterpret_cast<uint8_t *>(world_addresses_table->Data());
            if (!base)
                return false;

            const uint32_t slot = frame_index % graph::kWorldAddressesSlotCount;

            // 槽步长是上界（可能大于 sizeof）⇒ 按字节偏移取槽。
            auto *dst = reinterpret_cast<graph::WorldAddresses *>(
                base + size_t(slot) * graph::kWorldAddressesSlotStride);

            graph::WorldAddresses want{};

            // ══ 地址列的统一口径（本函数是这张表的**唯一**写者）══════════════════════════════
            // 每一列只写"解出来的**有效**地址"；解不到有效地址（0）时**保留本槽上一份好值**
            //（`dst->addr_xxx` —— `dst` 指向本帧槽那一行 ⇒ "上一份好值"天然同槽，不会串到别槽）。
            // 为什么不写 0：0 基址会让读该段的 shader 解引用 0（UB / 设备丢失），比"沿用一份稍旧的
            // **有效**地址"危险得多。"从来没有过有效值"（世界刚建、`dst` 亦为 0）自然保持 0
            // = 消费者判**缺席**。
            // ⚠ 对**按帧槽切份**的列同样成立，且**不许把 `slot × 步长` 加到 0 基址上** —— 那会写出
            //   `slot×832` 这类"看着有效"的垃圾地址，消费者判不出缺席 ⇒ 解引用 UB（本批就是收这个口）。
            // 首个按此口径写的是 `addr_l2w`（B1b）；2026-10-07 起**全表九列统一**（契约见 Test 47 ⑦）。
            // ────────────────────────────────────────────────────────────────────────────

            // 解不到（0）⇒ 沿用本槽旧值；基址为 0 的按帧槽列**不参与** `slot × 步长` 运算。
            const auto pick = [](const uint64_t resolved, const uint64_t previous) -> uint64_t
            { return resolved != 0 ? resolved : previous; };
            const auto slot_offset = [slot](const uint64_t base, const uint64_t per_slot_bytes) -> uint64_t
            { return base != 0 ? (base + static_cast<uint64_t>(slot) * per_slot_bytes) : 0; };

            want.addr_camera_info = pick((camera_info_storage && camera_info_storage->IsReady())
                                         ? camera_info_storage->GetGPUBase() : 0,
                                         dst->addr_camera_info);

            want.addr_global_render_items = pick(GetRenderItemStorage()
                                                 ? GetRenderItemStorage()->GetGPUAddress() : 0,
                                                 dst->addr_global_render_items);

            want.addr_draw_item_ids = pick(GetDrawItemIDStorage()
                                           ? GetDrawItemIDStorage()->GetGPUAddress() : 0,
                                           dst->addr_draw_item_ids);

            // T3c：实体行表（世界私有；消费者 = GPU 剔除读实体行）。**按帧槽偏移**（T3c-3 实测：
            // 可动实体的 `row[Transform]` 随帧槽变，3 个行号轮转 ⇒ 单份表会被本帧写入覆盖上一帧
            // 正在被读的内容 = 跨帧别名）。份数 = 帧槽空间，与变换侧环深度**同源**（A5 已关闭：
            // `HGL_L2W_RING_FRAMES == HGL_FRAME_SLOT_TOTAL`；历史上的 3 曾致离屏槽与主帧槽别名
            //（4%3=1、5%3=2…）、prepass 覆写主帧在途矩阵 ⇒ 整帧只剩清屏色）。未建表 ⇒ 沿用旧值/缺席。
            want.addr_entity_gpu = pick(
                slot_offset(entity_gpu_table_addr,
                            static_cast<uint64_t>(EntityGPUTable::kCapacity) * sizeof(EntityGPU)),
                dst->addr_entity_gpu);

            // T3d-3a：实体 4-ID 表（世界私有；消费者 = GPU 剔除取池口径 geometry_id /
            // material / texture 行）。按帧槽偏移（与实体行表同纪律：可动实体 transform_id 随帧槽变）。
            want.addr_entity_render_ids = pick(
                slot_offset(entity_render_id_table_addr,
                            static_cast<uint64_t>(EntityRenderIDTable::kRowStride)   // T4c3a：每槽 = kRowStride 行（pass 分半）
                            * sizeof(graph::RenderItemDescriptor)),
                dst->addr_entity_render_ids);

            // T6b：光源行表（世界私有；行 0 = 主方向光）。按帧槽偏移——光参数可由作者逐帧改
            //（行内容随帧变），与实体行表同判据。
            want.addr_lights = pick(
                slot_offset(light_row_table_addr,
                            static_cast<uint64_t>(LightRowTable::kCapacity) * sizeof(graph::LightRow)),
                dst->addr_lights);

            // T3c-2b（B1）：L2W 行号由**变换侧**写入实体行表（生产者写自己的产物）；
            // 世界这里只负责把**脏行**上传设备镜像。

            // T3d：L2W 矩阵池基址入表（消费者 = GPU 剔除按 `row[Transform]` 行号直索引）。
            // **整块一个地址**（全帧槽同址）：行号本身已含帧槽段基址（B1b：静态 `1+slot` /
            // 动态 `环基址+slot`）⇒ 不需要像实体表那样按帧槽切份、也不需要帧槽偏移。
            // 取不到（变换缓冲未建 / 系统未注册）⇒ 保留上一份好值（统一口径的**首个**使用者，
            // 见函数开头的"地址列的统一口径"）。
            {
                uint64_t l2w_addr = 0;
                if (auto ts = GetSystem<TransformSystem>())
                {
                    if (auto *tb = ts->GetTransformBuffer())
                    {
                        if (auto *l2w_buf = tb->GetTransformDataBuffer())
                        {
                            if (auto *l2w_gpu = l2w_buf->GetGPUBuffer())
                            {
                                l2w_addr = gpu_device
                                    ? gpu_device->GetBufferDeviceAddressAligned16(
                                          l2w_gpu->GetVkDeviceBuffer())
                                    : 0;
                            }
                        }
                    }
                }
                want.addr_l2w = pick(l2w_addr, dst->addr_l2w);
            }

            UploadEntityGPUDirtyRows(slot);
            UploadEntityRenderIDDirtyRows();
            UploadLightRowDirtyRows();

            // env：**provider 槽参数块随本世界生效的 profile**（单份 buffer ⇒ 全帧槽同址；
            // T1：IBL 句柄自 sky 归位到它真正的消费者槽 `addr_ambient_env`；
            // 2026-10-07：`addr_sky` 随 sky 段整删退出本表），
            // **影子数据随世界**（T6b/P4 归属订正：以前挂 profile，两世界共享 default profile 时互踩）。
            // 影子表按帧槽各一份 ⇒ 地址 = 基址 + 帧槽 × sizeof(ShadowInfo)（写本帧槽的是提交点的
            // CommitShadowInfo）。两列同走函数开头的统一口径。
            // ⚠ 2026-10-07 订正（本批）：此处原来是"`if (addr != 0)` 才赋值"的**静默丢值**写法 ——
            //   `want{}` 从零起算 ⇒ 解不到就**真的把 0 写进了表**，与注释完全相反；而影子那列没有守卫，
            //   未建表时会把 `slot × 832` 写成"看着有效"的垃圾地址。两处都已按统一口径（0 ⇒ 沿用本槽旧值；
            //   基址 0 不参与 slot 偏移）改正。
            const graph::EnvProfileID active_env_profile = GetEnvProfileID();
            const uint64_t ambient_env_addr = ResolveEnvManager()
                                              ? ResolveEnvManager()->GetAmbientEnvAddress(active_env_profile)
                                              : 0;
            want.addr_ambient_env = pick(ambient_env_addr, dst->addr_ambient_env);

            want.addr_shadow = pick(
                slot_offset(shadow_info_addr, sizeof(graph::ShadowInfo)),
                dst->addr_shadow);

            const uint64_t old_shadow = dst->addr_shadow;
            const uint64_t old_ambient_env = dst->addr_ambient_env;

            if (std::memcmp(dst, &want, sizeof(graph::WorldAddresses)) == 0)
                return false;

            *dst = want;
            world_addresses_table->Commit();

            // env 地址变化时留一行痕（诊断"两世界同帧各自的 shadow/ambient_env 是否真的分开"）
            if (old_shadow != want.addr_shadow || old_ambient_env != want.addr_ambient_env)
                LogInfo("[ECSContext::SyncWorldAddresses] 世界 \"%s\" 地址入表：profile=%u "
                        "shadow[%u]=0x%llX ambient_env=0x%llX",
                        GetName().c_str(), active_env_profile, slot,
                        static_cast<unsigned long long>(want.addr_shadow),
                        static_cast<unsigned long long>(want.addr_ambient_env));

            return true;
        }
} // namespace hgl::ecs