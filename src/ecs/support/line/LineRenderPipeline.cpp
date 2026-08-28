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
#include <hgl/graph/module/ShaderProgramManager.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/DescriptorBindingSet.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <hgl/graph/mesh/GeometryDataBuffer.h>
#include <hgl/graph/mesh/GeometryDrawRange.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/MaterialDefinitionRegistry.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/VKShaderProgram.h>
#include <hgl/vk/VKBuffer.h>
#include <hgl/vk/VKCommandBuffer.h>
#include <hgl/vk/VKRenderTarget.h>
#include <hgl/graph/ShaderBufferSources.h>
#include <hgl/vk/VKRenderAssign.h>
#include <hgl/vk/VKBindlessTextureManager.h>
#include <hgl/vk/VKGlobalSceneUBOSet.h>
#include <hgl/vk/VKVABList.h>
#include <hgl/math/geometry/Frustum.h>
#include <hgl/log/Log.h>
#include <glm/glm.hpp>
#include <limits>

namespace hgl::ecs
{
    // A5 限流日志：日志系统无级别过滤（GLogVerbose 与 GLogInfo 同输出），
    // 每帧高频日志用计数器限流——每 60 帧输出一次
    static uint32_t s_line_log_ticks[16] = {};

    template<typename... Args>
    void LinePeriodicLog(uint32_t &tick, const char *fmt, Args&&... args)
    {
        if ((++tick % 60u) == 1u)
            GLogInfo(fmt, std::forward<Args>(args)...);
    }
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
            gvf.Add(graph::VertexSemantic::Size, VK_FORMAT_R32G32_SFLOAT, 2, sizeof(float) * 2);

            return gvf;
        }
    }

    // -------------------------------------------------------------------------
    const std::string LineRenderPipeline::kName{ "Line" };

    // -------------------------------------------------------------------------
    // LineBuffer helpers（P2：单 buffer——删 4 slot 分组）
    // -------------------------------------------------------------------------

    void LineRenderPipeline::LineBuffer::Reset()
    {
        line_count = 0;
        bool pos_valid = va_pos.IsValid();
        bool color_valid = va_color.IsValid();
        bool transform_valid = va_transform.IsValid();
        bool width_valid = va_width.IsValid();
        
        LinePeriodicLog(s_line_log_ticks[0], "[LineRenderPipeline] Reset: pos_valid=%d color_valid=%d transform_valid=%d width_valid=%d",
                 pos_valid ? 1 : 0,
             color_valid ? 1 : 0,
             transform_valid ? 1 : 0,
             width_valid ? 1 : 0);
        
        if (pos_valid)   va_pos.Seek(0);
        if (color_valid) va_color.Seek(0);
        if (transform_valid) va_transform.Seek(0);
        if (width_valid) va_width.Seek(0);
        if (draw_range)  draw_range->vertex_count = 0;
    }

    void LineRenderPipeline::LineBuffer::Clear()
    {
        va_pos.Bind(nullptr);
        va_color.Bind(nullptr);
        va_transform.Bind(nullptr);
        va_width.Bind(nullptr);
        SAFE_CLEAR(data_buffer);
        SAFE_CLEAR(draw_range);
        SAFE_CLEAR(geometry);
        SAFE_CLEAR(mesh_draw_params);
        line_count   = 0;
        gpu_capacity = 0;
    }

    bool LineRenderPipeline::LineBuffer::EnsureCapacity(
        uint32_t needed,
        graph::VulkanDevice*     dev,
        graph::DescriptorBindingSet* binding_set)
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
        va_transform.Bind(nullptr);
        va_width.Bind(nullptr);
        SAFE_CLEAR(data_buffer);
        SAFE_CLEAR(draw_range);
        SAFE_CLEAR(geometry);

        // Create new geometry (2 verts per line)
        const AnsiString name = AnsiString("LineBuffer");
        geometry = graph::CreateGeometry(dev,
                         CreateLineGeometryVertexFormat(),
                         name, new_cap * 2, 0,
                         graph::IndexType::AUTO, nullptr,
                         graph::BufferAllocPolicy::StagedUpload);
        if (!geometry)
            return false;

        // 顶点输入统一为 SSBO：GeometryDataBuffer 槽位数 = Geometry 语义数
        const uint32_t semantic_count = geometry->GetGeometryVertexFormat().GetCount();

        data_buffer = new graph::GeometryDataBuffer(semantic_count,
                                                    geometry->GetIBO(),
                                                    geometry->GetVDM());
        if (!data_buffer
         || !data_buffer->Update(geometry))
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
        const int size_idx   = geometry->GetVABIndex(graph::VertexSemantic::Size);

        if (pos_idx < 0 || color_idx < 0 || transform_idx < 0 || size_idx < 0)
        {
            SAFE_CLEAR(data_buffer);
            SAFE_CLEAR(draw_range);
            SAFE_CLEAR(geometry);
            return false;
        }

        va_pos.Bind(geometry->GetVAB(pos_idx));
        va_color.Bind(geometry->GetVAB(color_idx));
        va_transform.Bind(geometry->GetVAB(transform_idx));
        va_width.Bind(geometry->GetVAB(size_idx));

        LinePeriodicLog(s_line_log_ticks[1], "[LineRenderPipeline] LineBuffer after Bind: pos_valid=%d color_valid=%d transform_valid=%d width_valid=%d",
                 va_pos.IsValid() ? 1 : 0,
                 va_color.IsValid() ? 1 : 0,
                 va_transform.IsValid() ? 1 : 0,
                 va_width.IsValid() ? 1 : 0);

        if (!va_pos.IsValid() || !va_color.IsValid() || !va_transform.IsValid() || !va_width.IsValid())
        {
            GLogWarning("[LineRenderPipeline] LineBuffer accessor bind failed (pos_valid=%d color_valid=%d transform_valid=%d width_valid=%d)",
                        va_pos.IsValid() ? 1 : 0,
                        va_color.IsValid() ? 1 : 0,
                        va_transform.IsValid() ? 1 : 0,
                        va_width.IsValid() ? 1 : 0);
            SAFE_CLEAR(data_buffer);
            SAFE_CLEAR(draw_range);
            SAFE_CLEAR(geometry);
            return false;
        }

        va_pos.Seek(0);
        va_color.Seek(0);
        va_transform.Seek(0);
        va_width.Seek(0);

        gpu_capacity = new_cap;
        return true;
    }

    bool LineRenderPipeline::LineBuffer::AddSegment(
        const hgl::math::Vector3f& from,
        const hgl::math::Vector3f& to,
        uint8_t                     color_index,
        float                       width,
        float                       min_width,
        graph::Assign::TransformID::ValueType transform_index)
    {
        bool pos_valid = va_pos.IsValid();
        bool color_valid = va_color.IsValid();
        bool transform_valid = va_transform.IsValid();
        bool width_valid = va_width.IsValid();
        
        if (!pos_valid || !color_valid || !transform_valid || !width_valid)
        {
            GLogWarning("[LineRenderPipeline] AddSegment accessor invalid: pos=%d color=%d transform=%d width=%d",
                        pos_valid ? 1 : 0,
                        color_valid ? 1 : 0,
                        transform_valid ? 1 : 0,
                        width_valid ? 1 : 0);
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
        // Size 语义 V2F：[满宽(最粗), 最细阈值]——mesh shader 统一 clamp(满宽×衰减, 最细, 满宽)
        // 每线段 2 顶点（from/to）各写一次
        if (!va_width.Write(hgl::math::Vector2f(width, min_width)))
            return false;
        if (!va_width.Write(hgl::math::Vector2f(width, min_width)))
            return false;

        ++line_count;
        if (draw_range)
            draw_range->vertex_count = line_count * 2;
        return true;
    }

    void LineRenderPipeline::LineBuffer::Draw(graph::RenderCmdBuffer* cmd)
    {
        if (!cmd)
        {
            GLogWarning("[LineRenderPipeline] Draw skipped: cmd is null");
            return;
        }

        if (line_count == 0)
            return;

        if (!data_buffer || !draw_range || !geometry || !material)
        {
            GLogWarning("[LineRenderPipeline] Draw skipped: data_buffer/draw_range/geometry/material null while line_count=%u", line_count);
            return;
        }

        // P2：mesh shader 单 buffer——4 个顶点 SSBO 绑到 material 的 PerObject set
        // （单 buffer 无 slot 竞争，直接绑共享 set 即可——不再需要每 slot 独立 set）
        auto bind_ssbo = [&](const graph::VertexSemantic semantic, const char *name)
        {
            auto *vab = geometry->GetVAB(semantic);
            if (!vab)
                return false;
            material->BindSSBO(graph::DescriptorSetType::PerObject, name,
                               vab->GetVkBuffer(), 0, VK_WHOLE_SIZE);
            return true;
        };

        if (!bind_ssbo(graph::VertexSemantic::Position, "VertexPosition")
         || !bind_ssbo(graph::VertexSemantic::Color, "VertexColor")
         || !bind_ssbo(graph::Assign::TransformID::VIS_SEMANTIC, "VertexTransformID")
         || !bind_ssbo(graph::VertexSemantic::Size, "VertexSize"))
            return;

        // IndirectMeshDraw：mesh per-draw 参数表（row 0——Line 单 draw，gl_DrawID=0）
        if (mesh_draw_params)
            material->BindSSBO(graph::DescriptorSetType::PerObject,
                               "mesh_draw_params",
                               mesh_draw_params->GetGPUBuffer()->GetVkDeviceBuffer(),
                               0, VK_WHOLE_SIZE);

        auto *mp = material->GetMP(graph::DescriptorSetType::PerObject);
        if (!mp)
            return;
        mp->Update();
        const VkDescriptorSet ds = mp->GetVkDescriptorSet();
        cmd->BindDescriptorSets(material->GetPipelineLayout(),
                                static_cast<uint32_t>(graph::DescriptorSetType::PerObject),
                                &ds, 1, nullptr, 0);

        // Mesh shader 绘制：每线程 1 线段，threadgroup = MESH_GROUP_SIZE
        //（per-draw 段偏移/线宽参数经 mesh_draw_params 参数表传递——不再推 push constant）
        const uint32_t group_count = (line_count + LineRenderPipeline::MESH_GROUP_SIZE - 1)
                                    / LineRenderPipeline::MESH_GROUP_SIZE;
        cmd->DrawMeshTasks(group_count);

        LinePeriodicLog(s_line_log_ticks[2], "[LineRenderPipeline] DrawMeshTasks issued: data_buffer=%p line_count=%u vertex_count=%u groups=%u",
                 data_buffer,
                 line_count,
                 line_count * 2u,
                 group_count);
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

        GLogInfo(OS_TEXT("[LineRenderPipeline] Initialize: START"));

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

        const graph::GeometryVertexFormat line_gvf = CreateLineGeometryVertexFormat();

        // ------- Create material -------
        graph::mtl::MaterialRecipe recipe{};
        recipe.mtl_def_id = "VertexPaletteColor";

        auto* mat_mgr = gc->GetMaterialManager();
        if (!mat_mgr)
            return false;

        {
            graph::mtl::MaterialDefinitionBuildRequest mtl_request{};
            mtl_request.recipe = recipe;
            mtl_request.primitive_type = graph::PrimitiveType::Lines;
            mtl_request.geometry_vertex_format = &line_gvf;
            material_ = mat_mgr->AcquireShaderProgram(mtl_request);
        }
        if (!material_)
            return false;

        if (auto rdbs = context_->GetSystem<RenderDescriptorBindingSystem>())
            rdbs->RegisterPipelineMaterial(material_);

        // ------- Create descriptor binding set -------
        binding_set_storage_.SetMaterial(material_);
        binding_set_ = &binding_set_storage_;

        // ------- Create pipeline -------
        // P2：mesh 管线（宽度入 SSBO，cull off——quad 绕序不定，双面绘制）
        pipeline_ = rp->CreatePipeline(material_, graph::mtl::MakeLineMeshConfig(), &line_gvf);

        if (!pipeline_)
            return false;

        SyncTransformBinding();

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

        SyncTransformBinding();

        // Reset 单 Line buffer（P2：删 4 slot 分组）
        line_buffer_.Reset();

        collected_.clear();
        stats_ = LineCollectStats{};
        total_line_count_ = 0;

        return true;
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

        LinePeriodicLog(s_line_log_ticks[3], "[LineRenderPipeline] Collect summary: total=%u visible=%u collected=%zu culled_visibility=%u culled_frustum=%u culled_hzb=%u",
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

        // P2：单 Line buffer（删 4 slot 分组）——first pass 只算总数
        uint32_t expected_total = 0;
        for (const auto& comp : collected_)
        {
            expected_total += static_cast<uint32_t>(comp->lines.size());
        }

        // Ensure GPU capacity（单 buffer，按总数）
        if (!line_buffer_.EnsureCapacity(expected_total, device_, binding_set_))
        {
            GLogWarning("[LineRenderPipeline] EnsureCapacity failed: need=%u cap=%u",
                        expected_total,
                        line_buffer_.gpu_capacity);
            return; // Allocation failure: skip frame
        }

        if (line_buffer_.gpu_capacity < expected_total)
        {
            GLogWarning("[LineRenderPipeline] LineBuffer capacity insufficient after ensure: cap=%u need=%u",
                        line_buffer_.gpu_capacity,
                        expected_total);
        }

        line_buffer_.Reset(); // seek back to 0

        // Second pass: write segments
        uint32_t write_fail_count = 0;
        uint32_t transform_owner_components = 0;
        uint32_t resolved_transform_components = 0;
        for (const auto& comp : collected_)
        {
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
                // P2：width 入 SSBO（删 slot 分组）——单 buffer 顺序写入
                if (!line_buffer_.AddSegment(seg.from, seg.to, seg.color_index,
                                             static_cast<float>(comp->width),
                                             seg.min_width,
                                             transform_id))
                {
                    ++write_fail_count;
                    comp_write_ok = false;
                }
            }

            if (comp_write_ok)
                comp->MarkSynced();
        }

        // Tally total
        total_line_count_ = line_buffer_.line_count;

        if (write_fail_count > 0 || total_line_count_ != expected_total)
        {
            GLogWarning("[LineRenderPipeline] Build mismatch: expected=%u built=%u write_fail=%u collected_components=%zu",
                        expected_total,
                        total_line_count_,
                        write_fail_count,
                        collected_.size());

            GLogWarning("[LineRenderPipeline]   line_buffer: built=%u capacity=%u pos_valid=%d color_valid=%d",
                        line_buffer_.line_count,
                        line_buffer_.gpu_capacity,
                        line_buffer_.va_pos.IsValid() ? 1 : 0,
                        line_buffer_.va_color.IsValid() ? 1 : 0);
        }

        if (transform_owner_components > 0)
        {
            static uint32_t s_diag_frame = 0;
            ++s_diag_frame;

            if ((s_diag_frame % 120u) == 1u || resolved_transform_components == 0)
            {
                LinePeriodicLog(s_line_log_ticks[4], "[LineRenderPipeline] TransformID resolve: owners=%u resolved_nonzero=%u static=%u dynamic=%u dynamic_base=%u",
                         transform_owner_components,
                         resolved_transform_components,
                         static_count,
                         dynamic_count,
                         dynamic_base);
            }
        }

        LinePeriodicLog(s_line_log_ticks[5], "[LineRenderPipeline] Build summary: collected=%zu expected_lines=%u built_lines=%u write_fail=%u",
                 collected_.size(),
                 expected_total,
                 total_line_count_,
                 write_fail_count);

        // IndirectMeshDraw：写 mesh per-draw 参数行 row 0（Line 单 draw 非实例化——
        // 绘制时 gl_DrawID=0；shader 不再读 push constant。build 阶段写入，
        // RenderBufferUploadSystem 上传后再进录制）
        if (device_ && total_line_count_ > 0)
        {
            if (!line_buffer_.mesh_draw_params)
                line_buffer_.mesh_draw_params = device_->CreateSSBO(
                    "ECS:Line:MeshDrawParams", sizeof(graph::mtl::MeshDrawParams));

            if (line_buffer_.mesh_draw_params)
            {
                auto *gpu = line_buffer_.mesh_draw_params->GetGPUBuffer();
                auto *row = gpu ? static_cast<graph::mtl::MeshDrawParams *>(
                    gpu->Map(0, sizeof(graph::mtl::MeshDrawParams))) : nullptr;

                if (row)
                {
                    row->index_base      = 0;
                    row->vertex_base     = 0;
                    row->is_indexed      = 0;
                    row->total_vertices  = total_line_count_ * 2u;
                    row->first_instance  = 0;
                    gpu->Unmap();
                }
            }
        }
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

        cmd->BindPipeline(pipeline_);

        // Set 0（Scene UBO）/ Set 3（Bindless 纹理）按材质自身 layout 绑定。
        // VVL 的 set 兼容 ID 取 layout 在 set 0..N 的全部 DSL 前缀，绑定 layout 必须与
        // draw 时管线 layout（= 材质 pipeline layout）一致。见 PipelineMaterialRenderer::Render。
        if (auto* rc = context_ ? context_->GetRenderContext() : nullptr)
        {
            if (auto* gc = rc->GetGraphicsContext())
            {
                const VkPipelineLayout layout = material_->GetPipelineLayout();

                if (auto *scene_set = gc->GetGlobalSceneUBOSet();
                    scene_set && scene_set->IsValid())
                {
                    scene_set->BindToCmd(*cmd, layout);
                }

                if (auto *bindless_mgr = gc->GetBindlessTextureManager();
                    bindless_mgr && bindless_mgr->IsValid())
                {
                    bindless_mgr->BindToCmd(*cmd,
                                            layout,
                                            static_cast<uint32_t>(graph::DescriptorSetType::Bindless));
                }
            }
        }

        // P2：单 Line buffer（删 4 slot 分组 + SetLineWidth）——一次 DrawMeshTasks
        line_buffer_.material = material_;
        line_buffer_.Draw(cmd);

        LinePeriodicLog(s_line_log_ticks[8], "[LineRenderPipeline] Render end: submitted_lines=%u expected_lines=%u",
                 total_line_count_,
                 total_line_count_);
    }

    void LineRenderPipeline::Shutdown()
    {
        // P2：单 Line buffer（删 4 slot 分组）
        line_buffer_.Clear();

        if (auto* gc = context_ ? context_->GetGraphicsContext() : nullptr)
        {
            auto* mat_mgr = gc->GetMaterialManager();
            if (mat_mgr)
            {
                binding_set_storage_.SetMaterial(nullptr);
                binding_set_ = nullptr;
                if (material_)
                {
                    if (auto rdbs = context_->GetSystem<RenderDescriptorBindingSystem>())
                        rdbs->UnregisterPipelineMaterial(material_);
                    mat_mgr->Destroy(material_); material_ = nullptr;
                }
            }
        }

        pipeline_    = nullptr; // owned by RenderPass
        device_      = nullptr;
        initialized_ = false;
        bound_transform_buffer_ = nullptr;
        bound_transform_data_buffer_ = nullptr;
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
        LinePeriodicLog(s_line_log_ticks[9], "[LineRenderPipeline] SyncTransformBinding snapshot: tab=0x%llX dbuf=0x%llX vk=0x%llX gpu=0x%llX size=%llu dirty=%d static=%u dynamic=%u",
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

        // P2：单 material 共享 PerObject set——BindTransform(material_) 已绑 L2W 到共享 set，
        // 无需再补每 slot 独立 set（slot_mp 机制已随 4 slot 分组整体删除）。
        // mesh shader 的 GetL2W() 读 l2w.mats[TransformID]，即此绑定。
    }

}  // namespace hgl::ecs
