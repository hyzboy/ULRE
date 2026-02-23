#include <hgl/ecs/support/line/LineRenderPipeline.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/components/LinesComponent.h>
#include <hgl/ecs/components/BoundingBoxComponent.h>
#include <hgl/ecs/components/VisibilityComponent.h>
#include <hgl/ecs/systems/tick/CameraSystem.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/render/RenderContext.h>
#include <hgl/graph/module/MaterialManager.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <hgl/graph/mesh/Primitive.h>
#include <hgl/graph/mtl/Material3DCreateConfig.h>
#include <hgl/graph/mtl/UBOCommon.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/VKMaterial.h>
#include <hgl/vk/VKBuffer.h>
#include <hgl/vk/VKCommandBuffer.h>
#include <hgl/vk/VKRenderTarget.h>
#include <hgl/vk/VKVertexInputConfig.h>
#include <hgl/vk/StructuredBufferAccessor.h>
#include <hgl/math/geometry/Frustum.h>
#include <glm/glm.hpp>
#include <algorithm>

namespace hgl::ecs
{
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
        if (va_pos.IsValid())   va_pos.Seek(0);
        if (va_color.IsValid()) va_color.Seek(0);
        if (primitive)          primitive->SetDrawCounts(0);
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
        graph::MaterialInstance* mi,
        graph::Pipeline*         p,
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
        geometry = graph::CreateGeometry(dev, mi->GetVIL(), name, new_cap * 2, 0,
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
        va_pos.Seek(0);
        va_color.Seek(0);

        gpu_capacity = new_cap;
        return true;
    }

    void LineRenderPipeline::LineWidthSlot::AddSegment(
        const hgl::math::Vector3f& from,
        const hgl::math::Vector3f& to,
        uint8_t                     color_index)
    {
        va_pos.Write(from);
        va_pos.Write(to);
        va_color.Write(color_index);
        va_color.Write(color_index);
        ++line_count;
        if (primitive)
            primitive->SetDrawCounts(line_count * 2);
    }

    void LineRenderPipeline::LineWidthSlot::Draw(graph::RenderCmdBuffer* cmd)
    {
        if (line_count == 0 || !primitive)
            return;
        cmd->BindDataBuffer(primitive->GetDataBuffer());
        cmd->Draw(primitive->GetDataBuffer(), primitive->GetRenderData());
    }

    // -------------------------------------------------------------------------
    // LineRenderPipeline
    // -------------------------------------------------------------------------

    LineRenderPipeline::LineRenderPipeline(ECSContext* context)
        : context_(context)
    {
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
        graph::mtl::Material3DCreateConfig cfg(
            graph::PrimitiveType::Lines,
            graph::mtl::WithCamera::With,
            graph::mtl::WithLocalToWorld::Without,
            graph::mtl::WithSky::Without);

        auto* mci = graph::mtl::CreateVertexPattleColor3D(gc->GetDevAttr(), &cfg);
        if (!mci)
            return false;

        auto* mat_mgr = gc->GetMaterialManager();
        if (!mat_mgr) { delete mci; return false; }

        material_ = mat_mgr->CreateMaterial("M_Line3D_ECS", mci);
        if (!material_) { delete mci; return false; }

        // ------- Create material instance -------
        graph::VILConfig vil;
        vil.Add(graph::VAN::Color, VF_V1U8);
        mi_ = mat_mgr->CreateMaterialInstance(material_, &vil);
        if (!mi_) { delete mci; return false; }

        // ------- Create pipeline -------
        pipeline_ = support_wide_lines_
            ? rp->CreatePipeline(mi_, graph::InlinePipeline::DynamicLineWidth3D)
            : rp->CreatePipeline(mi_, graph::InlinePipeline::Solid3D);

        if (!pipeline_) { delete mci; return false; }

        // ------- Create color palette UBO -------
        auto* buf_mgr = gc->GetBufferManager();
        if (!buf_mgr) { delete mci; return false; }

        auto* raw_buf = buf_mgr->CreateUBO("LineColorPaletteUBO_ECS",
                                            graph::StructuredBufferAccessor<LineColorPalette>::GetSize());
        if (!raw_buf) { delete mci; return false; }
        raw_buf->SetUpdateClass(graph::BufferUpdateClass::Default);

        auto* ubo = graph::StructuredBufferAccessor<LineColorPalette>::Create(
                        raw_buf, &graph::mtl::SBS_ColorPattle, false);
        if (!ubo) { delete mci; return false; }

        ubo_color_   = ubo;
        ubo_raw_buf_ = raw_buf;

        // Bind UBO to material, upload default white palette
        material_->BindUBO(&graph::mtl::SBS_ColorPattle, ubo->GetGPUBuffer());
        material_->Update();

        // Initialize default palette (white)
        std::fill(std::begin(palette_), std::end(palette_), hgl::Color4f(1.0f, 1.0f, 1.0f, 1.0f));
        palette_dirty_ = true;

        delete mci;
        initialized_ = true;
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

        // First pass: count lines per slot
        uint32_t slot_counts[MAX_WIDTHS] = {};
        for (const auto& comp : collected_)
        {
            const uint32_t idx = GetSlotIndex(comp->width);
            slot_counts[idx] += static_cast<uint32_t>(comp->lines.size());
        }

        // Ensure GPU capacity per slot (recreate if needed)
        const uint32_t num_slots = support_wide_lines_ ? MAX_WIDTHS : 1;
        for (uint32_t i = 0; i < num_slots; ++i)
        {
            if (slot_counts[i] == 0)
                continue;
            if (!slots_[i].EnsureCapacity(slot_counts[i], device_, mi_, pipeline_, i + 1))
                return; // Allocation failure: skip frame
            slots_[i].Reset(); // seek back to 0
        }

        // Second pass: write segments
        for (const auto& comp : collected_)
        {
            const uint32_t idx = GetSlotIndex(comp->width);
            for (const auto& seg : comp->lines)
                slots_[idx].AddSegment(seg.from, seg.to, seg.color_index);
            comp->MarkSynced();
        }

        // Tally total
        total_line_count_ = 0;
        for (uint32_t i = 0; i < num_slots; ++i)
            total_line_count_ += slots_[i].line_count;
    }

    void LineRenderPipeline::FlushPaletteToGPU()
    {
        if (!palette_dirty_ || !ubo_color_)
            return;

        auto* ubo = static_cast<UBOLineColorPalette*>(ubo_color_);
        auto* pal = ubo->Data();
        if (pal)
        {
            for (uint32_t i = 0; i < PALETTE_SIZE; ++i)
                (*pal)[i] = palette_[i];
            ubo->MarkDirty();
        }
        palette_dirty_ = false;
    }

    void LineRenderPipeline::Render(hgl::graph::RenderCmdBuffer* cmd)
    {
        if (!cmd || !initialized_ || total_line_count_ == 0)
            return;

        if (!pipeline_ || !mi_)
            return;

        FlushPaletteToGPU();

        auto* mat = mi_->GetMaterial();
        if (mat)
            cmd->BindDescriptorSets(mat);

        cmd->BindPipeline(pipeline_);

        const uint32_t num_slots = support_wide_lines_ ? MAX_WIDTHS : 1;
        for (uint32_t i = 0; i < num_slots; ++i)
        {
            if (slots_[i].line_count == 0)
                continue;

            if (support_wide_lines_)
                cmd->SetLineWidth(static_cast<float>(i + 1));

            slots_[i].Draw(cmd);
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
                if (material_) { mat_mgr->Destroy(material_); material_ = nullptr; }
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
    }

    void LineRenderPipeline::SetPaletteColor(int index, const hgl::Color4f& color)
    {
        if (index < 0 || index >= static_cast<int>(PALETTE_SIZE))
            return;
        palette_[index]  = color;
        palette_dirty_   = true;
    }

}  // namespace hgl::ecs
