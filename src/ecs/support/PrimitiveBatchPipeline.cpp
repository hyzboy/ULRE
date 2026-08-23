#include<hgl/ecs/support/PrimitiveBatchPipeline.h>
#include<source_location>
#include<cstdio>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/BoundingBoxComponent.h>
#include<hgl/ecs/systems/tick/BoundingBoxUpdateSystem.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/MaterialComponent.h>
#include<hgl/ecs/core/PrimitiveRenderItem.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/object/ObjectTracker.h>
#include<hgl/util/hash/FNV1a.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKObjectNameBuilder.h>
#include<hgl/vk/VKIndirectCommandBuffer.h>
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
        bool BatchRequiresIndirectCommands(const MaterialBatch &batch)
        {
            for (auto *item : batch.items)
            {
                const auto *data_buffer = item ? item->GetGeometryDataBuffer() : nullptr;
                if (data_buffer && data_buffer->vdm)
                    return true;
            }

            return false;
        }

        void ReleaseICB(MaterialBatch &batch)
        {
            if (batch.icb_draw)
            {
                delete batch.icb_draw;
                batch.icb_draw = nullptr;
            }

            if (batch.icb_mesh_tasks)
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

        void WriteICB(VkDrawIndirectCommand* draw_cmd, DrawBatch* batch)
        {
            if (!draw_cmd || !batch || !batch->geom_draw_range)
                return;

            draw_cmd->vertexCount = (batch->geom_draw_range->index_count > 0)
                ? batch->geom_draw_range->index_count
                : batch->geom_draw_range->vertex_count;
            draw_cmd->instanceCount = batch->instance_count;
            draw_cmd->firstVertex = 0;   // SSBO 顶点输入：段偏移走 push constant（vertex_base）
            draw_cmd->firstInstance = batch->first_instance;
        }

        uint64_t ResolveSSBOBindingSignature(RenderItem *item)
        {
            hgl::hash::FNV1aHasher64 h;
            const auto *primitive_item = dynamic_cast<PrimitiveRenderItem *>(item);
            const auto material_comp = primitive_item ? primitive_item->GetMaterialComponent() : nullptr;
            if (!material_comp)
                return static_cast<uint64_t>(h);

            const uint32_t binding_count =
                static_cast<uint32_t>(material_comp->resolved_ssbo_bindings.size());
            h << binding_count;
            for (const auto &binding : material_comp->resolved_ssbo_bindings)
            {
                h << binding.ssbo_type
                  << binding.ssbo_id
                  << binding.valid;
            }

            return static_cast<uint64_t>(h);
        }
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

        return true;
    }

    void PrimitiveBatchPipeline::RunCulling()
    {
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

            auto bbox = entity->GetComponent<BoundingBoxComponent>();
            if (bbox)
            {
                if (bbox->HasWorldAABB())
                    item->isVisible = TestFrustumWithWorldAABB(item, bbox.get());
                else
                    item->isVisible = TestFrustumWithLocalAABB(item, bbox.get());
            }
            else
            {
                item->isVisible = TestFrustumWithBoundingSphere(item);
            }
        }
    }

    bool PrimitiveBatchPipeline::TestFrustumWithWorldAABB(RenderItem* item, const BoundingBoxComponent* bbox)
    {
        const auto& world_aabb = bbox->GetWorldAABB();
        const glm::vec3 world_center = world_aabb.GetCenter();
        const glm::vec3 world_extents = world_aabb.GetExtent();
        const float radius = glm::length(world_extents);

        return frustum.SphereIn(world_center, radius) != math::Frustum::Scope::OUTSIDE;
    }

    bool PrimitiveBatchPipeline::TestFrustumWithLocalAABB(RenderItem* item, const BoundingBoxComponent* bbox)
    {
        const glm::vec3 local_center = bbox->GetCenter();
        const glm::vec3 local_extents = bbox->GetExtents();
        const float radius = glm::length(local_extents);

        const glm::mat4 worldMat = item->GetWorldMatrix();
        const glm::vec3 world_center = glm::vec3(worldMat * glm::vec4(local_center, 1.0f));

        return frustum.SphereIn(world_center, radius) != math::Frustum::Scope::OUTSIDE;
    }

    bool PrimitiveBatchPipeline::TestFrustumWithBoundingSphere(RenderItem* item)
    {
        auto* prim_item = dynamic_cast<PrimitiveRenderItem*>(item);
        if (!prim_item)
            return true;  // No bounding sphere data for this item type — keep visible
        auto primitiveComp = prim_item->GetPrimitiveComponent();
        if (!primitiveComp)
            return false;

        auto transform = item->GetTransform();
        if (!transform)
            return false;

        glm::vec3 worldPos = transform->GetWorldPosition();
        float boundingRadius = primitiveComp->GetBoundingRadius();

        if (boundingRadius <= 0.0f)
            return true;

        return frustum.SphereIn(worldPos, boundingRadius) != math::Frustum::Scope::OUTSIDE;
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
        uint32_t static_count = 0;
        uint32_t dynamic_count = 0;
        uint32_t dynamic_base = 0;

        if (transform_system)
        {
            static_count = transform_system->GetStaticCount();
            dynamic_count = transform_system->GetDynamicCount();
            dynamic_base = transform_system->GetDynamicBaseIndex(static_count, dynamic_count);
        }

        for (auto& itemPtr : cache.renderItems)
        {
            RenderItem* item = itemPtr.get();
            if (!item)
                continue;

            auto transform = item->GetTransform();
            if (!transform)
                continue;

            const auto handle = transform->GetStorageHandle();
            if (handle == TransformDataStorage::INVALID_HANDLE)
            {
                item->transform_index = 0;
                continue;
            }

            uint32_t group_index = 0;
            if (transform_system &&
                transform_system->TryGetTransformGroupIndex(handle, transform->IsMovable(), group_index))
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
                        item->transform_index = transform->IsMovable() ? (dynamic_base + group_index) : (group_index + 1);
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
            std::string draw_str = world->GetResourceNamePrefix() + ":IndirectDrawBuffer";

            draw_name = graph::ObjectNameBuilder(draw_str.c_str());
        }
        else
        {
            draw_name = graph::ObjectNameBuilder("RPBS_IndirectDrawBuffer");
        }

        return draw_name;
    }

    void PrimitiveBatchPipeline::ReallocICB(MaterialBatch& batch)
    {
        HGL_CAPTURE_SCOPE();
        if (!device || batch.items.empty())
            return;

        uint32_t icb_new_count = 1;
        while (icb_new_count < batch.items.size())
            icb_new_count <<= 1;

        // IndirectMeshDraw：mesh 材质需 mesh 命令缓冲（与 legacy ICB 同容量策略）
        const bool mesh_material = ProgramHasMeshStage(batch.key.shader_program);

        if (batch.icb_draw
         && icb_new_count <= batch.icb_draw->GetMaxCount()
         && (!mesh_material
             || (batch.icb_mesh_tasks
                 && icb_new_count <= batch.icb_mesh_tasks->GetMaxCount())))
            return;

        ReleaseICB(batch);

        auto draw_name = BuildICBNames();

        batch.icb_draw = device->CreateIndirectDrawBuffer(icb_new_count, draw_name);

        if (mesh_material)
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

            if (!batch.icb_draw)
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

        VkDrawIndirectCommand* draw_cmd = nullptr;

        if (needs_indirect)
        {
            draw_cmd = batch.icb_draw->MapCmd();

            if (!draw_cmd)
            {
                batch.icb_draw->Unmap();
                batch.draw_batches_count = 0;
                batch.draw_batches.clear();
                return;
            }
        }

        batch.draw_batches.clear();
        batch.draw_batches.resize(count);

        DrawBatch* draw_batch = batch.draw_batches.data();
        RenderItem* item = batch.items[0];
        const graph::GeometryDataBuffer *data_buffer = item ? item->GetGeometryDataBuffer() : nullptr;
        const graph::GeometryDrawRange *draw_range = item ? item->GetGeometryDrawRange() : nullptr;
        const graph::Geometry *geometry = nullptr;
        if (auto *prim_item = dynamic_cast<PrimitiveRenderItem *>(item))
        {
            auto prim_comp = prim_item->GetPrimitiveComponent();
            if (prim_comp && prim_comp->GetPrimitiveAsset())
                geometry = prim_comp->GetPrimitiveAsset()->GetGeometry();
        }

        if (!data_buffer || !draw_range)
        {
            // needs_indirect==false 时 ICB 已被 ReleaseICB 置空，Unmap 会崩溃
            if (needs_indirect)
            {
                batch.icb_draw->Unmap();
            }
            batch.draw_batches_count = 0;
            batch.draw_batches.clear();
            return;
        }

        batch.draw_batches_count = 1;
        draw_batch->first_instance = base_instance;
        draw_batch->instance_count = 1;
        draw_batch->Set(data_buffer, draw_range, geometry);

        const graph::GeometryDataBuffer* current_data_buffer = draw_batch->geom_data_buffer;
        const graph::GeometryDrawRange* current_draw_range = draw_batch->geom_draw_range;

        for (size_t i = 1; i < count; i++)
        {
            item = batch.items[i];
            data_buffer = item ? item->GetGeometryDataBuffer() : nullptr;
            draw_range = item ? item->GetGeometryDrawRange() : nullptr;

            if (!data_buffer || !draw_range)
                continue;

            const graph::GeometryDataBuffer* item_data_buf = data_buffer;
            const graph::GeometryDrawRange* item_draw_range = draw_range;

            if (*current_data_buffer == *item_data_buf &&
                *current_draw_range == *item_draw_range)
            {
                ++draw_batch->instance_count;
                continue;
            }

            if (needs_indirect && draw_batch->geom_data_buffer && draw_batch->geom_data_buffer->vdm)
            {
                WriteICB(draw_cmd++, draw_batch);
            }

            ++batch.draw_batches_count;
            ++draw_batch;

            draw_batch->first_instance = base_instance + static_cast<uint32_t>(i);
            draw_batch->instance_count = 1;
            draw_batch->Set(data_buffer, draw_range);

            current_data_buffer = draw_batch->geom_data_buffer;
            current_draw_range = draw_batch->geom_draw_range;
        }

        if (needs_indirect && draw_batch->geom_data_buffer && draw_batch->geom_data_buffer->vdm)
        {
            WriteICB(draw_cmd, draw_batch);
        }

        if (needs_indirect)
        {
            batch.icb_draw->Unmap();
        }

        // ── IndirectMeshDraw：mesh 命令 + per-draw 参数行（后置统一写）──
        WriteMeshDrawCommands(batch);
    }

    void PrimitiveBatchPipeline::EnsureMeshDrawParams(MaterialBatch& batch)
    {
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
            * sizeof(graph::mtl::MeshDrawParams);
        batch.mesh_draw_params_buffer = batch.buffer_manager->CreateSSBO(
            "ECS:Batch:MeshDrawParams", byte_size, nullptr, graph::SharingMode::Exclusive);
    }

    void PrimitiveBatchPipeline::WriteMeshDrawCommands(MaterialBatch& batch)
    {
        const uint32_t count = batch.draw_batches_count;
        if (count == 0 || !batch.key.shader_program)
            return;

        const bool is_lines =
            batch.key.shader_program->GetPrimitiveType() == graph::PrimitiveType::Lines;

        // viewport 高度（LineQuad 线宽换算用）——渲染目标高度，与录制期 cmd viewport 一致
        uint32_t viewport_height = 1;
        if (world)
        {
            auto *render_ctx = world->GetRenderContext();
            auto *rt = render_ctx ? render_ctx->GetCurrentRenderTarget() : nullptr;
            if (rt)
                viewport_height = rt->GetExtent().height;
        }

        // 参数行：每个 DrawBatch 一行（含私有 VBO 直接绘制——渲染器按行 offset 视图绑定）
        if (batch.mesh_draw_params_buffer)
        {
            auto *params_gpu = batch.mesh_draw_params_buffer->GetGPUBuffer();
            auto *row = params_gpu
                ? static_cast<graph::mtl::MeshDrawParams *>(params_gpu->Map(
                      0, static_cast<VkDeviceSize>(count) * sizeof(graph::mtl::MeshDrawParams)))
                : nullptr;

            if (row)
            {
                for (uint32_t i = 0; i < count; ++i)
                {
                    DrawBatch &db = batch.draw_batches[i];
                    const auto *range = db.geom_draw_range;

                    db.params_row = i;
                    row[i].index_base = static_cast<uint32_t>(range ? range->first_index : 0);
                    row[i].vertex_base = static_cast<uint32_t>(range ? range->vertex_offset : 0);
                    row[i].is_indexed = (range && range->index_count > 0) ? 1u : 0u;
                    // total_vertices：索引几何 = index_count（每索引 1 顶点查表），非索引 = vertex_count
                    row[i].total_vertices = range
                        ? static_cast<uint32_t>(range->index_count > 0
                             ? range->index_count
                             : range->vertex_count)
                        : 0u;
                    row[i].viewport_height = static_cast<float>(viewport_height);
                    row[i].first_instance = db.first_instance;
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
                    if (!db.geom_data_buffer || !db.geom_data_buffer->vdm
                     || !db.geom_draw_range)
                        continue;

                    const auto *range = db.geom_draw_range;
                    const uint32_t total_vertices = static_cast<uint32_t>(
                        range->index_count > 0 ? range->index_count
                                               : range->vertex_count);

                    mesh_cmd->groupCountX = CalcMeshGroupCount(is_lines, total_vertices);
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
        SortBatchItems(batch);
        EnsureBatchIndexRows(batch);
        EnsureMeshDrawParams(batch);
        BuildBatches(batch, 0);
        WriteBatchIndexRows(batch);
    }

    void PrimitiveBatchPipeline::SortBatchItems(MaterialBatch& batch)
    {
        std::vector<RenderItem*> static_items;
        std::vector<RenderItem*> movable_items;
        static_items.reserve(batch.items.size());
        movable_items.reserve(batch.items.size());

        for (auto *item : batch.items)
        {
            if (!item)
                continue;

            auto transform = item->GetTransform();
            if (transform && !transform->IsMovable())
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
        if (!batch.buffer_manager || batch.items.empty())
            return;

        const uint32_t item_count = static_cast<uint32_t>(batch.items.size());
        uint32_t new_node_count = 1;
        while (new_node_count < item_count)
            new_node_count <<= 1;

        // Per-batch L2W index rows SSBO — written in draw order.
        if (!batch.l2w_index_rows_buffer || batch.l2w_index_rows_capacity < item_count)
        {
            batch.l2w_index_rows_capacity = new_node_count;

            if (batch.l2w_index_rows_buffer)
            {
                if (batch.buffer_manager)
                    batch.buffer_manager->Release(batch.l2w_index_rows_buffer);
                else
                    delete batch.l2w_index_rows_buffer;
                batch.l2w_index_rows_buffer = nullptr;
            }

            if (batch.buffer_manager)
            {
                const VkDeviceSize byte_size = static_cast<VkDeviceSize>(batch.l2w_index_rows_capacity) * sizeof(uint32_t);
                batch.l2w_index_rows_buffer = batch.buffer_manager->CreateSSBO(
                    "ECS:Batch:L2WIndexRows", byte_size, nullptr, graph::SharingMode::Exclusive);
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
                    const VkDeviceSize byte_size =
                        static_cast<VkDeviceSize>(batch.material_data_index_rows_capacity)
                        * graph::mtl::MaterialDataIndexRowStride * sizeof(uint32_t);
                    batch.material_data_index_rows_buffer = batch.buffer_manager->CreateSSBO(
                        "ECS:Batch:MaterialDataIndexRows", byte_size, nullptr, graph::SharingMode::Exclusive);
                }
            }
        }

    }

    void PrimitiveBatchPipeline::WriteBatchIndexRows(MaterialBatch& batch)
    {
        if (batch.items.empty())
            return;

        const uint32_t item_count = static_cast<uint32_t>(batch.items.size());

        // Also write per-batch L2W index rows SSBO in the same draw order.
        // This is what the shader reads via ResolveTransformID(gl_InstanceIndex).
        if (batch.l2w_index_rows_buffer)
        {
            auto *l2w_gpu = batch.l2w_index_rows_buffer->GetGPUBuffer();
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

        // Write per-batch fixed-width DataIndex rows in draw order.
        if (batch.material_data_index_rows_buffer)
        {
            auto *mi_gpu = batch.material_data_index_rows_buffer->GetGPUBuffer();
            if (mi_gpu)
            {
                uint32_t *row_ptr = static_cast<uint32_t *>(
                    mi_gpu->Map(
                        0,
                        static_cast<VkDeviceSize>(item_count)
                        * graph::mtl::MaterialDataIndexRowStride
                        * sizeof(uint32_t)));
                if (row_ptr)
                {
                    for (size_t i = 0; i < item_count; ++i)
                    {
                        const uint32_t base =
                            static_cast<uint32_t>(i) * graph::mtl::MaterialDataIndexRowStride;
                        for (uint32_t slot = 0; slot < graph::mtl::MaterialDataIndexRowStride; ++slot)
                            row_ptr[base + slot] = 0u;

                        auto *primitive_item = dynamic_cast<PrimitiveRenderItem *>(batch.items[i]);
                        auto material_comp = primitive_item
                            ? primitive_item->GetMaterialComponent()
                            : nullptr;
                        if (material_comp && !material_comp->data_index_values.empty())
                        {
                            for (uint32_t slot = 0;
                                 slot < material_comp->data_index_values.size()
                                  && slot < graph::mtl::MaterialDataIndexRowStride;
                                 ++slot)
                                row_ptr[base + slot] = material_comp->data_index_values[slot];
                        }
                    }
                    mi_gpu->Unmap();
                }
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
        auto* render_ctx = world ? world->GetRenderContext() : nullptr;
        auto* rt = render_ctx ? render_ctx->GetCurrentRenderTarget() : nullptr;
        auto* current_render_pass = rt ? rt->GetRenderPass() : nullptr;

        for (auto& itemPtr : cache.renderItems)
        {
            RenderItem* item = itemPtr.get();
            if (!item || !item->isVisible)
                continue;

            auto* shader_prog = item->GetShaderProgram();
            auto* pipeline = item->GetPipeline();
            auto* prim_item = dynamic_cast<PrimitiveRenderItem*>(item);
            auto prim_comp = prim_item ? prim_item->GetPrimitiveComponent() : nullptr;
            const std::shared_ptr<MaterialComponent> material_comp = prim_item ? prim_item->GetMaterialComponent() : nullptr;

            if (prim_item)
            {
                const bool needs_recipe_rows = shader_prog
                    && graph::mtl::MaterialRequiresRecipeRuntimeRows(shader_prog->GetShaderResourceSchema());
                const bool missing_rows = needs_recipe_rows
                                       && (!material_comp
                                        || material_comp->data_index_row == uint32_t(-1));
                if (missing_rows)
                {
                    LogWarning("[PrimitiveBatchPipeline] Skip primitive item: unresolved recipe rows. shader_prog=%s",
                               shader_prog ? shader_prog->GetName().c_str() : "<null>");
                    continue;
                }
            }

            if (!shader_prog || !pipeline)
            {
                if (prim_comp && prim_comp->HasAnyMaterialRecipeSource())
                {
                    LogWarning("[PrimitiveBatchPipeline] Skip primitive item: unresolved runtime pipeline. shader_prog=%s render_pass=%p",
                               shader_prog ? shader_prog->GetName().c_str() : "<null>",
                               current_render_pass);
                }
                continue;
            }

            ShaderProgramPipelineKey key(shader_prog,
                                         pipeline,
                                         ResolveSSBOBindingSignature(item));
            auto* batch_ptr = cache.materialBatches.GetValuePointer(key);

            if (!batch_ptr)
            {
                auto batch = std::make_unique<MaterialBatch>(key, device, buffer_manager);
                batch->cameraInfo = camera_info;
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

    void PrimitiveBatchPipeline::FinalizeBatches()
    {
        if (!world)
            return;

        auto& cache = world->GetRenderFrameCache();

        for (auto& pair : cache.materialBatches)
        {
            if (pair.second)
                FinalizeBatch(*pair.second);
        }
    }
}
