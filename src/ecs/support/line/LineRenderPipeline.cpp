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
#include <hgl/graph/module/MaterialManager.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/DescriptorBindingSet.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <hgl/graph/mesh/GeometryDataBuffer.h>
#include <hgl/graph/mesh/GeometryDrawRange.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/MaterialLibrary.h>
#include <hgl/mtl/UBOCommon.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/VKShaderProgram.h>
#include <hgl/vk/VKBuffer.h>
#include <hgl/vk/VKCommandBuffer.h>
#include <hgl/vk/VKRenderTarget.h>
#include <hgl/vk/VKRenderAssign.h>
#include <hgl/vk/VKVertexInputConfig.h>
#include <hgl/vk/VKVABList.h>
#include <hgl/vk/StructuredBufferAccessor.h>
#include <hgl/math/geometry/Frustum.h>
#include <hgl/log/Log.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <limits>

namespace hgl::ecs
{
    namespace
    {
        graph::GeometryVertexFormat CreateLineGeometryVertexFormat()
        {
            graph::GeometryVertexFormat gvf;

            gvf.Add(graph::VertexSemantic::Position, VK_FORMAT_R32G32B32_SFLOAT, 3, sizeof(float) * 3);
            gvf.Add(graph::VertexSemantic::Color, VK_FORMAT_R8_UINT, 1, sizeof(uint8_t));
            gvf.Add(graph::Assign::TransformID::VIS_SEMANTIC,
                    graph::Assign::TransformID::VAB_FMT,
                    1,
                    graph::Assign::TransformID::STRIDE_BYTES);

            return gvf;
        }
    }

    // -------------------------------------------------------------------------
    // Type aliases (local to this TU)
    // -------------------------------------------------------------------------
    using LineColorPalette    = graph::Color4f[LineRenderPipeline::PALETTE_SIZE];
    using UBOLineColorPalette = graph::StructuredBufferAccessor<LineColorPalette>;

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
        bool transform_valid = va_transform.IsValid();
        
        GLogInfo("[LineRenderPipeline] Reset: pos_valid=%d color_valid=%d transform_valid=%d",
                 pos_valid ? 1 : 0,
             color_valid ? 1 : 0,
             transform_valid ? 1 : 0);
        
        if (pos_valid)   va_pos.Seek(0);
        if (color_valid) va_color.Seek(0);
        if (transform_valid) va_transform.Seek(0);
        if (draw_range)  draw_range->vertex_count = 0;
    }

    void LineRenderPipeline::LineWidthSlot::Clear()
    {
        va_pos.Bind(nullptr);
        va_color.Bind(nullptr);
        va_transform.Bind(nullptr);
        SAFE_CLEAR(data_buffer);
        SAFE_CLEAR(draw_range);
        SAFE_CLEAR(geometry);
        line_count   = 0;
        gpu_capacity = 0;
    }

    bool LineRenderPipeline::LineWidthSlot::EnsureCapacity(
        uint32_t needed,
        graph::VulkanDevice*     dev,
        graph::DescriptorBindingSet* binding_set,
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
        SAFE_CLEAR(data_buffer);
        SAFE_CLEAR(draw_range);
        SAFE_CLEAR(geometry);

        // Create new geometry (2 verts per line)
        const graph::AnsiString name = graph::AnsiString("LineSlot_W") + graph::AnsiString::numberOf(width);
        geometry = graph::CreateGeometry(dev,
                         CreateLineGeometryVertexFormat(),
                         name, new_cap * 2, 0,
                         graph::IndexType::AUTO, nullptr,
                         graph::BufferAllocPolicy::StagedUpload);
        if (!geometry)
            return false;

        if (!binding_set || !binding_set->GetVIL())
        {
            GLogError("[LineRenderPipeline] EnsureCapacity failed: binding_set/vil is null");
            SAFE_CLEAR(geometry);
            return false;
        }

        data_buffer = new graph::GeometryDataBuffer(binding_set->GetVIL()->GetVertexAttribCount(),
                                                    geometry->GetIBO(),
                                                    geometry->GetVDM());
        if (!data_buffer
         || !data_buffer->Update(geometry,
                                 binding_set->GetVIL()->GetVIFList(),
                                 binding_set->GetVIL()->GetVertexAttribCount()))
        {
            GLogError("[LineRenderPipeline] GeometryDataBuffer::Update failed");
            SAFE_CLEAR(data_buffer);
            SAFE_CLEAR(geometry);
            return false;
        }

        draw_range = new graph::GeometryDrawRange();
        if (!draw_range)
        {
            SAFE_CLEAR(data_buffer);
            SAFE_CLEAR(geometry);
            return false;
        }
        draw_range->Set(geometry);
        draw_range->vertex_count = 0;

        const int pos_idx   = geometry->GetVABIndex(graph::VertexSemantic::Position);
        const int color_idx = geometry->GetVABIndex(graph::VertexSemantic::Color);
        const int transform_idx = geometry->GetVABIndex(graph::Assign::TransformID::VIS_SEMANTIC);

        if (pos_idx < 0 || color_idx < 0 || transform_idx < 0)
        {
            SAFE_CLEAR(data_buffer);
            SAFE_CLEAR(draw_range);
            SAFE_CLEAR(geometry);
            return false;
        }

        va_pos.Bind(geometry->GetVAB(pos_idx));
        va_color.Bind(geometry->GetVAB(color_idx));
        va_transform.Bind(geometry->GetVAB(transform_idx));

        GLogInfo("[LineRenderPipeline] Slot %u after Bind: pos_valid=%d color_valid=%d transform_valid=%d",
                 width,
                 va_pos.IsValid() ? 1 : 0,
                 va_color.IsValid() ? 1 : 0,
                 va_transform.IsValid() ? 1 : 0);

        if (!va_pos.IsValid() || !va_color.IsValid() || !va_transform.IsValid())
        {
            GLogWarning("[LineRenderPipeline] Slot %u accessor bind failed (pos_valid=%d color_valid=%d transform_valid=%d)",
                        width,
                        va_pos.IsValid() ? 1 : 0,
                        va_color.IsValid() ? 1 : 0,
                        va_transform.IsValid() ? 1 : 0);
            SAFE_CLEAR(data_buffer);
            SAFE_CLEAR(draw_range);
            SAFE_CLEAR(geometry);
            return false;
        }

        va_pos.Seek(0);
        va_color.Seek(0);
        va_transform.Seek(0);

        gpu_capacity = new_cap;
        return true;
    }

    bool LineRenderPipeline::LineWidthSlot::AddSegment(
        const hgl::math::Vector3f& from,
        const hgl::math::Vector3f& to,
        uint8_t                     color_index,
        graph::Assign::TransformID::ValueType transform_index)
    {
        bool pos_valid = va_pos.IsValid();
        bool color_valid = va_color.IsValid();
        bool transform_valid = va_transform.IsValid();
        
        if (!pos_valid || !color_valid || !transform_valid)
        {
            GLogWarning("[LineRenderPipeline] AddSegment accessor invalid: pos=%d color=%d transform=%d",
                        pos_valid ? 1 : 0,
                        color_valid ? 1 : 0,
                        transform_valid ? 1 : 0);
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
        if (!va_transform.Write(transform_index))
            return false;
        if (!va_transform.Write(transform_index))
            return false;

        ++line_count;
        if (draw_range)
            draw_range->vertex_count = line_count * 2;
        return true;
    }

    void LineRenderPipeline::LineWidthSlot::Draw(graph::RenderCmdBuffer* cmd)
    {
        if (!cmd)
        {
            GLogWarning("[LineRenderPipeline] Draw skipped: cmd is null");
            return;
        }

        if (line_count == 0)
            return;

        if (!data_buffer || !draw_range)
        {
            GLogWarning("[LineRenderPipeline] Draw skipped: data_buffer/draw_range null while line_count=%u", line_count);
            return;
        }

        bool bound_ok = false;

        if (data_buffer && geometry)
        {
            const int transform_idx = geometry->GetVABIndex(graph::Assign::TransformID::VIS_SEMANTIC);
            const VkBuffer transform_vk = transform_idx >= 0 ? geometry->GetVkBuffer(transform_idx) : VK_NULL_HANDLE;

            if (transform_vk != VK_NULL_HANDLE && data_buffer->vab_count > 0)
            {
                graph::VABList vab_list(data_buffer->vab_count + 1);
                const bool base_ok = vab_list.Add(data_buffer);
                const bool tid_ok = vab_list.Add(transform_vk, 0);
                bound_ok = base_ok && tid_ok && cmd->BindVAB(&vab_list);

                if (!bound_ok)
                {
                    GLogWarning("[LineRenderPipeline] Draw fallback bind failed: line_count=%u base_count=%u transform_idx=%d vk=0x%llX",
                                line_count,
                                data_buffer->vab_count,
                                transform_idx,
                                static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(transform_vk)));
                }
            }
        }

        if (!bound_ok)
            cmd->BindDataBuffer(data_buffer);

        cmd->Draw(data_buffer, draw_range);

        GLogInfo("[LineRenderPipeline] Draw issued: data_buffer=%p draw_range=%p line_count=%u vertex_count=%u bound_with_tid=%d",
                 data_buffer,
                 draw_range,
                 line_count,
                 line_count * 2u,
                 bound_ok ? 1 : 0);
    }

    // -------------------------------------------------------------------------
    // LineRenderPipeline
    // -------------------------------------------------------------------------

    LineRenderPipeline::LineRenderPipeline(ECSContext* context)
        : context_(context)
    {
        // Initialize palette to white by default
        std::fill(std::begin(palette_), std::end(palette_), hgl::Color4f(1.0f, 1.0f, 1.0f, 1.0f));
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
        GLogInfo(OS_TEXT("[LineRenderPipeline] Initialize: palette_[0]=(%.2f,%.2f,%.2f,%.2f) palette_[1]=(%.2f,%.2f,%.2f,%.2f)"),
                 palette_[0].r, palette_[0].g, palette_[0].b, palette_[0].a,
                 palette_[1].r, palette_[1].g, palette_[1].b, palette_[1].a);

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

        support_wide_lines_ = device_->GetDevAttr() && device_->GetDevAttr()->wide_lines;

        // ------- Create material -------
        graph::mtl::MaterialRecipe recipe{};
        recipe.mtl_def_id = "VertexPattleColor3D";

        auto* mat_mgr = gc->GetMaterialManager();
        if (!mat_mgr)
            return false;

        {
            graph::mtl::MaterialDefinitionBuildRequest mtl_request{};
            mtl_request.recipe = recipe;
            mtl_request.primitive_type = graph::PrimitiveType::Lines;
            material_ = mat_mgr->AcquireMaterialProgram(mtl_request);
        }
        if (!material_)
            return false;

        if (auto rdbs = context_->GetSystem<RenderDescriptorBindingSystem>())
            rdbs->RegisterPipelineMaterial(material_);

        const graph::GeometryVertexFormat line_gvf = CreateLineGeometryVertexFormat();

        // ------- Create descriptor binding set -------
        binding_vil_ = material_->CreateVIL(line_gvf);
        if (!binding_vil_)
            return false;

        binding_set_storage_.SetMaterial(material_);
        binding_set_storage_.SetVIL(binding_vil_);
        binding_set_ = &binding_set_storage_;

        // ------- Create pipeline -------
        pipeline_ = support_wide_lines_
            ? rp->CreatePipeline(material_, binding_vil_, graph::PipelinePreset::DynamicLineWidth3D, false, &line_gvf)
            : rp->CreatePipeline(material_, binding_vil_, graph::PipelinePreset::Solid3D, false, &line_gvf);

        if (!pipeline_)
            return false;

        // ------- Create color palette UBO -------
        auto* buf_mgr = gc->GetBufferManager();
        if (!buf_mgr)
            return false;

        auto* raw_buf = buf_mgr->CreateUBO("LineColorPaletteUBO_ECS",
                                            graph::StructuredBufferAccessor<LineColorPalette>::GetSize());
        if (!raw_buf)
            return false;
        raw_buf->SetUpdateClass(graph::BufferUpdateClass::Default);

        auto* ubo = graph::StructuredBufferAccessor<LineColorPalette>::Create(
                        raw_buf, &graph::mtl::SBS_ColorPattle, false);
        if (!ubo)
            return false;

        ubo_color_   = ubo;
        ubo_raw_buf_ = raw_buf;

        // Bind UBO to material
        material_->BindUBO(&graph::mtl::SBS_ColorPattle, ubo->GetGPUBuffer());
        material_->Update();

        // Flush current palette to UBO (palette initialized in constructor)
        FlushPaletteToGPU();

        SyncTransformBinding();

        initialized_ = true;
        
        GLogInfo(OS_TEXT("[LineRenderPipeline] Initialize: COMPLETE, palette_[0]=(%.2f,%.2f,%.2f,%.2f)"),
                 palette_[0].r, palette_[0].g, palette_[0].b, palette_[0].a);
        
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

        GLogInfo("[LineRenderPipeline] Collect summary: total=%u visible=%u collected=%zu culled_visibility=%u culled_frustum=%u culled_hzb=%u",
                 stats_.total_components,
                 stats_.visible_components,
                 collected_.size(),
                 stats_.culled_by_visibility,
                 stats_.culled_by_frustum,
                 stats_.culled_by_hzb);
    }

    void LineRenderPipeline::RunBuild()
    {
        if (!initialized_)
        {
            GLogWarning("[LineRenderPipeline] RunBuild skipped: pipeline not initialized");
            return;
        }

        if (collected_.empty())
        {
            GLogInfo("[LineRenderPipeline] RunBuild skipped: no collected line components");
            return;
        }

        std::shared_ptr<TransformSystem> transform_system_sp = context_ ? context_->GetSystem<TransformSystem>() : nullptr;
        TransformSystem* transform_system = transform_system_sp.get();
        uint32_t static_count = 0;
        uint32_t dynamic_count = 0;
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

            static_count = transform_system->GetStaticCount();
            dynamic_count = transform_system->GetDynamicCount();
            dynamic_base = transform_system->GetDynamicBaseIndex(static_count, dynamic_count);
        }

        // First pass: count lines per slot
        uint32_t slot_counts[MAX_WIDTHS] = {};
        uint32_t expected_total = 0;
        uint32_t non_empty_slots = 0;
        for (const auto& comp : collected_)
        {
            const uint32_t idx = GetSlotIndex(comp->width);
            const uint32_t cnt = static_cast<uint32_t>(comp->lines.size());
            if (slot_counts[idx] == 0 && cnt > 0)
                ++non_empty_slots;
            slot_counts[idx] += cnt;
            expected_total += cnt;
        }

        // Ensure GPU capacity per slot (recreate if needed)
        const uint32_t num_slots = support_wide_lines_ ? MAX_WIDTHS : 1;
        for (uint32_t i = 0; i < num_slots; ++i)
        {
            if (slot_counts[i] == 0)
                continue;
            if (!slots_[i].EnsureCapacity(slot_counts[i], device_, binding_set_, i + 1))
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
        uint32_t transform_owner_components = 0;
        uint32_t resolved_transform_components = 0;
        for (const auto& comp : collected_)
        {
            const uint32_t idx = GetSlotIndex(comp->width);
            bool comp_write_ok = true;

            graph::Assign::TransformID::ValueType transform_id = 0;
            if (transform_system)
            {
                Entity* owner = comp ? comp->GetOwner() : nullptr;
                auto transform = owner ? owner->GetComponent<TransformComponent>() : nullptr;
                if (transform)
                {
                    ++transform_owner_components;

                    const auto handle = transform->GetStorageHandle();
                    uint32_t group_index = 0;
                    if (handle != TransformDataStorage::INVALID_HANDLE
                        && transform_system->TryGetTransformGroupIndex(handle, transform->IsMovable(), group_index))
                    {
                        const uint32_t resolved = transform->IsMovable() ? (dynamic_base + group_index)
                                                                          : (group_index + 1u);

                        constexpr uint32_t kMaxTransformID = std::numeric_limits<graph::Assign::TransformID::ValueType>::max();
                        transform_id = resolved > kMaxTransformID
                                     ? 0
                                     : static_cast<graph::Assign::TransformID::ValueType>(resolved);

                        if (transform_id != 0)
                            ++resolved_transform_components;
                    }
                }
            }

            for (const auto& seg : comp->lines)
            {
                if (!slots_[idx].AddSegment(seg.from, seg.to, seg.color_index, transform_id))
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

        if (transform_owner_components > 0)
        {
            static uint32_t s_diag_frame = 0;
            ++s_diag_frame;

            if ((s_diag_frame % 120u) == 1u || resolved_transform_components == 0)
            {
                GLogInfo("[LineRenderPipeline] TransformID resolve: owners=%u resolved_nonzero=%u static=%u dynamic=%u dynamic_base=%u",
                         transform_owner_components,
                         resolved_transform_components,
                         static_count,
                         dynamic_count,
                         dynamic_base);
            }
        }

        GLogInfo("[LineRenderPipeline] Build summary: collected=%zu non_empty_slots=%u expected_lines=%u built_lines=%u write_fail=%u",
                 collected_.size(),
                 non_empty_slots,
                 expected_total,
                 total_line_count_,
                 write_fail_count);
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
        if (!cmd)
        {
            GLogWarning("[LineRenderPipeline] Render skipped: cmd is null");
            return;
        }

        if (!initialized_)
        {
            GLogWarning("[LineRenderPipeline] Render skipped: pipeline not initialized");
            return;
        }

        if (total_line_count_ == 0)
        {
            GLogInfo("[LineRenderPipeline] Render skipped: total_line_count=0 (visible_components=%u total_components=%u)",
                     stats_.visible_components,
                     stats_.total_components);
            return;
        }

        if (!pipeline_ || !material_)
        {
            GLogWarning("[LineRenderPipeline] Render skipped: invalid render resources (pipeline=%p material=%p)",
                        pipeline_,
                        material_);
            return;
        }

        cmd->BindDescriptorSets(material_);

        cmd->BindPipeline(pipeline_);

        const uint32_t num_slots = support_wide_lines_ ? MAX_WIDTHS : 1;
        uint32_t draw_lines = 0;
        GLogInfo("[LineRenderPipeline] Render begin: total_line_count=%u num_slots=%u wide_lines=%d",
                 total_line_count_,
                 num_slots,
                 support_wide_lines_ ? 1 : 0);

        for (uint32_t i = 0; i < num_slots; ++i)
        {
            if (slots_[i].line_count == 0)
                continue;

            if (support_wide_lines_)
                cmd->SetLineWidth(static_cast<float>(i + 1));

            GLogInfo("[LineRenderPipeline] Render slot: slot=%u line_count=%u capacity=%u data_buffer=%p",
                     i + 1,
                     slots_[i].line_count,
                     slots_[i].gpu_capacity,
                     slots_[i].data_buffer);
            slots_[i].Draw(cmd);
            draw_lines += slots_[i].line_count;
        }

        if (draw_lines != total_line_count_)
        {
            GLogWarning("[LineRenderPipeline] Render mismatch: total_line_count=%u draw_lines=%u",
                        total_line_count_,
                        draw_lines);
        }

        GLogInfo("[LineRenderPipeline] Render end: submitted_lines=%u expected_lines=%u",
                 draw_lines,
                 total_line_count_);
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
                if (material_ && binding_vil_)
                {
                    material_->Release(binding_vil_);
                    binding_vil_ = nullptr;
                }
                binding_set_storage_.SetVIL(nullptr);
                binding_set_storage_.SetMaterial(nullptr);
                binding_set_ = nullptr;
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

        pipeline_    = nullptr; // owned by RenderPass
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
        
        palette_[index]  = color;
        palette_dirty_   = true;
        
        GLogInfo(OS_TEXT("[LineRenderPipeline] SetPaletteColor: palette_[%d] now = (%.2f,%.2f,%.2f,%.2f)"),
                 index, palette_[index].r, palette_[index].g, palette_[index].b, palette_[index].a);
        
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

        auto* transform_data_buffer = transform_buffer->GetTransformDataBuffer();
        if (!transform_data_buffer)
        {
            GLogWarning("[LineRenderPipeline] SyncTransformBinding: transform_data_buffer is null");
            return;
        }

        const uint32_t static_count = transform_system->GetStaticCount();
        const uint32_t dynamic_count = transform_system->GetDynamicCount();
        transform_buffer->EnsureCapacity(static_count, dynamic_count, graph::BufferAllocPolicy::Auto);

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
