#include <hgl/ecs/support/line/SimpleLineRenderPipeline.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/components/SimpleLinesComponent.h>
#include <hgl/ecs/support/cull/BoundingBoxAccessor.h>
#include <hgl/ecs/support/transform/TransformAccessor.h>
#include <hgl/ecs/support/transform/TransformAssignmentBuffer.h>
#include <hgl/ecs/systems/tick/CameraSystem.h>
#include <hgl/ecs/systems/tick/TransformSystem.h>
#include <hgl/ecs/systems/render/buffer/RenderSceneUBOSystem.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/SSBOBufferRegistry.h>
#include <hgl/graph/render/RenderContext.h>
#include <hgl/graph/ubo/GlobalAddresses.h>
#include <hgl/graph/module/ShaderProgramManager.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <hgl/graph/mesh/GeometryDataBuffer.h>
#include <hgl/graph/mesh/GeometryDrawRange.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/MaterialResolver.h>
#include <hgl/mtl/ScenePassProfileResolver.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/VKShaderProgram.h>
#include <hgl/vk/buffer/DeviceBuffer.h>
#include <hgl/vk/VKCommandBuffer.h>
#include <hgl/vk/VKRenderTarget.h>
#include <hgl/graph/ShaderBufferSources.h>
#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/graph/RootAddressPush.h>
#include <hgl/vk/VKBindlessTextureManager.h>
#include <hgl/math/geometry/Frustum.h>
#include <hgl/log/Log.h>
#include <glm/glm.hpp>

namespace hgl::ecs
{
    // A5 限流日志：日志系统无级别过滤（GLogVerbose 与 GLogInfo 同输出），
    // 每帧高频日志用计数器限流——每 60 帧输出一次
    static uint32_t s_simpleline_log_ticks[16] = {};

    template<typename... Args>
    void SimpleLinePeriodicLog(uint32_t &tick, const char *fmt, Args... args)
    {
        if ((++tick % 60u) == 1u)
            GLogInfo(fmt, args...);
    }
    namespace
    {
        // 标准 Line 顶点 ABI：Position(V3F) + Color(R8_UINT 调色板索引) + TransformID(R32_UINT)。
        // 与 VertexPaletteLines 领域契约一致——**没有 Size**（原生 1px 线图元无宽度）。
        graph::GeometryVertexFormat CreateSimpleLineGeometryVertexFormat()
        {
            graph::GeometryVertexFormat gvf;

            gvf.Add(graph::VertexSemantic::Position, VK_FORMAT_R32G32B32_SFLOAT, 3, sizeof(float) * 3);
            gvf.Add(graph::VertexSemantic::Color, VK_FORMAT_R8_UINT, 1, sizeof(uint8_t));
            gvf.Add(graph::VertexSemantic::TransformID,
                    VK_FORMAT_R32_UINT,
                    1,
                    sizeof(uint32_t));

            return gvf;
        }
    }

    // -------------------------------------------------------------------------
    const AnsiString SimpleLineRenderPipeline::kName{ "SimpleLine" };

    // -------------------------------------------------------------------------
    // LineBuffer helpers（单 buffer——同 Line P2 形态）
    // -------------------------------------------------------------------------

    void SimpleLineRenderPipeline::LineBuffer::Reset()
    {
        line_count = 0;
        bool pos_valid = va_pos.IsValid();
        bool color_valid = va_color.IsValid();
        bool transform_valid = va_transform.IsValid();

        if (pos_valid)   va_pos.Seek(0);
        if (color_valid) va_color.Seek(0);
        if (transform_valid) va_transform.Seek(0);
        if (draw_range)  draw_range->vertex_count = 0;
    }

    void SimpleLineRenderPipeline::LineBuffer::Clear()
    {
        va_pos.Bind(nullptr);
        va_color.Bind(nullptr);
        va_transform.Bind(nullptr);
        SAFE_CLEAR(data_buffer);
        SAFE_CLEAR(draw_range);
        SAFE_CLEAR(geometry);
        SAFE_CLEAR(mesh_draw_params);
        line_count   = 0;
        gpu_capacity = 0;
        failed_capacity = 0;
        retry_delay_frames = 0;
        last_failure_frame = 0;
    }

    bool SimpleLineRenderPipeline::LineBuffer::EnsureCapacity(
        uint32_t needed,
        graph::VulkanDevice*     dev,
        uint32_t                 frame_serial,
        uint32_t                 frame_slot)
    {
        if ((gpu_capacity == 0) != (geometry == nullptr)
         || (geometry && (!data_buffer || !draw_range))
         || (!geometry && (data_buffer || draw_range)))
        {
            GLogError("[SimpleLineRenderPipeline] LineBuffer state invariant failed (capacity=%u geometry=%p data=%p range=%p)",
                      gpu_capacity,
                      static_cast<void *>(geometry),
                      static_cast<void *>(data_buffer),
                      static_cast<void *>(draw_range));
            return false;
        }

        if (needed <= gpu_capacity)
        {
            if (geometry
             && (!va_pos.IsValid() || !va_color.IsValid()
              || !va_transform.IsValid()))
            {
                GLogError("[SimpleLineRenderPipeline] LineBuffer accessors invalid at existing capacity=%u",
                          gpu_capacity);
                return false;
            }
            return true;
        }

        if (!dev)
        {
            GLogError("[SimpleLineRenderPipeline] Cannot grow LineBuffer without a Vulkan device");
            return false;
        }

        // Build replacement resources before touching the live buffer. Geometric growth avoids
        // repeated reallocations when the visible line count increases gradually.
        const uint64_t granule = SimpleLineRenderPipeline::LINES_GRANULE;
        // CreateVAB currently computes stride * vertex_count as uint32_t.
        const uint64_t max_capacity = 0xFFFFFFFFu / (2u * 3u * sizeof(float));
        const uint64_t required_capacity = needed;
        const uint64_t rounded_required =
            ((required_capacity + granule - 1u) / granule) * granule;
        uint64_t target_capacity = rounded_required;

        if (gpu_capacity)
        {
            const uint64_t doubled_capacity = static_cast<uint64_t>(gpu_capacity) * 2u;
            if (target_capacity < doubled_capacity)
                target_capacity = doubled_capacity;
        }

        target_capacity = ((target_capacity + granule - 1u) / granule) * granule;
        if (target_capacity > max_capacity)
            target_capacity = rounded_required;

        if (target_capacity > max_capacity)
        {
            GLogError("[SimpleLineRenderPipeline] LineBuffer capacity overflow: need=%u max=%llu",
                      needed,
                      static_cast<unsigned long long>(max_capacity));
            return false;
        }

        if (dev->IsDrawPhaseActive())
        {
            GLogError("[SimpleLineRenderPipeline] Refusing to grow LineBuffer while draw commands are being recorded");
            return false;
        }

        const uint32_t new_cap = static_cast<uint32_t>(target_capacity);
        if (failed_capacity == new_cap
         && retry_delay_frames > 0
         && static_cast<uint32_t>(frame_serial - last_failure_frame) < retry_delay_frames)
            return false;

        auto record_failure = [&]()
        {
            if (failed_capacity != new_cap)
            {
                failed_capacity = new_cap;
                retry_delay_frames = 1;
            }
            else if (retry_delay_frames < 60)
            {
                retry_delay_frames = retry_delay_frames > 30
                    ? 60
                    : retry_delay_frames * 2;
            }
            last_failure_frame = frame_serial;
            return false;
        };

        graph::Geometry* candidate_geometry = graph::CreateGeometry(
            dev,
            CreateSimpleLineGeometryVertexFormat(),
            AnsiString("SimpleLineBuffer:Slot") + AnsiString::numberOf(frame_slot),
            new_cap * 2u,
            0,
            graph::IndexType::AUTO,
            nullptr,
            graph::BufferAllocPolicy::StagedUpload);
        graph::GeometryDataBuffer* candidate_data_buffer = nullptr;
        graph::GeometryDrawRange* candidate_draw_range = nullptr;

        auto discard_candidate = [&]()
        {
            SAFE_CLEAR(candidate_data_buffer);
            SAFE_CLEAR(candidate_draw_range);
            SAFE_CLEAR(candidate_geometry);
        };

        if (!candidate_geometry)
        {
            GLogError("[SimpleLineRenderPipeline] Failed to create replacement LineBuffer geometry (capacity=%u)",
                      new_cap);
            return record_failure();
        }

        // 顶点输入统一为 SSBO：GeometryDataBuffer 槽位数 = Geometry 语义数
        const uint32_t semantic_count = candidate_geometry->GetGeometryVertexFormat().GetCount();

        candidate_data_buffer = new graph::GeometryDataBuffer(
            semantic_count,
            candidate_geometry->GetIBO(),
            candidate_geometry->GetVDM());
        if (!candidate_data_buffer || !candidate_data_buffer->Update(candidate_geometry))
        {
            GLogError("[SimpleLineRenderPipeline] GeometryDataBuffer::Update failed");
            discard_candidate();
            return record_failure();
        }

        candidate_draw_range = new graph::GeometryDrawRange();
        if (!candidate_draw_range)
        {
            GLogError("[SimpleLineRenderPipeline] Failed to create replacement LineBuffer draw range");
            discard_candidate();
            return record_failure();
        }
        candidate_draw_range->Set(candidate_geometry);
        candidate_draw_range->vertex_count = 0;

        const int pos_idx = candidate_geometry->GetVABIndex(graph::VertexSemantic::Position);
        const int color_idx = candidate_geometry->GetVABIndex(graph::VertexSemantic::Color);
        const int transform_idx = candidate_geometry->GetVABIndex(graph::VertexSemantic::TransformID);

        if (pos_idx < 0 || color_idx < 0 || transform_idx < 0)
        {
            GLogError("[SimpleLineRenderPipeline] Replacement LineBuffer is missing a required vertex semantic");
            discard_candidate();
            return record_failure();
        }

        bool replacement_installed = false;
        {
            graph::TypedArrayView3f candidate_va_pos(candidate_geometry->GetVAB(pos_idx));
            graph::TypedArrayView1u8 candidate_va_color(candidate_geometry->GetVAB(color_idx));
            TransformIDAccessor candidate_va_transform(candidate_geometry->GetVAB(transform_idx));
            const bool pos_valid = candidate_va_pos.IsValid();
            const bool color_valid = candidate_va_color.IsValid();
            const bool transform_valid = candidate_va_transform.IsValid();
            if (!pos_valid || !color_valid || !transform_valid)
            {
                GLogError("[SimpleLineRenderPipeline] Replacement LineBuffer accessor map failed (pos=%d color=%d transform=%d)",
                          pos_valid ? 1 : 0,
                          color_valid ? 1 : 0,
                          transform_valid ? 1 : 0);
            }
            else if (geometry && dev->WaitIdleResult() != VK_SUCCESS)
            {
                GLogError("[SimpleLineRenderPipeline] Cannot retire old LineBuffer: device did not become idle");
            }
            else
            {
                va_pos = static_cast<graph::TypedArrayView3f &&>(candidate_va_pos);
                va_color = static_cast<graph::TypedArrayView1u8 &&>(candidate_va_color);
                va_transform = static_cast<TransformIDAccessor &&>(candidate_va_transform);

                SAFE_CLEAR(data_buffer);
                SAFE_CLEAR(draw_range);
                SAFE_CLEAR(geometry);

                geometry = candidate_geometry;
                candidate_geometry = nullptr;
                data_buffer = candidate_data_buffer;
                candidate_data_buffer = nullptr;
                draw_range = candidate_draw_range;
                candidate_draw_range = nullptr;
                replacement_installed = true;
            }
        }

        if (!replacement_installed)
        {
            discard_candidate();
            return record_failure();
        }

        SimpleLinePeriodicLog(s_simpleline_log_ticks[1], "[SimpleLineRenderPipeline] LineBuffer after Bind: pos_valid=%d color_valid=%d transform_valid=%d",
                 va_pos.IsValid() ? 1 : 0,
                 va_color.IsValid() ? 1 : 0,
                 va_transform.IsValid() ? 1 : 0);

        va_pos.Seek(0);
        va_color.Seek(0);
        va_transform.Seek(0);

        line_count = 0;
        gpu_capacity = new_cap;
        failed_capacity = 0;
        retry_delay_frames = 0;
        last_failure_frame = 0;
        return true;
    }

    bool SimpleLineRenderPipeline::LineBuffer::AddSegment(
        const hgl::math::Vector3f& from,
        const hgl::math::Vector3f& to,
        uint8_t                     color_index,
        uint32_t                    transform_index)
    {
        bool pos_valid = va_pos.IsValid();
        bool color_valid = va_color.IsValid();
        bool transform_valid = va_transform.IsValid();

        if (!pos_valid || !color_valid || !transform_valid)
        {
            GLogWarning("[SimpleLineRenderPipeline] AddSegment accessor invalid: pos=%d color=%d transform=%d",
                        pos_valid ? 1 : 0,
                        color_valid ? 1 : 0,
                        transform_valid ? 1 : 0);
            return false;
        }

        // §3-⑤ 同款防御：**三个写入流必须同进同退**。任一失败 ⇒ 三流一律回退到上次成功的游标
        // （回退点 = line_count * 2：每线段 2 顶点锁步写；常见路径不可达，防御性措施）。
        const uint32_t rollback_cursor = line_count * 2u;

        const bool wrote_ok = va_pos.Write(from)
                            & va_pos.Write(to)
                            & va_color.Write(color_index)
                            & va_color.Write(color_index)
                            & va_transform.Write(transform_index)
                            & va_transform.Write(transform_index);

        if (!wrote_ok)
        {
            const bool rolled_back = va_pos.Seek(rollback_cursor)
                                   & va_color.Seek(rollback_cursor)
                                   & va_transform.Seek(rollback_cursor);

            SimpleLinePeriodicLog(s_simpleline_log_ticks[4],
                            "[SimpleLineRenderPipeline] AddSegment 写入失败：本次线段整体丢弃"
                            "（三流回退到游标 %u，回退成功=%d；缓冲到容量末尾时该回退不适用）",
                            rollback_cursor, rolled_back ? 1 : 0);
            return false;
        }

        ++line_count;
        if (draw_range)
            draw_range->vertex_count = line_count * 2;
        return true;
    }

    void SimpleLineRenderPipeline::LineBuffer::Draw(graph::RenderCmdBuffer* cmd)
    {
        if (!cmd)
        {
            GLogWarning("[SimpleLineRenderPipeline] Draw skipped: cmd is null");
            return;
        }

        if (line_count == 0)
            return;

        if (!data_buffer || !draw_range || !geometry || !material)
        {
            GLogWarning("[SimpleLineRenderPipeline] Draw skipped: data_buffer/draw_range/geometry/material null while line_count=%u", line_count);
            return;
        }

        // 顶点数据已 BDA 化：per-draw 参数经 pc_root（push constant）下发——
        // mesh_draw_params 行 0 地址与 l2w 表地址由 Render 在 draw 前 PushRootAddresses。
        // LineList mesh 绘制：每线程 1 线段（2 顶点 1 线图元），threadgroup = MESH_GROUP_SIZE
        const uint32_t group_count = (line_count + SimpleLineRenderPipeline::MESH_GROUP_SIZE - 1)
                                    / SimpleLineRenderPipeline::MESH_GROUP_SIZE;
        cmd->DrawMeshTasks(group_count);
    }

    // -------------------------------------------------------------------------
    // SimpleLineRenderPipeline
    // -------------------------------------------------------------------------

    SimpleLineRenderPipeline::SimpleLineRenderPipeline(ECSContext* context)
        : context_(context)
    {
    }

    SimpleLineRenderPipeline::~SimpleLineRenderPipeline()
    {
        Shutdown();
    }

    const AnsiString& SimpleLineRenderPipeline::GetName() const { return kName; }
    ECSContext*         SimpleLineRenderPipeline::GetWorld() const { return context_; }

    bool SimpleLineRenderPipeline::Initialize()
    {
        if (initialized_)
            return true;

        GLogInfo(OS_TEXT("[SimpleLineRenderPipeline] Initialize: START"));

        auto* gc = context_ ? context_->GetGraphicsContext() : nullptr;
        if (!gc)
        {
            if (auto* rc = context_ ? context_->GetRenderContext() : nullptr)
                gc = rc->GetGraphicsContext();
        }

        if (!gc)
            return false;

        auto* rt = context_->GetRenderTarget();
        if (!rt)
            return false;

        graph::RenderPass* rp = rt->GetRenderPass();
        if (!rp)
            return false;

        device_ = gc->GetDevice();
        if (!device_)
            return false;

        const graph::GeometryVertexFormat line_gvf = CreateSimpleLineGeometryVertexFormat();

        // ------- Create material -------
        // VertexPaletteLines 领域契约携带 mesh_shader_mode = LineList（唯一出处）
        graph::mtl::MaterialRecipe recipe{};
        recipe.domain = graph::mtl::MaterialDomain::VertexPaletteLines;

        auto* mat_mgr = gc->GetMaterialManager();
        if (!mat_mgr)
            return false;

        {
            graph::mtl::MaterialBuildRequest mtl_request{};
            mtl_request.recipe = recipe;
            mtl_request.primitive_type = graph::PrimitiveType::Lines;
            mtl_request.geometry_vertex_format = &line_gvf;
            graph::mtl::PassProfileValidationDiagnostic template_diagnostic{};
            if (!graph::mtl::ResolveScenePassProfileRequest(
                    graph::mtl::PassProfileID::ForwardUnlit,
                    graph::ShaderStage::Fragment,
                    graph::mtl::MakeForwardUnlitProfile(),
                    mtl_request.render_template_request, template_diagnostic))
                return false;
            material_ = mat_mgr->AcquireShaderProgram(mtl_request);
        }
        if (!material_)
            return false;

        // ------- Create pipeline -------
        // mesh 管线（cull off——线图元无绕序，双面设置无副作用；同 Line 复用配置）。
        // EDS 1/2/3：管线只带 shader 部分 ⇒ 这份配置自己留着，绘制期动态下发。
        pipeline_config_ = graph::mtl::MakeLineMeshConfig();
        pipeline_ = rp->CreatePipeline(material_, pipeline_config_);

        if (!pipeline_)
            return false;

        SyncTransformBinding();

        initialized_ = true;

        return true;
    }

    bool SimpleLineRenderPipeline::PrepareFrame()
    {
        if (!context_)
            return false;

        // 键用世界内帧序列号（`RenderFrameCache::frame_serial`）而非 `GetFrameIndex()`：
        // 后者是驱动相机的帧槽号，离屏 pass 恒为固定槽（永不推进）⇒ 同槽后续帧会被误判为"本帧已准备"。
        const uint32_t frame = context_->GetRenderFrameCache().frame_serial;
        if (frame == prepared_frame_)
            return true;

        current_line_buffer_ = nullptr;
        total_line_count_ = 0;
        const uint32_t slot = context_->GetFrameIndex();
        if (slot >= HGL_FRAME_SLOT_TOTAL)
        {
            GLogError("[SimpleLineRenderPipeline] Invalid render frame slot %u (limit=%u)",
                      slot, static_cast<uint32_t>(HGL_FRAME_SLOT_TOTAL));
            return false;
        }

        if (!initialized_ && !Initialize())
            return false;

        SyncTransformBinding();

        current_line_buffer_ = &line_buffers_[slot];
        current_line_buffer_->Reset();

        collected_.Clear();
        stats_ = SimpleLineCollectStats{};
        total_line_count_ = 0;
        prepared_frame_ = frame;

        return true;
    }

    void SimpleLineRenderPipeline::RunCollect()
    {
        if (!PrepareFrame())
            return;

        collected_.Clear();
        stats_ = SimpleLineCollectStats{};

        // Build frustum from camera
        bool frustum_valid = false;
        hgl::math::Frustum frustum;

        auto camera_system = context_->GetSystem<CameraSystem>();
        if (camera_system)
        {
            const auto* cam = context_->GetActiveCameraInfo();
            if (cam)
            {
                frustum.SetMatrix(cam->vp);
                frustum_valid = true;
            }
        }

        // T2-c：收集 = 池的活跃集视图（裸指针）。语义不变：仍是**全场景收集 + 末次一次绘制**。
        ValueArray<SimpleLinesComponent *> all;
        context_->GetComponents<SimpleLinesComponent>(all);

        for (const auto& comp : all)
        {
            if (!comp) continue;
            ++stats_.total_components;

            if (comp->lines.IsEmpty())
            {
                // 空线段不是**可见性剔除**——单列一项，别污染真视锥/可见性统计。
                ++stats_.skipped_empty;
                continue;
            }

            Entity* owner = comp->GetOwner();
            if (!owner) continue;

            // 可见性检查：真值只在实体级
            if (!context_->IsEntityVisible(owner->GetEntityID()))
            {
                ++stats_.culled_by_visibility;
                continue;
            }

            // Frustum cull
            if (frustum_valid)
            {
                const BoundingBoxAccessor bbox = context_->GetBoundingBoxByEntity(owner->GetEntityID());

                if (bbox.IsValid() && bbox.HasWorldBounds())
                {
                    const auto aabb    = bbox.GetWorldBounds();
                    const glm::vec3 c  = aabb.GetCenter();
                    const glm::vec3 e  = aabb.GetExtent();
                    if (frustum.SphereIn(c, glm::length(e)) ==
                        hgl::math::Frustum::Scope::OUTSIDE)
                    {
                        ++stats_.culled_by_frustum;
                        continue;
                    }
                }
            }

            collected_.Add(comp);
            ++stats_.visible_components;
        }
    }

    void SimpleLineRenderPipeline::RunBuild()
    {
        if (!initialized_)
        {
            GLogWarning("[SimpleLineRenderPipeline] RunBuild skipped: pipeline not initialized");
            return;
        }

        if (collected_.empty())
        {
            GLogInfo("[SimpleLineRenderPipeline] RunBuild skipped: no collected line components");
            return;
        }

        if (!current_line_buffer_)
        {
            GLogError("[SimpleLineRenderPipeline] RunBuild skipped: no prepared frame buffer");
            return;
        }
        LineBuffer &line_buffer_ = *current_line_buffer_;

        auto transform_system_sp = context_ ? context_->GetSystem<TransformSystem>() : nullptr;
        TransformSystem* transform_system = transform_system_sp.get();
        uint32_t dynamic_base = 0;
        if (transform_system)
        {
            // Keep line animation independent of external tick ordering:
            // apply pending movable transform dirty updates right before resolving IDs/upload.
            transform_system->Update(0.0f);

            // Ensure transform handle ordering / index maps / buffer layout are up-to-date
            // before resolving per-component TransformID for line vertices.
            transform_system->SubmitTransformUpdates();

            // First frame may create L2W buffer during submit; re-sync descriptor binding
            // immediately so current frame draw does not see an uninitialized set=2 binding.
            SyncTransformBinding();

            dynamic_base = transform_system->GetDynamicBaseIndex();   // B1b：预算决定
        }

        // 单 Line buffer——first pass 只算总数
        uint32_t expected_total = 0;
        for (const auto& comp : collected_)
        {
            const size_t component_line_count = static_cast<size_t>(comp->lines.GetCount());
            if (component_line_count > static_cast<size_t>(0xFFFFFFFFu - expected_total))
            {
                GLogError("[SimpleLineRenderPipeline] Total line count exceeds the supported 32-bit capacity");
                return;
            }
            expected_total += static_cast<uint32_t>(component_line_count);
        }

        // Ensure GPU capacity（单 buffer，按总数）
        const uint32_t frame_serial = context_
            ? context_->GetRenderFrameCache().frame_serial
            : 0;
        if (!line_buffer_.EnsureCapacity(expected_total, device_, frame_serial, context_->GetFrameIndex()))
        {
            SimpleLinePeriodicLog(s_simpleline_log_ticks[10],
                            "[SimpleLineRenderPipeline] EnsureCapacity failed: need=%u cap=%u",
                            expected_total,
                            line_buffer_.gpu_capacity);
            return; // Allocation failure: skip frame
        }

        if (line_buffer_.gpu_capacity < expected_total)
        {
            GLogWarning("[SimpleLineRenderPipeline] LineBuffer capacity insufficient after ensure: cap=%u need=%u",
                        line_buffer_.gpu_capacity, expected_total);
        }

        line_buffer_.Reset(); // seek back to 0

        // Second pass: write segments
        uint32_t write_fail_count = 0;
        uint32_t resolved_transform_components = 0;
        for (const auto& comp : collected_)
        {
            uint32_t transform_id = 0;
            if (transform_system)
            {
                Entity* owner = comp ? comp->GetOwner() : nullptr;
                const TransformAccessor transform = owner ? context_->GetTransformByEntity(owner->GetEntityID())
                                                          : TransformAccessor{};
                if (transform.IsValid())
                {
                    const TransformDataStorage::HandleID handle = transform.GetID();
                    uint32_t slot = 0;
                    if (handle != TransformDataStorage::INVALID_HANDLE
                        && transform_system->TryGetTransformSlot(handle, transform.IsMovable(), slot))
                    {
                        // L2W 行号约定（具名权威 = `TransformAssignmentBuffer.h`）：静态 `kFirstObjectL2WSlot + slot`
                        // / 动态 `预算基址 + slot`
                        transform_id = transform.IsMovable() ? (dynamic_base + slot)
                                                             : (kFirstObjectL2WSlot + slot);

                        if (transform_id != 0)
                            ++resolved_transform_components;
                    }
                }
            }

            for (const auto& seg : comp->lines)
            {
                if (!line_buffer_.AddSegment(seg.from, seg.to, seg.color_index, transform_id))
                {
                    ++write_fail_count;
                }
            }
        }

        // Tally total
        total_line_count_ = line_buffer_.line_count;
        (void)write_fail_count;
        (void)resolved_transform_components;

        // IndirectMeshDraw：写 mesh per-draw 参数（单 draw 非实例化——
        // 绘制时 gl_DrawID=0；shader 经 MeshDrawCommand -> geometry_id 查全局池）
        if (device_ && total_line_count_ > 0)
        {
            if (!line_buffer_.mesh_draw_params)
                line_buffer_.mesh_draw_params = device_->CreateSSBO(
                    "ECS:SimpleLine:MeshDrawCommands", sizeof(graph::mtl::MeshDrawCommand));

            auto *gc_l = context_ ? context_->GetGraphicsContext() : nullptr;
            auto *pool = gc_l ? gc_l->GetGlobalSSBOBufferRegistry() : nullptr;

            if (line_buffer_.geometry && pool && device_)
                line_buffer_.geometry->EnsureMeshDrawParams(pool, device_);

            const uint32_t geometry_id = line_buffer_.geometry ? line_buffer_.geometry->GetGeometryID() : 0;

            if (pool && geometry_id != 0)
            {
                graph::mtl::MeshDrawParams row{};
                row.index_base      = 0;
                row.vertex_base     = 0;
                row.is_indexed         = 0;
                row.vertex_count       = total_line_count_ * 2u;   // 绘制顶点数 = 线段数 × 2
                row.index_count        = 0;
                row.data_vertex_count  = static_cast<uint32_t>(line_buffer_.geometry->GetVertexCount());
                row.data_index_count   = static_cast<uint32_t>(line_buffer_.geometry->GetIndexCount());
                row.first_instance  = 0;

                if (auto *rdm = gc_l->GetSSBOBufferRegistry())
                {
                    const uint64_t null_addr = rdm->GetNullRowAddress();
                    row.addr_uv        = null_addr;
                    row.addr_ntb       = null_addr;
                    row.addr_luminance = null_addr;
                    row.addr_index     = null_addr;
                    row.addr_size      = null_addr;   // LineList 无宽度流

                    if (line_buffer_.geometry && device_)
                    {
                        auto fill_addr = [&](graph::VertexSemantic semantic, uint64_t &field)
                        {
                            if (auto *vab = line_buffer_.geometry->GetVAB(semantic))
                                field = device_->GetBufferDeviceAddressAligned16(vab->GetVkBuffer());
                        };

                        fill_addr(graph::VertexSemantic::Position,    row.addr_position);
                        fill_addr(graph::VertexSemantic::Color,       row.addr_color);
                        fill_addr(graph::VertexSemantic::TransformID, row.addr_transform_id);
                    }
                }

                pool->Write(geometry_id, row);
            }

            if (line_buffer_.mesh_draw_params)
            {
                auto *gpu = line_buffer_.mesh_draw_params->GetGPUBuffer();
                auto *cmd = gpu ? static_cast<graph::mtl::MeshDrawCommand *>(
                    gpu->Map(0, sizeof(graph::mtl::MeshDrawCommand))) : nullptr;

                if (cmd)
                {
                    cmd->geometry_id = geometry_id;
                    cmd->first_instance = 0;
                    gpu->Unmap();
                }
            }
        }
    }

    void SimpleLineRenderPipeline::Render(hgl::graph::RenderCmdBuffer* cmd)
    {
        if (!cmd)
        {
            GLogWarning("[SimpleLineRenderPipeline] Render skipped: cmd is null");
            return;
        }

        if (!initialized_)
        {
            GLogWarning("[SimpleLineRenderPipeline] Render skipped: pipeline not initialized");
            return;
        }

        if (total_line_count_ == 0)
        {
            GLogInfo("[SimpleLineRenderPipeline] Render skipped: total_line_count=0 (visible_components=%u total_components=%u)",
                     stats_.visible_components, stats_.total_components);
            return;
        }

        if (!pipeline_ || !material_)
        {
            GLogWarning("[SimpleLineRenderPipeline] Render skipped: invalid render resources (pipeline=%p material=%p)",
                        pipeline_, material_);
            return;
        }

        if (!current_line_buffer_)
        {
            GLogError("[SimpleLineRenderPipeline] Render skipped: no prepared frame buffer");
            return;
        }
        LineBuffer &line_buffer_ = *current_line_buffer_;

        cmd->BindPipeline(pipeline_);

        // EDS 1/2/3：pipeline 只保留 shader 部分——渲染侧动态应用管线状态。
        cmd->ApplyPipelineState(pipeline_config_);

        // 全局集（Scene/Bindless）绑定：每 cmd 首绑一次（守卫），重复调用跳过。
        if (auto* rc = context_ ? context_->GetRenderContext() : nullptr)
        {
            if (auto* gc = rc->GetGraphicsContext())
                gc->BindGlobalDescriptorSets(cmd, material_->GetPipelineLayout());
        }

        line_buffer_.material = material_;

        // RootAddresses push constant：单 draw（参数表 row 0，gl_DrawID=0）——
        // mesh shader 经 pc_root.addr_batch_mesh_draw_params 解引用参数表（A3-1）；
        // l2w 表地址（A3-2：palette 材质走 transform_id 流直查 l2w.mats[transform_id]）。
        graph::IGPUBuffer *l2w_gpu = nullptr;
        if (bound_transform_data_buffer_)
            l2w_gpu = bound_transform_data_buffer_->GetGPUBuffer();

        // 相机行号（槽号 * 槽总数 + 本帧槽）：着色器用它索引 cameras[]。
        const uint32_t active_cam_row = context_ ? context_->GetActiveCameraRow() : 0;

        // 全局地址表基址 = 本次 push 的根入口
        graph::GraphicsContext *addr_gc = context_ ? context_->GetGraphicsContext() : nullptr;
        if (!addr_gc)
            if (auto *addr_rc = context_ ? context_->GetRenderContext() : nullptr)
                addr_gc = addr_rc->GetGraphicsContext();
        // 表按帧槽多份：用本批次所属帧槽（= active_cam_row 的低位）取本槽表地址。
        const uint64_t addr_global_addresses =
            addr_gc ? addr_gc->GetGlobalAddressesAddress(
                          graph::GlobalAddressesSlotFromCameraRow(active_cam_row)) : 0;

        graph::PushRootAddresses(
            cmd,
            device_,
            material_->GetPipelineLayout(),
            addr_global_addresses,
            context_ ? context_->GetWorldAddressesAddress(context_->GetFrameIndex()) : 0,
            line_buffer_.mesh_draw_params
                ? line_buffer_.mesh_draw_params->GetGPUBuffer() : nullptr,
            l2w_gpu,
            nullptr,
            nullptr,
            0,
            nullptr,
            nullptr,
            nullptr,
            active_cam_row);

        line_buffer_.Draw(cmd);
    }

    void SimpleLineRenderPipeline::Shutdown()
    {
        bool has_gpu_resources = false;
        for (const auto &buffer : line_buffers_)
            if (buffer.geometry || buffer.mesh_draw_params)
                has_gpu_resources = true;

        if (has_gpu_resources)
        {
            if (!device_)
            {
                GLogError("[SimpleLineRenderPipeline] Shutdown cannot retire GPU resources without a Vulkan device");
                return;
            }

            if (!device_->WaitIdle())
            {
                GLogError("[SimpleLineRenderPipeline] Shutdown aborted: GPU resources could not be retired safely");
                return;
            }
        }

        current_line_buffer_ = nullptr;
        for (auto &buffer : line_buffers_)
            buffer.Clear();

        if (auto* gc = context_ ? context_->GetGraphicsContext() : nullptr)
        {
            auto* mat_mgr = gc->GetMaterialManager();
            if (mat_mgr)
            {
                if (material_)
                {
                    mat_mgr->Destroy(material_); material_ = nullptr;
                }
            }
        }

        pipeline_    = nullptr; // owned by RenderPass
        device_      = nullptr;
        initialized_ = false;
        prepared_frame_ = UINT32_MAX;
        collected_.Clear();
        stats_ = SimpleLineCollectStats{};
        total_line_count_ = 0;
        bound_transform_buffer_ = nullptr;
        bound_transform_data_buffer_ = nullptr;
    }

    void SimpleLineRenderPipeline::SyncTransformBinding()
    {
        if (!context_ || !material_)
            return;

        auto transform_system_sp = context_->GetSystem<TransformSystem>();
        auto* transform_system = transform_system_sp.get();
        if (!transform_system)
            return;

        transform_system->EnsureTransformBuffer();
        auto* transform_buffer = transform_system->GetTransformBuffer();
        if (!transform_buffer)
            return;

        auto* transform_data_buffer = transform_buffer->GetTransformDataBuffer();
        if (!transform_data_buffer)
        {
            GLogWarning("[SimpleLineRenderPipeline] SyncTransformBinding: transform_data_buffer is null");
            return;
        }

        const uint32_t static_count = transform_system->GetStaticCount();
        const uint32_t dynamic_count = transform_system->GetDynamicCount();
        transform_buffer->EnsureBudget(static_count, dynamic_count);

        // BDA（A5-2）：l2w 表地址经 pc_root.addr_l2w 由 Render 在 draw 前 push——
        // 仍缓存 buffer 供 Render 取地址（重建变化时 push 新地址）。
        if (transform_buffer != bound_transform_buffer_
         || transform_data_buffer != bound_transform_data_buffer_)
        {
            bound_transform_buffer_ = transform_buffer;
            bound_transform_data_buffer_ = transform_data_buffer;

            GLogInfo("[SimpleLineRenderPipeline] SyncTransformBinding: transform buffer changed (l2w addr pushed via pc_root)");
        }
    }

}  // namespace hgl::ecs
