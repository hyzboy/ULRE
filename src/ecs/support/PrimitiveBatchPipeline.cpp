#include<hgl/ecs/support/PrimitiveBatchPipeline.h>
#include<hgl/ecs/support/RenderItemDataStorage.h>
#include<hgl/ecs/support/RenderStrategyParity.h>
#include<hgl/ecs/support/DrawItemIDStorage.h>
#include<hgl/ecs/support/DrawItemCompaction.h>
#include<hgl/ecs/support/BoundingVolumeCull.h>
#include<source_location>
#include<cmath>
#include<cstdlib>
#include<cstdio>
#include<cstring>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/support/BoundingBoxAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/ShadowProxy.h>
#include<hgl/ecs/support/PrimitiveState.h>
#include<hgl/ecs/support/GPUCullSystem.h>
#include<hgl/ecs/core/PrimitiveRenderItem.h>
#include<hgl/ecs/core/InstancedPrimitiveRenderItem.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/graph/module/SSBOBufferRegistry.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/vk/VertexDataManager.h>

#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/object/ObjectTracker.h>
#include<hgl/util/hash/FNV1a.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKObjectNameBuilder.h>
#include<hgl/vk/buffer/IndirectCommandBuffer.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKRenderPass.h>
#include<hgl/log/Log.h>
#include<algorithm>
#include<chrono>
#include<cstdint>
#include<string>
#include<unordered_map>

namespace hgl::ecs
{
    namespace
    {
        // 材质来源判据已收敛为实体级自由函数 HasAnyMaterialSource(Entity*)（support/PrimitiveState.h）。

        bool BatchRequiresIndirectCommands(const MaterialBatch &batch)
        {
            // BDA 后顶点数据经行内地址寻址——所有 DrawBatch（含私有 VBO）都进
            // 同一次 vkCmdDrawMeshTasksIndirectEXT multi-draw，命令序 = 行序 1:1。
            // 凡含几何项的 batch 都需要 ICB（mesh 为唯一顶点路径，恒走间接）。
            for (auto *item : batch.items)
            {
                // B3-iii：判据从"有每实体 CPU 几何副本"改为"行键有几何"（等价：副本只在有几何时存在）
                if (item && item->draw_key.HasGeometry())
                    return true;
            }

            return false;
        }

        void ReleaseICB(MaterialBatch &batch)
        {
            if (batch.icb_mesh_tasks && batch.own_icb_mesh_tasks)
            {
                delete batch.icb_mesh_tasks;
                batch.icb_mesh_tasks = nullptr;
            }
        }

        // 材质是否含 mesh stage（ShaderGen 材质恒是——mesh 为唯一顶点路径）
        bool ProgramHasMeshStage(const graph::ShaderProgram *program)
        {
            if (!program)
                return false;

            for (const auto &stage : program->GetStageList())
            {
                if (stage.stage == VK_SHADER_STAGE_MESH_BIT_EXT)
                    return true;
            }

            return false;
        }

        // A6-2b/b3：BDA 后无 per-material SSBO 绑定差异（材质数据/顶点流全走行内
        // 地址与 pc_root），合批 key 只按 shader+pipeline 区分——同 shader 即同批。
    }

    bool PrimitiveBatchPipeline::PrepareFrame(ECSContext* ctx)
    {
        if (!ctx)
            return false;

        world = ctx;
        auto& cache = world->GetRenderFrameCache();
        if (cache.renderItems.empty())
            return false;

        camera_info = cache.cameraInfo;
        if (!camera_info)
            return false;

        if (!device)
            device = world->GetGPUDevice();

        const uint32_t frame_index = world->GetFrameIndex();
        if (prepared_frame_index != frame_index)
            prepared_frame_index = frame_index;

        last_compaction_stats = CompactionStats{};
        if (world)
        {
            if (auto *id_storage = world->GetDrawItemIDStorage())
                id_storage->Reset();
        }

        return true;
    }

    void PrimitiveBatchPipeline::RunCulling()
    {
        // T3f-2（形态 B，判据 2）：上一帧 guard 判定接管 ⇒ 本帧**跳过 CPU 剔除**。
        // 时序口径：guard 在 `FinalizeBatches()`（RenderBatch，批建成*之后*）评估，
        // 批成员资格在 `BuildMaterialBatches` 按 isVisible 过滤 ⇒ "跳不跳"只能取
        // **上一帧**的结果；稳态下批 = 全项（新 item 默认 isVisible=true），
        // 幸存数 ≤ 批容量 ⇒ DrawBatch 记账的 maxDrawCount 是**真上界**，不会截绘制。
        // 交接帧（上帧接管、本帧前提破了）：批全项但不接管 = 一帧过画（屏内观感
        // 无损、guard 已报错），下一帧 cull 恢复。shadow pass 同样跳（caster 全进
        // 阴影图 = 多画不出错，首版口径，见设计稿 §8.3）。
        if (world && IsGPUCullOverrideActive())
            return;

        PerformFrustumCulling();
    }

    void PrimitiveBatchPipeline::RunSorting()
    {
        SortByDistance();
    }

    void PrimitiveBatchPipeline::RunTransformIndexing()
    {
        TransformSystem* transform_system = nullptr;
        if (world)
        {
            if (auto system = world->GetSystem<TransformSystem>())
            {
                transform_system = system.get();
                transform_system->SetDevice(device);
                transform_system->EnsureTransformBuffer();
                transform_system->RefreshHandleOrder();
            }
        }

        AssignTransformIndices(transform_system);
    }

    void PrimitiveBatchPipeline::RunBatching()
    {
        BuildMaterialBatches();
        FinalizeBatches();
    }

    void PrimitiveBatchPipeline::PerformFrustumCulling()
    {
        if (!world || !camera_info)
            return;

        auto& cache = world->GetRenderFrameCache();

        frustum.SetMatrix(camera_info->vp);

        for (auto& itemPtr : cache.renderItems)
        {
            RenderItem* item = itemPtr.get();
            if (!item || !item->isVisible)
                continue;

            auto entity = item->GetEntity();
            if (!entity)
                continue;

            const BoundingBoxAccessor bbox = world->GetBoundingBoxByEntity(entity->GetEntityID());

            if (bbox.IsValid())
            {
                if (bbox.HasWorldBounds())
                    item->isVisible = TestFrustumWithWorldAABB(item, bbox.GetWorldBounds());
                else
                    item->isVisible = TestFrustumWithLocalAABB(item, bbox.GetCenter(), bbox.GetExtents());
            }
            else
            {
                item->isVisible = TestFrustumWithBoundingSphere(item);
            }
        }
    }

    bool PrimitiveBatchPipeline::TestFrustumWithWorldAABB(RenderItem* item, const math::AABB& world_aabb)
    {
        //worldAABB 已包含实体缩放，可直接用 P/N 顶点法做精确判定
        return frustum.BoxIn(world_aabb) != math::Frustum::Scope::OUTSIDE;
    }

    bool PrimitiveBatchPipeline::TestFrustumWithLocalAABB(RenderItem* item, const glm::vec3& local_center, const glm::vec3& local_extents)
    {
        const glm::mat4 worldMat = item->GetWorldMatrix();
        const glm::vec3 world_center = glm::vec3(worldMat * glm::vec4(local_center, 1.0f));

        //本地尺寸必须乘以世界缩放，否则放大的实体会被误剔除
        const float radius = ToWorldBoundingRadius(glm::length(local_extents), worldMat);

        return frustum.SphereIn(world_center, radius) != math::Frustum::Scope::OUTSIDE;
    }

    bool PrimitiveBatchPipeline::TestFrustumWithBoundingSphere(RenderItem* item)
    {
        auto* prim_item = dynamic_cast<PrimitiveRenderItem*>(item);
        if (!prim_item)
            return true;  // No bounding sphere data for this item type — keep visible
        Entity *owner = item->GetEntity();
        if (!owner)
            return false;

        auto transform = item->GetTransform();
        if (!transform.IsValid())
            return false;

        // A5a：包围球半径随几何状态迁入 GeometryData 组件（实体级查询，不在组件内缓存）
        const GeometryData *geometry = nullptr;
        if (ECSContext *context = owner->GetContext())
            geometry = context->GetGeometryData(owner);

        const float boundingRadius = geometry ? geometry->GetBoundingRadius() : 0.0f;

        if (boundingRadius <= 0.0f)
            return true;

        const glm::vec3 worldPos = transform.GetWorldPosition();

        //GetBoundingRadius() 由几何体本地 AABB 求得，不含实体缩放，这里补上世界缩放
        const float worldRadius = ToWorldBoundingRadius(boundingRadius, item->GetWorldMatrix());

        return frustum.SphereIn(worldPos, worldRadius) != math::Frustum::Scope::OUTSIDE;
    }

    void PrimitiveBatchPipeline::SortByDistance()
    {
        if (!world)
            return;

        auto& cache = world->GetRenderFrameCache();

        std::sort(cache.renderItems.begin(), cache.renderItems.end(),
            [](const std::unique_ptr<RenderItem>& a, const std::unique_ptr<RenderItem>& b) {
                return a->distanceToCamera < b->distanceToCamera;
            });
    }

    void PrimitiveBatchPipeline::AssignTransformIndices(TransformSystem* transform_system)
    {
        if (!world)
            return;

        auto& cache = world->GetRenderFrameCache();
        uint32_t dynamic_base = 0;

        if (transform_system)
            dynamic_base = transform_system->GetDynamicBaseIndex();   // B1b：预算决定，与名单长度无关

        for (auto& itemPtr : cache.renderItems)
        {
            RenderItem* item = itemPtr.get();
            if (!item)
                continue;

            auto transform = item->GetTransform();
            if (!transform.IsValid())
                continue;

            const auto handle = transform.GetID();
            if (handle == TransformDataStorage::INVALID_HANDLE)
            {
                item->transform_index = 0;
                continue;
            }

            uint32_t slot = 0;
            if (transform_system &&
                transform_system->TryGetTransformSlot(handle, transform.IsMovable(), slot))
            {
                const PositionSourceSpec position_source_spec = item->GetPositionSourceSpec();
                const TransformPolicySpec transform_policy_spec = item->GetTransformPolicySpec();

                switch (position_source_spec)
                {
                case PositionSourceSpec::MeshVertex:
                case PositionSourceSpec::Quad2DGenerated:
                case PositionSourceSpec::TerrainHeightmapGrid:
                case PositionSourceSpec::ProceduralGenerated:
                default:
                    switch (transform_policy_spec.facing)
                    {
                    case FacingPolicy::None:
                    case FacingPolicy::CameraFacing:
                    case FacingPolicy::AxisLocked:
                    default:
                        // Fixed screen-space scaling is a separate policy switch from facing.
                        if (transform_policy_spec.fixedScreenSpaceScale)
                        {
                            // R08 ingress-only stage: keep transform index behavior unchanged.
                        }
                        // B1b：L2W 行号 = 静态 `1 + slot` / 动态 `预算基址 + slot`（slot 跨帧稳定）
                        item->transform_index = transform.IsMovable() ? (dynamic_base + slot) : (slot + 1);
                        break;
                    }
                    break;
                }
            }
            else
            {
                item->transform_index = 0;
            }
        }
    }

    graph::BufferManager* PrimitiveBatchPipeline::GetBufferManager() const
    {
        // W7：GetGraphicsContext() 已内置 render_ctx fallback
        auto *gc = world ? world->GetGraphicsContext() : nullptr;
        return gc ? gc->GetBufferManager() : nullptr;
    }

    graph::ObjectNameBuilder
    PrimitiveBatchPipeline::BuildICBNames() const
    {
        graph::ObjectNameBuilder draw_name;

        if (world && !world->GetResourceNamePrefix().empty())
        {
            std::string draw_str = world->GetResourceNamePrefix() + ":IndirectMeshTaskBuffer";

            draw_name = graph::ObjectNameBuilder(draw_str.c_str());
        }
        else
        {
            draw_name = graph::ObjectNameBuilder("RPBS_IndirectMeshTaskBuffer");
        }

        return draw_name;
    }

    void PrimitiveBatchPipeline::ReallocICB(MaterialBatch& batch)
    {
        HGL_CAPTURE_SCOPE();
        if (!device || batch.items.empty() || batch.gpu_driven_override)
            return;

        uint32_t icb_new_count = 1;
        while (icb_new_count < batch.items.size())
            icb_new_count <<= 1;

        if (batch.icb_mesh_tasks && icb_new_count <= batch.icb_mesh_tasks->GetMaxCount())
            return;

        if (batch.icb_mesh_tasks)
        {
            delete batch.icb_mesh_tasks;
            batch.icb_mesh_tasks = nullptr;
        }

        auto draw_name = BuildICBNames();

        batch.icb_mesh_tasks = device->CreateIndirectMeshTaskBuffer(icb_new_count, draw_name);
    }

    void PrimitiveBatchPipeline::BuildBatches(MaterialBatch& batch, const uint32_t base_instance)
    {
        const size_t count = batch.items.size();
        if (count == 0)
        {
            batch.draw_batches_count = 0;
            batch.draw_batches.clear();
            return;
        }

        if (!device)
        {
            batch.draw_batches_count = 0;
            batch.draw_batches.clear();
            return;
        }

        const bool needs_indirect = BatchRequiresIndirectCommands(batch);
        if (needs_indirect)
        {
            ReallocICB(batch);

            if (!batch.icb_mesh_tasks)
            {
                batch.draw_batches_count = 0;
                batch.draw_batches.clear();
                return;
            }
        }
        else
        {
            ReleaseICB(batch);
        }

        batch.draw_batches.clear();
        batch.draw_batches.reserve(count);

        auto *gc = world ? world->GetGraphicsContext() : nullptr;
        auto *pool = gc ? gc->GetGlobalSSBOBufferRegistry() : nullptr;
        auto *dev = world ? world->GetGPUDevice() : nullptr;
        auto *id_storage = world ? world->GetDrawItemIDStorage() : nullptr;

        size_t i = 0;
        while (i < count)
        {
            RenderItem *first_item = batch.items[i];
            const RenderItem::MeshDrawKey draw_key = first_item
                ? first_item->draw_key
                : RenderItem::MeshDrawKey{};

            // B3-iii：无行键 = 无几何（旧判据是"两份 CPU 副本都在"）⇒ 不进批
            if (!draw_key.HasGeometry())
            {
                ++i;
                continue;
            }

            const graph::Geometry *geometry = nullptr;
            if (auto *prim_item = dynamic_cast<PrimitiveRenderItem *>(first_item))
            {
                const GeometryData *geometry_comp = nullptr;
                if (Entity *owner = first_item->GetEntity())
                {
                    if (ECSContext *context = owner->GetContext())
                        geometry_comp = context->GetGeometryData(owner);
                }
                if (geometry_comp && geometry_comp->GetPrimitiveAsset())
                    geometry = geometry_comp->GetPrimitiveAsset()->GetGeometry();
            }

            // 同簇判据 = **行键相等**（B3-iii：旧实现按两份每实体 CPU 副本逐字段比较，
            // 含 VAB 列表与 ibo；同 asset ⇒ 同一行 ⇒ 必然同键，判定等价且不再需要那两份副本）。
            //
            // ⚠ GPU-Driven 批次例外（**必须每项自成簇**）：这类批次的绘制指令、L2W 索引槽、
            // mesh 命令行都由 GPU/示例侧的表逐项给出（第 i 项 ↔ 第 i 行/第 i 槽一一对应），
            // 合并成多实例簇会让整簇共用一个首实例 ⇒ 屏幕上只剩 1 个实例的变换
            // （2026-10-02 实测：ComputeFrustumCull 64 个立方体只显示 1 个）。
            size_t cluster_end = i;
            while (cluster_end + 1 < count)
            {
                RenderItem *next_item = batch.items[cluster_end + 1];

                if (batch.gpu_driven_override)
                    break;

                if (!next_item || !(next_item->draw_key == draw_key))
                    break;

                ++cluster_end;
            }

            const size_t cluster_size = cluster_end - i + 1;

            bool has_valid_4id = true;
            for (size_t k = i; k <= cluster_end; ++k)
            {
                RenderItem *item_k = batch.items[k];
                if (!item_k || item_k->GetRenderItemHandle() == INVALID_RENDER_ITEM_HANDLE)
                {
                    has_valid_4id = false;
                    break;
                }
            }

            if (has_valid_4id && !batch.gpu_driven_override)
            {
                if (cluster_size == 1 && batch.uses_render_item_resolve)
                {
                    if (auto *inst = dynamic_cast<InstancedPrimitiveRenderItem *>(batch.items[i]))
                    {
                        const uint32_t inst_count = inst->GetInstanceCount();
                        if (inst_count > 1)
                        {
                            CompactedDrawRange range{};
                            range.first_instance = inst->GetRenderItemHandle();
                            range.instance_count = inst_count;
                            range.is_direct = true;

                            DrawBatch db{};
                            db.first_instance = range.first_instance;
                            db.instance_count = range.instance_count;
                            db.Set(draw_key, geometry);
                            batch.draw_batches.push_back(db);

                            last_compaction_stats.total_input_items     += inst_count;
                            last_compaction_stats.direct_ranges         += 1;
                            last_compaction_stats.direct_items          += inst_count;
                            last_compaction_stats.bytes_saved_over_full += inst_count * 12;

                            i = cluster_end + 1;
                            continue;
                        }
                    }
                }

                hgl::ValueArray<RenderItemHandle> cluster_handles;
                for (size_t k = i; k <= cluster_end; ++k)
                {
                    if (auto *inst = dynamic_cast<InstancedPrimitiveRenderItem *>(batch.items[k]))
                    {
                        const uint32_t inst_count = inst->GetInstanceCount();
                        const RenderItemHandle base_h = inst->GetRenderItemHandle();
                        if (inst_count > 1)
                        {
                            for (uint32_t j = 0; j < inst_count; ++j)
                                cluster_handles.Add(base_h + j);
                        }
                        else
                        {
                            cluster_handles.Add(base_h);
                        }
                    }
                    else
                    {
                        cluster_handles.Add(batch.items[k]->GetRenderItemHandle());
                    }
                }

                CompactionStats stats{};
                hgl::ValueArray<CompactedDrawRange> compacted_ranges;
                CompactRenderItemHandles(
                    cluster_handles.GetData(),
                    static_cast<uint32_t>(cluster_handles.GetCount()),
                    id_storage,
                    compacted_ranges,
                    &stats);

                last_compaction_stats.total_input_items     += stats.total_input_items;
                last_compaction_stats.direct_ranges         += stats.direct_ranges;
                last_compaction_stats.direct_items          += stats.direct_items;
                last_compaction_stats.indexed_ranges        += stats.indexed_ranges;
                last_compaction_stats.indexed_items         += stats.indexed_items;
                last_compaction_stats.bytes_saved_over_full += stats.bytes_saved_over_full;

                if (batch.uses_render_item_resolve)
                {
                    for (int r = 0; r < compacted_ranges.GetCount(); ++r)
                    {
                        const auto &range = compacted_ranges[r];
                        DrawBatch db{};
                        db.first_instance = range.first_instance;
                        db.instance_count = range.instance_count;
                        db.Set(draw_key, geometry);
                        batch.draw_batches.push_back(db);
                    }
                }
                else
                {
                    DrawBatch db{};
                    db.first_instance = base_instance + static_cast<uint32_t>(i);
                    db.instance_count = 0;
                    for (size_t k = i; k <= cluster_end; ++k)
                    {
                        if (auto *inst = dynamic_cast<InstancedPrimitiveRenderItem *>(batch.items[k]))
                            db.instance_count += inst->GetInstanceCount();
                        else
                            ++db.instance_count;
                    }
                    if (db.instance_count == 0)
                        db.instance_count = 1;
                    db.Set(draw_key, geometry);
                    batch.draw_batches.push_back(db);
                }
            }
            else
            {
                DrawBatch db{};
                db.first_instance = base_instance + static_cast<uint32_t>(i);
                db.instance_count = 0;
                for (size_t k = i; k <= cluster_end; ++k)
                {
                    if (auto *inst = dynamic_cast<InstancedPrimitiveRenderItem *>(batch.items[k]))
                        db.instance_count += inst->GetInstanceCount();
                    else
                        ++db.instance_count;
                }
                if (db.instance_count == 0)
                    db.instance_count = 1;
                db.Set(draw_key, geometry);
                batch.draw_batches.push_back(db);
            }

            i = cluster_end + 1;
        }

        batch.draw_batches_count = static_cast<uint32_t>(batch.draw_batches.size());

        // ── IndirectMeshDraw：mesh 命令 + per-draw 参数行（后置统一写）──
        WriteMeshDrawCommands(batch);
    }

    void PrimitiveBatchPipeline::EnsureMeshDrawParams(MaterialBatch& batch)
    {
        if (batch.gpu_driven_override)
            return;

        if (!batch.key.shader_program
         || !ProgramHasMeshStage(batch.key.shader_program))
            return;

        if (!batch.buffer_manager || batch.items.empty())
            return;

        // 参数行数上限 = DrawBatch 数 ≤ item 数（实例合并只减不增）——与行表同策略 pow2 扩容
        const uint32_t item_count = static_cast<uint32_t>(batch.items.size());
        uint32_t new_capacity = 1;
        while (new_capacity < item_count)
            new_capacity <<= 1;

        if (batch.mesh_draw_params_buffer
         && batch.mesh_draw_params_capacity >= item_count)
            return;

        batch.mesh_draw_params_capacity = new_capacity;

        if (batch.mesh_draw_params_buffer)
        {
            if (batch.buffer_manager)
                batch.buffer_manager->Release(batch.mesh_draw_params_buffer);
            else
                delete batch.mesh_draw_params_buffer;
            batch.mesh_draw_params_buffer = nullptr;
        }

        const VkDeviceSize byte_size =
            static_cast<VkDeviceSize>(batch.mesh_draw_params_capacity)
            * sizeof(graph::mtl::MeshDrawCommand);
        batch.mesh_draw_params_buffer = batch.buffer_manager->CreateSSBO(
            "ECS:Batch:MeshDrawCommands", byte_size, nullptr, graph::SharingMode::Exclusive);

        // 新建 SSBO 内容未定义——整表清零。
        if (batch.mesh_draw_params_buffer)
        {
            if (auto *gpu_zero = batch.mesh_draw_params_buffer->GetGPUBuffer())
            {
                if (auto *zero_ptr = gpu_zero->Map(0, byte_size))
                {
                    std::memset(zero_ptr, 0, static_cast<size_t>(byte_size));
                    gpu_zero->Unmap();
                }
            }
        }
    }

    void PrimitiveBatchPipeline::WriteMeshDrawCommands(MaterialBatch& batch)
    {
        if (batch.gpu_driven_override)
            return;

        const uint32_t count = batch.draw_batches_count;
        if (count == 0 || !batch.key.shader_program)
            return;

        const bool is_lines =
            batch.key.shader_program->GetPrimitiveType() == graph::PrimitiveType::Lines;

        // 命令参数行：每个 DrawBatch 一行（MeshDrawCommand: geometry_id + first_instance）
        if (batch.mesh_draw_params_buffer)
        {
            auto *params_gpu = batch.mesh_draw_params_buffer->GetGPUBuffer();
            auto *cmds = params_gpu
                ? static_cast<graph::mtl::MeshDrawCommand *>(params_gpu->Map(
                      0, static_cast<VkDeviceSize>(count) * sizeof(graph::mtl::MeshDrawCommand)))
                : nullptr;

            if (cmds)
            {
                for (uint32_t i = 0; i < count; ++i)
                {
                    DrawBatch &db = batch.draw_batches[i];

                    // B3-iii：几何行号直接来自批的行键（旧实现三级回退 + 往每实体副本里回写镜像）
                    cmds[i].geometry_id = db.draw_key.geometry_id;
                    cmds[i].first_instance = db.first_instance;
                }

                params_gpu->Unmap();
            }
        }

        // mesh 命令：仅 VDM DrawBatch（与渲染器累积逻辑一致）。
        // run（同 GeometryDataBuffer 连续段）内命令序与行序 1:1——间接 flush 按 run
        // 首行 offset 绑定参数表，shader 内 rows[gl_DrawID]（每次 indirect 调用 0 起
        // 编号）恰好对齐；Z 恒 1。
        if (batch.icb_mesh_tasks)
        {
            auto *mesh_cmd = batch.icb_mesh_tasks->MapCmd();

            if (mesh_cmd)
            {
                for (uint32_t i = 0; i < count; ++i)
                {
                    const DrawBatch &db = batch.draw_batches[i];
                    // BDA：全部 DrawBatch（含私有 VBO，地址经 db.geometry 直取填行）都写
                    // mesh 间接命令——命令序 = DrawBatch 序 = 参数行序 1:1
                    if (!db.draw_key.HasGeometry())
                        continue;


                    // B3-iii：绘制元素数取自行键（= 行里 is_indexed ? index_count : vertex_count，逐位等价）
                    const uint32_t total_vertices = db.draw_key.DrawElementCount();

                    if (db.geometry && db.geometry->HasMeshlets())
                    {
                        mesh_cmd->groupCountX = db.geometry->GetMeshletCount();
                    }
                    else
                    {
                        mesh_cmd->groupCountX = CalcMeshGroupCount(is_lines, total_vertices);
                    }
                    mesh_cmd->groupCountY = db.instance_count > 1
                        ? db.instance_count
                        : 1u;
                    mesh_cmd->groupCountZ = 1u;
                    ++mesh_cmd;
                }

                batch.icb_mesh_tasks->Unmap();
            }
        }
    }

    void PrimitiveBatchPipeline::FinalizeBatch(MaterialBatch& batch)
    {
        // 本批次的**世界表**（WorldAddresses）本帧槽地址：相机行表 / 渲染项表 / DrawItemID 表
        // 都是世界私有地址、都在表内 —— 多世界渲染只换这一个指针。表地址随世界、随帧槽 ⇒ 每批重取。
        batch.world_addresses_address = world
            ? world->GetWorldAddressesAddress(world->GetFrameIndex()) : 0;

        for (auto *item : batch.items)
        {
            if (auto *inst_item = dynamic_cast<InstancedPrimitiveRenderItem *>(item))
            {
                if (inst_item->IsGPUDriven())
                {
                    batch.gpu_driven_override = true;
                    batch.own_icb_mesh_tasks = false;
                    batch.own_mesh_draw_params = false;
                    batch.own_l2w_index = false;
                    batch.own_material_data_rows = false;

                    if (inst_item->GetL2WBuffer())
                        batch.l2w_buffer = inst_item->GetL2WBuffer();
                    if (inst_item->GetL2WIndexBuffer())
                        batch.l2w_index_buffer = inst_item->GetL2WIndexBuffer();
                    if (inst_item->GetMeshDrawParamsBuffer())
                        batch.mesh_draw_params_buffer = inst_item->GetMeshDrawParamsBuffer();
                    if (inst_item->GetMaterialDataRowsBuffer())
                        batch.material_data_index_rows_buffer = inst_item->GetMaterialDataRowsBuffer();
                    if (inst_item->GetIndirectMeshTaskBuffer())
                        batch.icb_mesh_tasks = inst_item->GetIndirectMeshTaskBuffer();
                    if (inst_item->GetIndirectCountBuffer())
                    {
                        batch.icb_count_buffer = inst_item->GetIndirectCountBuffer();
                        batch.icb_count_buffer_offset = inst_item->GetIndirectCountOffset();
                    }
                }
            }

            if (auto *prim_item = dynamic_cast<PrimitiveRenderItem *>(item))
            {
                // A4：材质运行期绑定在世界的**共享行**上（RenderItem 只持行号）。
                if (const MaterialRuntimeRow *mat_row = prim_item->GetMaterialRuntimeRow())
                {
                    if (mat_row->material_texture_zero_row_gpu)
                        batch.texture_reference_base_addr = mat_row->material_texture_zero_row_gpu;
                    else if (mat_row->material_texture_row_gpu)
                        batch.texture_reference_base_addr =
                            mat_row->material_texture_row_gpu -
                            uint64_t(mat_row->material_texture_configuration.row_index) *
                            uint64_t(mat_row->material_texture_configuration.row_stride);
                }
            }
        }

        SortBatchItems(batch);
        EnsureBatchIndexRows(batch);
        EnsureMeshDrawParams(batch);
        BuildBatches(batch, 0);
        WriteBatchIndexRows(batch);
    }

    void PrimitiveBatchPipeline::SortBatchItems(MaterialBatch& batch)
    {
        if (batch.gpu_driven_override)
            return;

        std::vector<RenderItem*> static_items;
        std::vector<RenderItem*> movable_items;
        static_items.reserve(batch.items.size());
        movable_items.reserve(batch.items.size());

        for (auto *item : batch.items)
        {
            if (!item)
                continue;

            auto transform = item->GetTransform();
            if (transform.IsValid() && !transform.IsMovable())
                static_items.push_back(item);
            else
                movable_items.push_back(item);
        }

        std::sort(static_items.begin(), static_items.end(),
            [](const RenderItem* a, const RenderItem* b) {
                return a->Compare(*b) < 0;
            });

        std::sort(movable_items.begin(), movable_items.end(),
            [](const RenderItem* a, const RenderItem* b) {
                return a->Compare(*b) < 0;
            });

        batch.items.clear();
        batch.items.reserve(static_items.size() + movable_items.size());
        for (auto *item : static_items)
            batch.items.push_back(item);
        for (auto *item : movable_items)
            batch.items.push_back(item);

        for (size_t i = 0; i < batch.items.size(); ++i)
        {
            batch.items[i]->index = i;
        }

        batch.static_count = static_cast<uint32_t>(static_items.size());
    }

    void PrimitiveBatchPipeline::EnsureBatchIndexRows(MaterialBatch& batch)
    {
        if (batch.gpu_driven_override || batch.uses_render_item_resolve)
            return;

        if (!batch.buffer_manager || batch.items.empty())
            return;

        const uint32_t item_count = static_cast<uint32_t>(batch.items.size());
        uint32_t new_node_count = 1;
        while (new_node_count < item_count)
            new_node_count <<= 1;

        // Per-batch L2W index rows SSBO — written in draw order.
        if (!batch.l2w_index_buffer || batch.l2w_index_capacity < item_count)
        {
            batch.l2w_index_capacity = new_node_count;

            if (batch.l2w_index_buffer)
            {
                if (batch.buffer_manager)
                    batch.buffer_manager->Release(batch.l2w_index_buffer);
                else
                    delete batch.l2w_index_buffer;
                batch.l2w_index_buffer = nullptr;
            }

            if (batch.buffer_manager)
            {
                const VkDeviceSize byte_size = static_cast<VkDeviceSize>(batch.l2w_index_capacity) * sizeof(uint32_t);
                batch.l2w_index_buffer = batch.buffer_manager->CreateSSBO(
                    "ECS:Batch:L2WIndex", byte_size, nullptr, graph::SharingMode::Exclusive);
            }
        }

        // Per-batch DataIndex rows SSBO — shader consumes fixed-width rows by instance index.
        if (batch.key.shader_program
            && graph::mtl::MaterialRequiresRecipeRuntimeRows(batch.key.shader_program->GetShaderResourceSchema()))
        {
            if (!batch.material_data_index_rows_buffer || batch.material_data_index_rows_capacity < item_count)
            {
                batch.material_data_index_rows_capacity = new_node_count;

                if (batch.material_data_index_rows_buffer)
                {
                    if (batch.buffer_manager)
                        batch.buffer_manager->Release(batch.material_data_index_rows_buffer);
                    else
                        delete batch.material_data_index_rows_buffer;
                    batch.material_data_index_rows_buffer = nullptr;
                }

                if (batch.buffer_manager)
                {
                    // 每行存 payload + texture-reference 两个索引（8B）。
                    const VkDeviceSize row_bytes =
                        sizeof(graph::mtl::MaterialInstanceAddresses);

                    const VkDeviceSize byte_size =
                        static_cast<VkDeviceSize>(batch.material_data_index_rows_capacity)
                        * row_bytes;
                    batch.material_data_index_rows_buffer = batch.buffer_manager->CreateSSBO(
                        "ECS:Batch:MaterialDataAddresses", byte_size, nullptr, graph::SharingMode::Exclusive);

                    // 关键安全垫：全表预填零，保证任何时刻索引 0 可安全解引用；
                    // 随后每帧 WriteBatchIndexRows 覆盖为真实索引。
                    if (batch.material_data_index_rows_buffer)
                    {
                        if (auto *fill_gpu = batch.material_data_index_rows_buffer->GetGPUBuffer())
                        {
                            if (auto *fill_ptr = static_cast<
                                    graph::mtl::MaterialInstanceAddresses *>(
                                    fill_gpu->Map(0, byte_size)))
                            {
                                memset(fill_ptr, 0, static_cast<size_t>(byte_size));
                                fill_gpu->Unmap();
                            }
                        }
                    }
                }
            }
        }

    }

    void PrimitiveBatchPipeline::WriteBatchIndexRows(MaterialBatch& batch)
    {
        if (batch.items.empty() || batch.gpu_driven_override || batch.uses_render_item_resolve)
            return;

        const uint32_t item_count = static_cast<uint32_t>(batch.items.size());

        // Also write per-batch L2W index rows SSBO in the same draw order.
        // This is what the shader reads via ResolveTransformID(gl_InstanceIndex).
        if (batch.l2w_index_buffer)
        {
            auto *l2w_gpu = batch.l2w_index_buffer->GetGPUBuffer();
            if (l2w_gpu)
            {
                uint32_t *l2w_ptr = static_cast<uint32_t *>(l2w_gpu->Map(0, static_cast<VkDeviceSize>(item_count) * sizeof(uint32_t)));
                if (l2w_ptr)
                {
                    for (size_t i = 0; i < item_count; ++i)
                    {
                        RenderItem* item = batch.items[i];
                        *l2w_ptr = item ? item->transform_index : 0;
                        ++l2w_ptr;
                    }
                    l2w_gpu->Unmap();
                }
            }
        }

        // Write per-batch single-column DataIndex rows in draw order.
        if (batch.material_data_index_rows_buffer)
        {
            auto *mi_gpu = batch.material_data_index_rows_buffer->GetGPUBuffer();

            // 行表写入 payload/texture-reference 两个索引。
            // 优先从 RenderItemDataStorage 依据 4-ID 架构获取；若未注册则从材质运行期**共享行**回退。
            if (mi_gpu)
            {
                auto *row_ptr = static_cast<graph::mtl::MaterialInstanceAddresses *>(
                    mi_gpu->Map(
                        0,
                        static_cast<VkDeviceSize>(item_count)
                            * sizeof(graph::mtl::MaterialInstanceAddresses)));
                if (row_ptr)
                {
                    auto *storage = world ? world->GetRenderItemStorage() : nullptr;
                    for (size_t i = 0; i < item_count; ++i)
                    {
                        row_ptr[i] = { 0, 0 };
                        RenderItem *item = batch.items[i];
                        bool resolved = false;

                        const auto handle = item ? item->GetRenderItemHandle() : graph::INVALID_RENDER_ITEM_HANDLE;
                        if (storage && handle != graph::INVALID_RENDER_ITEM_HANDLE)
                        {
                            const auto *desc = storage->Get(handle);
                            if (desc)
                            {
                                row_ptr[i].payload_index = desc->material_id;
                                row_ptr[i].texture_reference_index = desc->texture_id;
                                resolved = true;
                            }
                        }

                        if (!resolved)
                        {
                            auto *primitive_item = dynamic_cast<PrimitiveRenderItem *>(item);
                            const MaterialRuntimeRow *material_row = primitive_item
                                ? primitive_item->GetMaterialRuntimeRow()
                                : nullptr;
                            if (material_row)
                            {
                                row_ptr[i].payload_index =
                                    material_row->data_index_row != uint32_t(-1)
                                        ? material_row->data_index_row : 0u;

                                row_ptr[i].texture_reference_index =
                                    material_row->material_texture_configuration.row_index;
                            }
                        }

                        // ── D3：接收侧阴影参数（逐图元）───────────────────────
                        // ShadowProxy 未挂载时保持行的零值 = 引擎默认
                        //（正常接收 + 倍率 1.0），与 D3 之前的行逐字节一致。
                        // 该行每帧在 FinalizeBatch 重写，故运行期 SetReceiveShadow /
                        // SetBiasMultiplier 下一帧即生效。
                        if (auto *prim_item = dynamic_cast<PrimitiveRenderItem *>(item))
                        {
                            Entity *prim_entity = item->GetEntity();

                            if (prim_entity)
                            {
                                // A5b：阴影接收/倍率经实体级自由函数取（原可渲染组件
                                // 的便利方法随该类删除，缺省约定唯一出处见 ShadowProxy.h）。
                                // A7a：接收判定由策略表给出（RenderNeed::ShadowReceiver 是唯一
                                // 判据）。receive_shadow 事实仍由实体级自由函数提供——缺省约定
                                // 的唯一出处见 ShadowProxy.h。
                                StrategyFacts strategy_facts;
                                strategy_facts.receive_shadow = CanReceiveShadow(prim_entity);

                                const uint32_t strategy_needs =
                                    EvaluateRenderNeed(prim_entity->GetComponentMask(),
                                                       strategy_facts.ToMask());

#if ULRE_STRATEGY_PARITY_ENABLED
                                // 反向守卫：参考实现 = 旧判据（Release 下整块编空）
                                ParityCheckShadowReceiver(
                                    HasRenderNeed(strategy_needs,RenderNeed::ShadowReceiver),
                                    CanReceiveShadow(prim_entity),
                                    "PrimitiveBatchPipeline");
#endif

                                if (!HasRenderNeed(strategy_needs,RenderNeed::ShadowReceiver))
                                    row_ptr[i].shadow_flags |=
                                        graph::mtl::kMaterialShadowFlagNoReceive;

                                const float bias_multiplier =
                                    GetShadowBiasMultiplier(prim_entity);

                                // 非法倍率（<=0 / NaN / inf）按"不调节"处理：
                                // 行里留 0，着色侧视 0 为引擎默认 1.0。
                                if (bias_multiplier > 0.0f
                                 && std::isfinite(bias_multiplier))
                                    row_ptr[i].shadow_bias_multiplier = bias_multiplier;
                            }
                        }

                        // A1-4：batch 的纹理引用池基址（pc_root.addr_texture_
                        // references）**无论 desc 是否 resolved 都要设置**——
                        // 此前它只在 fallback 分支赋值：4-ID 描述符正常时（稳态
                        // 每帧如此）addr_texture_references 恒 0，MTL_TEX 解引
                        // 用地址 0 → tex_handle=0 → SampleOptional fallback
                        // 1.0，masked 阴影退化为实心（本体批曾因首帧 desc 未
                        // 同步偶然走过 fallback 才幸免）。
                        {
                            auto *primitive_item = dynamic_cast<PrimitiveRenderItem *>(item);
                            const MaterialRuntimeRow *material_row = primitive_item
                                ? primitive_item->GetMaterialRuntimeRow()
                                : nullptr;
                            if (material_row
                             && batch.texture_reference_base_addr == 0)
                            {
                                if (material_row->material_texture_zero_row_gpu)
                                    batch.texture_reference_base_addr =
                                        material_row->material_texture_zero_row_gpu;
                                else if (material_row->material_texture_row_gpu)
                                    batch.texture_reference_base_addr =
                                        material_row->material_texture_row_gpu -
                                        uint64_t(material_row->material_texture_configuration.row_index) *
                                        uint64_t(material_row->material_texture_configuration.row_stride);
                            }
                        }
                    }

                    mi_gpu->Unmap();
                }
                return;
            }
        }
    }
    void PrimitiveBatchPipeline::BuildMaterialBatches()
    {
        if (!world)
            return;

        auto& cache = world->GetRenderFrameCache();

        TransformAssignmentBuffer* shared_transform_buffer = nullptr;
        if (auto transform_system = world->GetSystem<TransformSystem>())
        {
            shared_transform_buffer = transform_system->GetTransformBuffer();
        }

        auto buffer_manager = GetBufferManager();
        // 当前渲染目标唯一权威：world->GetRenderTarget()（RenderContext 副本已删除）
        auto* rt = world ? world->GetRenderTarget() : nullptr;
        auto* current_render_pass = rt ? rt->GetRenderPass() : nullptr;

        for (auto& itemPtr : cache.renderItems)
        {
            RenderItem* item = itemPtr.get();
            if (!item || !item->isVisible)
                continue;

            auto* shader_prog = item->GetShaderProgram();
            auto* pipeline = item->GetPipeline(current_render_pass);
            auto* prim_item = dynamic_cast<PrimitiveRenderItem*>(item);
            const MaterialRuntimeRow *material_row = prim_item ? prim_item->GetMaterialRuntimeRow() : nullptr;

            if (prim_item)
            {
                const bool needs_recipe_rows = shader_prog
                    && graph::mtl::MaterialRequiresRecipeRuntimeRows(shader_prog->GetShaderResourceSchema());
                const bool missing_rows = needs_recipe_rows
                                       && (!material_row
                                        || material_row->data_index_row == uint32_t(-1));
                if (missing_rows)
                {
                    LogWarning("[PrimitiveBatchPipeline] Skip primitive item: unresolved recipe rows. shader_prog=%s",
                               shader_prog ? shader_prog->GetName().c_str() : "<null>");
                    continue;
                }
            }

            if (!shader_prog || !pipeline)
            {
                // A7b：「有无材质来源」不再用于**静默**这条失败告警 —— 无材质来源的图元
                // 现在会走回退（错误）材质进入批次，其管线解析失败同样必须留痕，不能因为
                // 它“本来没有材质”就无声跳过（这正是被用户拍板取消的那条旧语义）。
                LogWarning("[PrimitiveBatchPipeline] Skip primitive item: unresolved runtime pipeline. shader_prog=%s render_pass=%p",
                           shader_prog ? shader_prog->GetName().c_str() : "<null>",
                           current_render_pass);
                continue;
            }

            ShaderProgramPipelineKey key(shader_prog,
                                         pipeline);
            auto* batch_ptr = cache.materialBatches.GetValuePointer(key);

            if (!batch_ptr)
            {
                auto batch = std::make_unique<MaterialBatch>(key, device, buffer_manager);
                batch->transform_buffer = shared_transform_buffer;
                batch->AddItem(item);
                cache.materialBatches[key] = std::move(batch);
            }
            else
            {
                (*batch_ptr)->buffer_manager = buffer_manager;
                (*batch_ptr)->transform_buffer = shared_transform_buffer;
                (*batch_ptr)->AddItem(item);
            }
        }
    }

    void PrimitiveBatchPipeline::TryApplyGPUCullOverride()
    {
        auto& cache = world->GetRenderFrameCache();

        // 每帧先复位（评估失败/从未评估 ⇒ 恒不接管；collect 读本记账）
        ++gpu_cull_override_eval_frame_;

        auto gpu_cull = world->GetSystem<GPUCullSystem>();
        if (!gpu_cull || !gpu_cull->IsEnabled() || !gpu_cull->IsDrawOverrideEnabled())
            return;

        // 只在主帧 pass 接管（shadow 批共用 materialBatches 表 ⇒ 必须显式挡）
        if (world->IsCurrentPassShadow())
            return;

        // 恰好一个含几何的批（R4 单批口径）
        MaterialBatch *target = nullptr;
        uint32_t geometry_batches = 0;

        for (auto& pair : cache.materialBatches)
        {
            MaterialBatch *b = pair.second.get();
            if (!b || b->items.empty() || !BatchRequiresIndirectCommands(*b))
                continue;

            ++geometry_batches;
            target = b;
        }

        auto reject = [this](const char *why)
        {
            static bool s_reported = false;
            if (!s_reported)
            {
                s_reported = true;
                GLogError("[T3f2] GPU cull draw override 前提不满足：%s ⇒ 本场景不接管"
                          "（绘制走 CPU 账；管线本身仍在 dispatch/对拍）", why);
            }
        };

        if (geometry_batches != 1)
        {
            reject(geometry_batches == 0 ? "无含几何批" : "多批（单批口径未满足）");
            return;
        }

        if (!ProgramHasMeshStage(target->key.shader_program))
        {
            reject("批无 mesh stage");
            return;
        }

        if (target->key.shader_program->GetPrimitiveType() == graph::PrimitiveType::Lines)
        {
            reject("批是 lines（X 组数公式不同源）");
            return;
        }

        if (target->items.size() > GPUCullSystem::GetGatherCapacity())
        {
            reject("批容量超 gather 表（kMaxVisibleEntities）");
            return;
        }

        auto *gc = world->GetGraphicsContext();
        auto *pool = gc ? gc->GetGlobalSSBOBufferRegistry() : nullptr;
        const EntityRenderIDTable &rid_table = world->GetEntityRenderIDTable();

        for (auto *item : target->items)
        {
            if (!item || dynamic_cast<InstancedPrimitiveRenderItem *>(item))
            {
                reject("含实例化/多实例项（gather 每实体一命令 Y=1）");
                return;
            }

            Entity *owner = item->GetEntity();
            if (!owner)
            {
                reject("含无 owner 实体项（4-ID 表只覆盖实体）");
                return;
            }

            const graph::RenderItemDescriptor *ids =
                rid_table.Row(owner->GetEntityID().index);

            if (!ids || ids->geometry_id == 0)
            {
                reject("实体 4-ID 行缺失");
                return;
            }

            const auto *params = pool ? pool->GetMeshDrawParamsRow(ids->geometry_id) : nullptr;

            if (!params)
            {
                reject("MeshDrawParams 池行缺失");
                return;
            }

            if (params->addr_meshlets != 0)
            {
                reject("meshlet 几何（X 组数与 CPU GetMeshletCount 分支不同源）");
                return;
            }

            // 阴影两位：全默认才可接管（gather 写 0/0 = 引擎默认语义）
            if (!CanReceiveShadow(owner) || GetShadowBiasMultiplier(owner) != 1.0f)
            {
                reject("实体有非默认阴影参数（ShadowProxy GPU 行未接线）");
                return;
            }
        }

        // 稳态判据护栏（与形态 A 首帧护栏同源，§8.2c）：实体表脏行上传与首次
        // dispatch 同帧 ⇒ 首帧 `survivors=0`；此时接管 = 一帧黑屏。dispatch 是
        // 同步提交（WaitLastSubmitFence）⇒ 这里读到的是**本帧**结果，可信帧恒 >0
        //（场景确有可见实体时）。判据不可信 ⇒ 本帧不接管，CPU 账继续。
        if (gpu_cull->GetGPUVisibleCount() == 0 && !target->items.empty())
            return;

        // 全部前提通过 ⇒ 接管
        if (gpu_cull->ApplyGPUCullOverrideHook(target))
        {
            gpu_cull_override_frame_ = gpu_cull_override_eval_frame_;

            // 正向判据日志（一次性）：像素对照必须证明"绘制真走 gather 表"，
            // 静默退回 CPU 账也会像素全等 ⇒ 没有本日志就不能宣称接管成立。
            static bool s_engaged_logged = false;
            if (!s_engaged_logged)
            {
                s_engaged_logged = true;
                GLogInfo("[T3f2] GPU cull draw override ENGAGED: batch items=%u survivors=%u"
                         "（行表/icb/count 已指向 gather 输出；CPU 剔除自本帧起跳过）",
                         (uint32_t)target->items.size(),
                         gpu_cull->GetGPUVisibleCount());
            }
        }
    }

    void PrimitiveBatchPipeline::FinalizeBatches()
    {
        if (!world)
            return;

        auto& cache = world->GetRenderFrameCache();

        // T3f-2（形态 B）：接管判定先于任何 FinalizeBatch —— hook 成功 ⇒ 批带上
        // `gpu_driven_override`，后续 sort/ensure/build/write 走既有 GPU-Driven 跳过契约；
        // 失败 ⇒ hook 内部把批复位回 CPU 模式（幂等，防上一帧接管残留把本帧带进
        // "CPU 路径被跳、指针却指着 GPU 表"的坏帧）。
        TryApplyGPUCullOverride();

        for (auto& pair : cache.materialBatches)
        {
            if (pair.second)
                FinalizeBatch(*pair.second);
        }
    }
}
