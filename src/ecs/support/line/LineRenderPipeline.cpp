#include <hgl/ecs/support/line/LineRenderPipeline.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/components/LinesComponent.h>
#include <hgl/ecs/components/BoundingBoxComponent.h>
#include <hgl/ecs/components/VisibilityComponent.h>
#include <hgl/ecs/components/TransformComponent.h>
#include <hgl/ecs/support/TransformAssignmentBuffer.h>
#include <hgl/ecs/systems/tick/CameraSystem.h>
#include <hgl/ecs/systems/tick/TransformSystem.h>
#include <hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/render/RenderContext.h>
#include <hgl/graph/module/ShaderMaterialProgramManager.h>
#include <hgl/graph/module/ResourceDomainManager.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <hgl/graph/mesh/Primitive.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/UBOCommon.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/VKShaderMaterialProgram.h>
#include <hgl/vk/VKBuffer.h>
#include <hgl/vk/VKCommandBuffer.h>
#include <hgl/vk/VKRenderTarget.h>
#include <hgl/vk/VKVertexInputConfig.h>
#include <hgl/vk/UBOAccessor.h>
#include <hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include <hgl/vk/pipeline/VKGraphicsPipelineBuildRequest.h>
#include <hgl/math/geometry/Frustum.h>
#include <hgl/ecs/support/PipelineResolveMetrics.h>
#include <hgl/log/Log.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <limits>

namespace hgl::ecs
{
    namespace
    {
        PipelineResolveCounters g_line_pipeline_resolve_counters;
        PipelineHotpathCounters g_line_render_hotpath_counters;

        graph::ResourceDomain *ResolveDomainForMaterial(graph::GraphicsContext *graphics_context,
                                                        graph::ShaderMaterialProgram *material,
                                                        uint32_t domain_id)
        {
            if (!material)
                return nullptr;

            auto *rdm = graphics_context ? graphics_context->GetResourceDomainManager() : nullptr;
            if (!rdm)
                return nullptr;

            const auto schema = material->GetShaderDataSchema();
            if (auto *domain = rdm->Get(schema, domain_id))
                return domain;

            graph::ResourceDomainCreateInfo ci;
            ci.schema = schema;
            ci.domain_id = domain_id;
            ci.initial_capacity = 1024;
            return rdm->Create(ci);
        }
    }

    // -------------------------------------------------------------------------
    // Type aliases (local to this TU)
    // -------------------------------------------------------------------------
    using LineColorPalette    = uint32_t[LineRenderPipeline::PALETTE_SIZE];
    using UBOLineColorPalette = graph::UBOAccessor<LineColorPalette,graph::mtl::UBODescriptorSemantic::ColorPattle>;

    // -------------------------------------------------------------------------
    const std::string LineRenderPipeline::kName{ "Line" };

    // -------------------------------------------------------------------------
    // LineWidthSlot helpers
    // -------------------------------------------------------------------------

    void LineRenderPipeline::LineWidthSlot::Reset()
    {
        line_count = 0;
        bool pos_valid = va_pos.IsValid();
        bool color_valid = va_color.IsValid();

        GLogInfo("[LineRenderPipeline] Reset: pos_valid=%d color_valid=%d",
                 pos_valid ? 1 : 0,
             color_valid ? 1 : 0);

        if (pos_valid)   va_pos.Seek(0);
        if (color_valid) va_color.Seek(0);
        if (primitive)   primitive->SetDrawCounts(0);
    }

    void LineRenderPipeline::LineWidthSlot::Clear()
    {
        va_pos.Bind(nullptr);
        va_color.Bind(nullptr);
        SAFE_CLEAR(primitive);
        SAFE_CLEAR(geometry);
        line_count   = 0;
        gpu_capacity = 0;
    }

    bool LineRenderPipeline::LineWidthSlot::EnsureCapacity(
        uint32_t needed,
        graph::VulkanDevice*     dev,
        graph::MaterialBindingInstance* mi,
        graph::GraphicsPipeline*         p,
        uint32_t                 width)
    {
        if (needed <= gpu_capacity)
            return true;

        // Round up to granule
        const uint32_t new_cap = ((needed + LineRenderPipeline::LINES_GRANULE - 1)
                                          / LineRenderPipeline::LINES_GRANULE)
                                          * LineRenderPipeline::LINES_GRANULE;

        // Release old resources
        va_pos.Bind(nullptr);
        va_color.Bind(nullptr);
        SAFE_CLEAR(primitive);
        SAFE_CLEAR(geometry);

        // Create new geometry (2 verts per line)
        const graph::AnsiString name = graph::AnsiString("LineSlot_W") + graph::AnsiString::numberOf(width);
        graph::GeometryVertexFormat gvf;
        gvf.Set(graph::VAN::Position, VF_V3F);
        gvf.Set(graph::VAN::Color, VF_V1U8);
        geometry = graph::CreateGeometry(dev, gvf, name, new_cap * 2, 0,
                         graph::IndexType::AUTO, nullptr,
                         graph::BufferAllocPolicy::StagedUpload);
        if (!geometry)
            return false;

        primitive = graph::DirectCreatePrimitive(geometry, mi, p);
        if (!primitive)
        {
            SAFE_CLEAR(geometry);
            return false;
        }

        const int pos_idx   = geometry->GetVABIndex(graph::VAN::Position);
        const int color_idx = geometry->GetVABIndex(graph::VAN::Color);

        if (pos_idx < 0 || color_idx < 0)
        {
            SAFE_CLEAR(primitive);
            SAFE_CLEAR(geometry);
            return false;
        }

        va_pos.Bind(geometry->GetVAB(pos_idx));
        va_color.Bind(geometry->GetVAB(color_idx));

        GLogInfo("[LineRenderPipeline] Slot %u after Bind: pos_valid=%d color_valid=%d",
                 width,
                 va_pos.IsValid() ? 1 : 0,
                 va_color.IsValid() ? 1 : 0);

        if (!va_pos.IsValid() || !va_color.IsValid())
        {
            GLogWarning("[LineRenderPipeline] Slot %u accessor bind failed (pos_valid=%d color_valid=%d)",
                        width,
                        va_pos.IsValid() ? 1 : 0,
                        va_color.IsValid() ? 1 : 0);
            SAFE_CLEAR(primitive);
            SAFE_CLEAR(geometry);
            return false;
        }

        va_pos.Seek(0);
        va_color.Seek(0);

        gpu_capacity = new_cap;
        return true;
    }

    bool LineRenderPipeline::LineWidthSlot::AddSegment(
        const hgl::math::Vector3f& from,
        const hgl::math::Vector3f& to,
        uint8_t                     color_index)
    {
        bool pos_valid = va_pos.IsValid();
        bool color_valid = va_color.IsValid();

        if (!pos_valid || !color_valid)
        {
            GLogWarning("[LineRenderPipeline] AddSegment accessor invalid: pos=%d color=%d",
                        pos_valid ? 1 : 0,
                        color_valid ? 1 : 0);
            return false;
        }

        if (!va_pos.Write(from))
            return false;
        if (!va_pos.Write(to))
            return false;
        if (!va_color.Write(color_index))
            return false;
        if (!va_color.Write(color_index))
            return false;

        ++line_count;
        if (primitive)
            primitive->SetDrawCounts(line_count * 2);
        return true;
    }

    void LineRenderPipeline::LineWidthSlot::Draw(graph::RenderCmdBuffer* cmd)
    {
        if (line_count == 0 || !primitive)
            return;

        const graph::GeometryDataBuffer *data_buffer = primitive->GetDataBuffer();
        cmd->BindDataBuffer(data_buffer);

        cmd->Draw(primitive->GetDataBuffer(), primitive->GetRenderData());
    }

    // -------------------------------------------------------------------------
    // LineRenderPipeline
    // -------------------------------------------------------------------------

    LineRenderPipeline::LineRenderPipeline(ECSContext* context)
        : context_(context)
    {
        // Initialize palette to white by default
        std::fill(std::begin(palette_), std::end(palette_), 0xFFFFFFFF);
        GLogInfo(OS_TEXT("[LineRenderPipeline] Constructor: initialized palette_ to all white"));
    }

    LineRenderPipeline::~LineRenderPipeline()
    {
        Shutdown();
    }

    const std::string& LineRenderPipeline::GetName() const { return kName; }
    ECSContext*         LineRenderPipeline::GetWorld() const { return context_; }

    bool LineRenderPipeline::Initialize()
    {
        if (initialized_)
            return true;

        GLogInfo(OS_TEXT("[LineRenderPipeline] Initialize: START"));

        auto* gc = context_ ? context_->GetGraphicsContext() : nullptr;
        if (!gc)
        {
            if (auto* rc = context_ ? context_->GetRenderContext() : nullptr)
                gc = rc->GetGraphicsContext();
        }

        if (!gc)
            return false;

        device_ = gc->GetDevice();
        if (!device_)
            return false;

        support_wide_lines_ = device_->GetDevAttr() && device_->GetDevAttr()->wide_lines;

        // ------- Create material -------
        graph::mtl::Material3DCreateConfig cfg(
            graph::PrimitiveType::Lines,
            graph::mtl::IncludeCamera::With,
            graph::mtl::IncludeL2W::With,
            graph::mtl::IncludeSky::Without);

        auto* mat_mgr = gc->GetMaterialManager();
        if (!mat_mgr)
            return false;

        material_ = mat_mgr->ResolveOrCreateProgram(graph::mtl::MaterialPreset::VertexPattleColor3D, &cfg);
        if (!material_)
            return false;

        if (auto rdbs = context_->GetSystem<RenderDescriptorBindingSystem>())
            rdbs->RegisterPipelineMaterial(material_);

        // ------- Create material instance -------
        graph::VILConfig vil;
        vil.Add(graph::VAN::Color, VF_V1U8);
        const auto preset = support_wide_lines_
            ? graph::GraphicsPipelinePreset::DynamicLineWidth3D
            : graph::GraphicsPipelinePreset::Solid3D;

        graph::MaterialInstanceSpec spec;
        spec.material = material_;
        spec.vil_cfg = &vil;
        spec.preset = preset;
        spec.domain = ResolveDomainForMaterial(gc, material_, 3001u);
        mi_ = mat_mgr->AcquireMaterialInstance(spec);
        if (!mi_)
            return false;

        // ------- Create color palette UBO -------
        auto* buf_mgr = gc->GetBufferManager();
        if (!buf_mgr)
            return false;

        auto* ubo = buf_mgr->CreateUBO<UBOLineColorPalette>();
        if (!ubo)
            return false;

        ubo_color_   = ubo;
        ubo_raw_buf_ = ubo->GetBuffer();

        // Bind UBO to material
        material_->BindUBO(ubo);
        material_->Update();

        // Flush current palette to UBO (palette initialized in constructor)
        FlushPaletteToGPU();

        SyncTransformBinding();

        initialized_ = true;

        return true;
    }

    bool LineRenderPipeline::ResolvePipelineForCurrentRenderTarget()
    {
        if (!context_ || !device_ || !material_ || !mi_)
            return false;

        auto* render_target = context_->GetRenderTarget();
        if (!render_target)
        {
            if (auto* render_context = context_->GetRenderContext())
                render_target = render_context->GetCurrentRenderTarget();
        }

        auto* render_format = render_target ? render_target->GetRenderFormat() : nullptr;
        if (!render_format)
            return false;

        if (pipeline_ && render_format_ == render_format)
            return true;

        RecordPipelineResolveAttempt(g_line_pipeline_resolve_counters);
        const uint64_t vkcreate_before = graph::RenderTargetFormat::GetVkCreateCount();

        const graph::GraphicsPipelinePreset preset = mi_->GetRenderPreset();
        const graph::GraphicsPipelineData* pipeline_data = graph::GetGraphicsPipelineData(preset);
        if (!pipeline_data)
        {
            const uint64_t failures = RecordPipelineResolveFailure(g_line_pipeline_resolve_counters);
            if (ShouldLogPow2(failures))
            {
                GLogWarning("[LineRenderPipeline] Missing GraphicsPipelineData for preset=%u, total_failures=%llu",
                            static_cast<unsigned int>(preset),
                            static_cast<unsigned long long>(failures));
            }
            return false;
        }

        graph::GraphicsPipelineBuildRequest req;
        req.material = material_;
        req.vil = mi_->GetVIL();
        req.render_format = render_format;
        req.pipeline_data = pipeline_data;
        req.primitive = material_->GetPrimitiveType();
        req.primitive_restart = (pipeline_data->input_assembly.primitiveRestartEnable == VK_TRUE);

        graph::GraphicsPipeline* resolved = device_->AcquireGraphicsPipeline(req);
        const uint64_t vkcreate_after = graph::RenderTargetFormat::GetVkCreateCount();
        const uint64_t vkcreate_delta = vkcreate_after - vkcreate_before;

        if (!resolved)
        {
            const uint64_t failures = RecordPipelineResolveFailure(g_line_pipeline_resolve_counters);
            if (ShouldLogPow2(failures))
            {
                GLogWarning("[LineRenderPipeline] GraphicsPipeline resolve failed: total_failures=%llu",
                            static_cast<unsigned long long>(failures));
            }
            return false;
        }

        pipeline_ = resolved;
        render_format_ = render_format;

        const uint32_t num_slots = support_wide_lines_ ? MAX_WIDTHS : 1;
        for (uint32_t i = 0; i < num_slots; ++i)
        {
            if (!slots_[i].primitive)
                continue;
            // Primitive pipeline state now completely decoupled; cached in pipeline_ member
        }

        RecordPipelineResolveSuccess(g_line_pipeline_resolve_counters);
        if (ShouldLogPipelineResolveCreated(vkcreate_delta))
        {
            GLogInfo("[LineRenderPipeline] GraphicsPipeline resolve created vk pipelines=%llu (attempts=%llu successes=%llu failures=%llu)",
                     static_cast<unsigned long long>(vkcreate_delta),
                     static_cast<unsigned long long>(g_line_pipeline_resolve_counters.attempts.load()),
                     static_cast<unsigned long long>(g_line_pipeline_resolve_counters.successes.load()),
                     static_cast<unsigned long long>(g_line_pipeline_resolve_counters.failures.load()));
        }

        return true;
    }

    bool LineRenderPipeline::PrepareFrame()
    {
        if (!context_)
            return false;

        const uint32_t frame = context_->GetFrameIndex();
        if (frame == prepared_frame_)
            return true;
        prepared_frame_ = frame;

        if (!initialized_ && !Initialize())
            return false;

        if (!ResolvePipelineForCurrentRenderTarget())
            return false;

        SyncTransformBinding();

        // Reset all active slots
        if (support_wide_lines_)
        {
            for (uint32_t i = 0; i < MAX_WIDTHS; ++i)
                slots_[i].Reset();
        }
        else
        {
            slots_[0].Reset();
        }

        collected_.clear();
        stats_ = LineCollectStats{};
        total_line_count_ = 0;

        return true;
    }

    uint32_t LineRenderPipeline::GetSlotIndex(uint8_t width) const
    {
        if (!support_wide_lines_)
            return 0;
        if (width == 0)
            return 0;
        uint32_t idx = static_cast<uint32_t>(width) - 1;
        return idx < MAX_WIDTHS ? idx : (MAX_WIDTHS - 1);
    }

    void LineRenderPipeline::RunCollect()
    {
        if (!PrepareFrame())
            return;

        // Build frustum from camera
        bool frustum_valid = false;
        hgl::math::Frustum frustum;

        auto camera_system = context_->GetSystem<CameraSystem>();
        if (camera_system)
        {
            const auto* cam = camera_system->GetCameraInfo();
            if (cam)
            {
                frustum.SetMatrix(cam->vp);
                frustum_valid = true;
            }
        }

        std::vector<std::shared_ptr<LinesComponent>> all;
        context_->GetComponents<LinesComponent>(all);

        for (const auto& comp : all)
        {
            if (!comp) continue;
            ++stats_.total_components;

            if (!comp->visible || comp->lines.empty())
            {
                ++stats_.culled_by_visibility;
                continue;
            }

            Entity* owner = comp->GetOwner();
            if (!owner) continue;

            if (context_ && !context_->IsEntityRenderEnabled(owner))
                continue;

            // VisibilityComponent check
            if (auto vis = owner->GetComponent<VisibilityComponent>())
            {
                if (!vis->IsVisible())
                {
                    ++stats_.culled_by_visibility;
                    continue;
                }
            }

            // Frustum cull
            if (frustum_valid)
            {
                if (auto bbox = owner->GetComponent<BoundingBoxComponent>())
                {
                    if (bbox->HasWorldAABB())
                    {
                        const auto& aabb   = bbox->GetWorldAABB();
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
            }

            collected_.push_back(comp);
            ++stats_.visible_components;
        }
    }

    void LineRenderPipeline::RunBuild()
    {
        if (!initialized_ || collected_.empty())
            return;

        std::shared_ptr<TransformSystem> transform_system_sp = context_ ? context_->GetSystem<TransformSystem>() : nullptr;
        TransformSystem* transform_system = transform_system_sp.get();
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
        }

        // First pass: count lines per slot
        uint32_t slot_counts[MAX_WIDTHS] = {};
        uint32_t expected_total = 0;
        for (const auto& comp : collected_)
        {
            const uint32_t idx = GetSlotIndex(comp->width);
            const uint32_t cnt = static_cast<uint32_t>(comp->lines.size());
            slot_counts[idx] += cnt;
            expected_total += cnt;
        }

        // Ensure GPU capacity per slot (recreate if needed)
        const uint32_t num_slots = support_wide_lines_ ? MAX_WIDTHS : 1;
        for (uint32_t i = 0; i < num_slots; ++i)
        {
            if (slot_counts[i] == 0)
                continue;
            if (!slots_[i].EnsureCapacity(slot_counts[i], device_, mi_, pipeline_, i + 1))
            {
                GLogWarning("[LineRenderPipeline] EnsureCapacity failed: slot=%u need=%u cap=%u",
                            i + 1,
                            slot_counts[i],
                            slots_[i].gpu_capacity);
                return; // Allocation failure: skip frame
            }

            if (slots_[i].gpu_capacity < slot_counts[i])
            {
                GLogWarning("[LineRenderPipeline] Slot capacity insufficient after ensure: slot=%u cap=%u need=%u",
                            i + 1,
                            slots_[i].gpu_capacity,
                            slot_counts[i]);
            }

            slots_[i].Reset(); // seek back to 0
        }

        // Second pass: write segments
        uint32_t write_fail_count = 0;
        for (const auto& comp : collected_)
        {
            const uint32_t idx = GetSlotIndex(comp->width);
            bool comp_write_ok = true;

            Entity* owner = comp->GetOwner();
            glm::mat4 world_matrix(1.0f);
            bool has_world_transform = false;
            if (owner)
            {
                if (auto transform = owner->GetComponent<TransformComponent>())
                {
                    world_matrix = transform->GetWorldMatrix();
                    has_world_transform = true;
                }
            }

            for (const auto& seg : comp->lines)
            {
                hgl::math::Vector3f from = seg.from;
                hgl::math::Vector3f to = seg.to;

                if (has_world_transform)
                {
                    const glm::vec4 world_from = world_matrix * glm::vec4(seg.from.x, seg.from.y, seg.from.z, 1.0f);
                    const glm::vec4 world_to = world_matrix * glm::vec4(seg.to.x, seg.to.y, seg.to.z, 1.0f);
                    from = hgl::math::Vector3f(world_from.x, world_from.y, world_from.z);
                    to = hgl::math::Vector3f(world_to.x, world_to.y, world_to.z);
                }

                if (!slots_[idx].AddSegment(from, to, seg.color_index))
                {
                    ++write_fail_count;
                    comp_write_ok = false;
                }
            }

            if (comp_write_ok)
                comp->MarkSynced();
        }

        // Tally total
        total_line_count_ = 0;
        for (uint32_t i = 0; i < num_slots; ++i)
            total_line_count_ += slots_[i].line_count;

        if (write_fail_count > 0 || total_line_count_ != expected_total)
        {
            GLogWarning("[LineRenderPipeline] Build mismatch: expected=%u built=%u write_fail=%u collected_components=%zu",
                        expected_total,
                        total_line_count_,
                        write_fail_count,
                        collected_.size());

            for (uint32_t i = 0; i < num_slots; ++i)
            {
                if (slot_counts[i] == 0 && slots_[i].line_count == 0)
                    continue;

                GLogWarning("[LineRenderPipeline]   slot=%u expected=%u built=%u capacity=%u pos_valid=%d color_valid=%d",
                            i + 1,
                            slot_counts[i],
                            slots_[i].line_count,
                            slots_[i].gpu_capacity,
                            slots_[i].va_pos.IsValid() ? 1 : 0,
                            slots_[i].va_color.IsValid() ? 1 : 0);
            }
        }

    }

    void LineRenderPipeline::FlushPaletteToGPU()
    {
        if (!palette_dirty_ || !ubo_color_)
        {
            GLogInfo(OS_TEXT("[LineRenderPipeline] FlushPaletteToGPU: skipped - dirty=%d ubo=%p"),
                     palette_dirty_ ? 1 : 0, ubo_color_);
            return;
        }

        auto* ubo = static_cast<UBOLineColorPalette*>(ubo_color_);

        GLogInfo(OS_TEXT("[LineRenderPipeline] FlushPaletteToGPU: writing %d colors directly from palette_"), PALETTE_SIZE);

        // Write directly from palette_ array to GPU buffer (not via mapped_data)
        // This ensures actual data transfer instead of no-op when source == destination
        bool write_ok = ubo->Write(palette_, 0, sizeof(LineColorPalette));

        GLogInfo(OS_TEXT("[LineRenderPipeline] FlushPaletteToGPU: Write result=%d size=%zu bytes"),
                 write_ok ? 1 : 0, sizeof(LineColorPalette));

        palette_dirty_ = false;
    }

    void LineRenderPipeline::Render(hgl::graph::RenderCmdBuffer* cmd)
    {
        if (!cmd || !initialized_ || total_line_count_ == 0)
            return;

        if (!pipeline_ || !mi_)
            return;

        const uint64_t vkcreate_before = graph::RenderTargetFormat::GetVkCreateCount();

        auto* mat = mi_->GetShaderMaterialProgram();
        if (mat)
            cmd->BindDescriptorSets(mat);

        cmd->BindPipeline(pipeline_);

        const uint32_t num_slots = support_wide_lines_ ? MAX_WIDTHS : 1;
        uint32_t draw_lines = 0;
        for (uint32_t i = 0; i < num_slots; ++i)
        {
            if (slots_[i].line_count == 0)
                continue;

            if (support_wide_lines_)
                cmd->SetLineWidth(static_cast<float>(i + 1));

            slots_[i].Draw(cmd);
            draw_lines += slots_[i].line_count;
        }

        if (draw_lines != total_line_count_)
        {
            GLogWarning("[LineRenderPipeline] Render mismatch: total_line_count=%u draw_lines=%u",
                        total_line_count_,
                        draw_lines);
        }

        const uint64_t vkcreate_after = graph::RenderTargetFormat::GetVkCreateCount();
        const uint64_t vkcreate_delta = vkcreate_after - vkcreate_before;
        const uint64_t violation_log_count = RecordPipelineHotpathViolationAndGetLogCount(vkcreate_delta,
                                                                                           g_line_render_hotpath_counters);
        if (violation_log_count > 0)
        {
            GLogWarning("[LineRenderPipeline] Stage-4 violation: render hot path created %llu vk pipeline(s), total_violations=%llu",
                        static_cast<unsigned long long>(vkcreate_delta),
                        static_cast<unsigned long long>(violation_log_count));
        }
    }

    void LineRenderPipeline::Shutdown()
    {
        const uint32_t num_slots = support_wide_lines_ ? MAX_WIDTHS : 1;
        for (uint32_t i = 0; i < num_slots; ++i)
            slots_[i].Clear();

        if (auto* gc = context_ ? context_->GetGraphicsContext() : nullptr)
        {
            auto* mat_mgr = gc->GetMaterialManager();
            if (mat_mgr)
            {
                if (mi_)       { mat_mgr->Destroy(mi_); mi_ = nullptr; }
                if (material_)
                {
                    if (auto rdbs = context_->GetSystem<RenderDescriptorBindingSystem>())
                        rdbs->UnregisterPipelineMaterial(material_);
                    mat_mgr->Destroy(material_); material_ = nullptr;
                }
            }

            if (ubo_color_)
            {
                auto* ubo = static_cast<UBOLineColorPalette*>(ubo_color_);
                auto* raw = ubo->GetBuffer();
                delete ubo;
                ubo_color_   = nullptr;
                ubo_raw_buf_ = nullptr;

                if (raw && gc->GetBufferManager())
                    gc->GetBufferManager()->Release(raw);
            }
        }

        pipeline_    = nullptr;
        render_format_ = nullptr;
        device_      = nullptr;
        initialized_ = false;
        bound_transform_buffer_ = nullptr;
        bound_transform_data_buffer_ = nullptr;
    }

    void LineRenderPipeline::SetPaletteColor(int index, const hgl::Color4f& color)
    {
        GLogInfo(OS_TEXT("[LineRenderPipeline] SetPaletteColor: index=%d color=(%.2f,%.2f,%.2f,%.2f) initialized=%d"),
                 index, color.r, color.g, color.b, color.a, initialized_ ? 1 : 0);

        if (index < 0 || index >= static_cast<int>(PALETTE_SIZE))
        {
            GLogWarning(OS_TEXT("[LineRenderPipeline] SetPaletteColor: index %d out of range"), index);
            return;
        }

        palette_[index]  = color.toABGR8();
        palette_dirty_   = true;

        // Flush immediately if pipeline is initialized
        if (initialized_)
        {
            GLogInfo(OS_TEXT("[LineRenderPipeline] SetPaletteColor: calling FlushPaletteToGPU immediately"));
            FlushPaletteToGPU();
        }
        else
        {
            GLogInfo(OS_TEXT("[LineRenderPipeline] SetPaletteColor: pipeline not initialized yet, deferring flush"));
        }
    }

    void LineRenderPipeline::SyncTransformBinding()
    {
        if (!context_ || !material_)
            return;

        std::shared_ptr<TransformSystem> transform_system_sp = context_->GetSystem<TransformSystem>();
        auto* transform_system = transform_system_sp.get();
        if (!transform_system)
            return;

        transform_system->EnsureTransformBuffer();
        auto* transform_buffer = transform_system->GetTransformBuffer();
        if (!transform_buffer)
            return;

        const uint32_t static_count = transform_system->GetStaticCount();
        const uint32_t dynamic_count = transform_system->GetDynamicCount();
        transform_buffer->EnsureCapacity(static_count, dynamic_count, graph::BufferAllocPolicy::Auto);

        auto* transform_data_buffer = transform_buffer->GetTransformDataBuffer();
        if (!transform_data_buffer)
        {
            GLogWarning("[LineRenderPipeline] SyncTransformBinding: transform_data_buffer is null after EnsureCapacity");
            return;
        }

        auto *gpu = transform_data_buffer->GetGPUBuffer();
        GLogInfo("[LineRenderPipeline] SyncTransformBinding snapshot: tab=0x%llX dbuf=0x%llX vk=0x%llX gpu=0x%llX size=%llu dirty=%d static=%u dynamic=%u",
                 static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(transform_buffer)),
                 static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(transform_data_buffer)),
                 static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(transform_data_buffer->GetBuffer())),
                 static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(gpu)),
                 static_cast<unsigned long long>(transform_data_buffer->GetSize()),
                 gpu ? (gpu->IsDirty() ? 1 : 0) : -1,
                 static_count,
                 dynamic_count);

        if (transform_buffer != bound_transform_buffer_
         || transform_data_buffer != bound_transform_data_buffer_)
        {
            transform_buffer->BindTransform(material_);
            material_->Update();
            bound_transform_buffer_ = transform_buffer;
            bound_transform_data_buffer_ = transform_data_buffer;

            GLogInfo("[LineRenderPipeline] SyncTransformBinding: bound transform buffer for Line material");
        }
    }

}  // namespace hgl::ecs
