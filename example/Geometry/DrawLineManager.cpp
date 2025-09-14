#include "DrawLineManager.h"
#include <cstring>
#include <hgl/graph/PrimitiveCreater.h>
#include <hgl/graph/VKDevice.h>
#include <hgl/graph/VKVertexInputLayout.h>
#include <hgl/graph/VKPrimitive.h>
#include <hgl/graph/module/MaterialManager.h>
#include <hgl/graph/mtl/Material3DCreateConfig.h>
#include <hgl/graph/VKMaterial.h>
#include <hgl/graph/module/PrimitiveManager.h>
#include <hgl/graph/module/MeshManager.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/RenderFramework.h>

namespace hgl
{
    namespace graph
    {
        DrawLineManager* CreateDrawLineManager(RenderFramework *rf)
        {
            if (!rf)
                return(nullptr);

            // Create PureColor3D material via factory
            mtl::Material3DCreateConfig cfg(PrimitiveType::Lines,
                                            mtl::WithCamera::With,
                                            mtl::WithLocalToWorld::Without,
                                            mtl::WithSky::Without);

            auto *mci = mtl::CreatePureColor3D(rf->GetDevAttr(), &cfg);

            if (mci == nullptr)
                return nullptr;

            Material *mat = rf->CreateMaterial("M_Line3D", mci);

            if (mat == nullptr)
            {
                delete mci;
                return nullptr;
            }

            MaterialInstance *mi = rf->CreateMaterialInstance(mat);

            if (mi == nullptr)
            {
                delete mat;
                delete mci;
                return nullptr;
            }

            Pipeline *p = rf->CreatePipeline(mi,InlinePipeline::Solid3D);

            DrawLineManager *mgr = new DrawLineManager(mi,p,rf->GetMeshManager());

            return mgr;
        }

        DrawLineManager::DrawLineManager(   MaterialInstance * mi,
                                            Pipeline *p,
                                            MeshManager *mm)
        {
            mi_line3d = mi;
            pipeline = p;
            mesh_manager = mm;

            primitive_creater = new PrimitiveCreater(mm->GetDevice(),mi->GetVIL());

            mesh = nullptr;

            for(auto &lsb : line_groups)
            {
                lsb.position.reserve(1024 * 6);
                lsb.color.reserve(1024 * 2);
                lsb.count = 0;
            }
        }

        DrawLineManager::~DrawLineManager()
        {
            delete primitive_creater;
        }

        void DrawLineManager::SetColor(const int index, const Color4f& c)
        {
            if (index < 0 || index >= 256) return;

            color_palette[index] = c;

            color_dirty = true;
        }

        bool DrawLineManager::AddLine(const Vector3f& from, const Vector3f& to, const uint8_t color_index, uint8_t width)
        {
            if (width == 0
              ||width > MAX_LINE_WIDTH)
                return(false);

            LineSegmentBuffer *lsb = &line_groups[width - 1];

            if(lsb->count * 2 >= lsb->color.size())
            {
                lsb->position.resize(lsb->position.size() + 1024 * 6);
                lsb->color.resize(lsb->color.size() + 1024 * 2);
            }

            lsb->position.push_back(from.x);
            lsb->position.push_back(from.y);
            lsb->position.push_back(from.z);

            lsb->position.push_back(to.x);
            lsb->position.push_back(to.y);
            lsb->position.push_back(to.z);

            lsb->color.push_back(color_index);
            lsb->color.push_back(color_index);

            ++lsb->count;

            line_dirty = true;
        }

        void DrawLineManager::ClearLines()
        {
            for(auto &lsb : line_groups)
                lsb.count = 0;

            line_dirty = true;
        }

        void DrawLineManager::UpdateLines()
        {
            if (!line_dirty)
                return;

            if(mesh)
            {
                mesh_manager->Release(mesh);
                mesh = nullptr;
            }

            uint32_t count = 0;

            for(auto &kv : line_groups)
                count += kv.count;

            if(!primitive_creater->Init("Line3D",count * 2,0,IndexType::U32))
            {
                line_dirty = false;
                return;
            }

            VABMapFloat    vab_pos    (primitive_creater->GetVABMap(VAN::Position,VF_V3F));
            VABMapU8       vab_color  (primitive_creater->GetVABMap(VAN::Color,VF_V1U8));

            float *pp = vab_pos.Map();
            uint8 *cp = vab_color.Map();

            for(auto &kv : line_groups)
            {
                hgl_cpy<float>(pp,kv.position.data(),kv.count * 2);
                hgl_cpy<uint8>(cp,kv.color.data(),kv.count * 2);

                pp += kv.count * 2;
                cp += kv.count * 2;
            }
        }

        void DrawLineManager::Draw(RenderCmdBuffer* cmd, DrawCallback draw_callback)
        {
            if (cmd == nullptr || draw_callback == nullptr) return;

            std::lock_guard lock(mutex_);

            // Build temporary per-width LineVertex arrays from segments
            for (auto& kv : groups_)
            {
                uint8_t width = kv.first;
                auto& segments = kv.second;

                if (segments.empty()) continue;

                std::vector<LineVertex> tmp;
                tmp.reserve(segments.size());

                for (const LineSegment& seg : segments)
                {
                    LineVertex lv;
                    lv.start = seg.start;
                    lv.end = seg.end;
                    // Representative color from palette using color_index
                    uint32_t packed = 0;
                    if (seg.color_index < 256)
                        packed = color_palette[seg.color_index].ToRGBA8();
                    lv.color = packed;
                    tmp.push_back(lv);
                }

                draw_callback(cmd, width, tmp.data(), tmp.size());
            }
        }

        size_t DrawLineManager::GetLineCount() const
        {
            std::lock_guard lock(mutex_);
            return line_count_;
        }

        bool DrawLineManager::Update()
        {
            std::lock_guard lock(mutex_);

            if (dirty_ == false)
                return true;

            // determine whether we need to upload (size changed)
            const bool need_upload_flag = (last_uploaded_count_ != static_cast<uint32_t>(line_count_));

            if (need_upload_flag == false)
            {
                dirty_ = false; // nothing changed in buffer size
                return true;
            }

            // Release previous primitive/mesh
            if (mesh_ != nullptr)
            {
                if (mesh_manager_ != nullptr) mesh_manager_->Release(mesh_);
                mesh_ = nullptr;
            }

            if (primitive_ != nullptr)
            {
                if (primitive_manager_ != nullptr) primitive_manager_->Release(primitive_);
                else delete primitive_;
                primitive_ = nullptr;
            }

            if (line_count_ == 0)
            {
                last_uploaded_count_ = 0;
                dirty_ = false;
                return true;
            }

            // vertex_count = line_count * 2
            const uint32_t vertex_count = static_cast<uint32_t>(line_count_ * 2);

            PrimitiveCreater pc(device_, vil_);

            if (pc.Init("DrawLineManager_Prim", vertex_count) == false)
            {
                dirty_ = false;
                return false;
            }

            const uint32_t vabCount = vil_->GetVertexAttribCount();
            for (uint32_t i = 0; i < vabCount; i++)
            {
                const VIF* vif = vil_->GetConfig(i);
                if (!vif) continue;
                AnsiString attr_name = vif->name;

                if (attr_name.CaseComp("position") == 0 || attr_name.CaseComp("pos") == 0 || attr_name.CaseComp("start") == 0)
                {
                    pc.WriteVAB(attr_name, vif->format, positions_.data());
                }
                else if (attr_name.CaseComp("color") == 0)
                {
                    if (vif->format == VK_FORMAT_R8G8B8A8_UNORM || vif->format == VK_FORMAT_R8G8B8A8_UINT || vif->format == VK_FORMAT_R8G8B8A8_SINT)
                    {
                        pc.WriteVAB(attr_name, vif->format, colors_.data());
                    }
                    else
                    {
                        std::vector<Vector4f> colf(vertex_count);
                        for (uint32_t j = 0; j < vertex_count; j++)
                        {
                            colf[j].x = colors_[j * 4 + 0] / 255.0f;
                            colf[j].y = colors_[j * 4 + 1] / 255.0f;
                            colf[j].z = colors_[j * 4 + 2] / 255.0f;
                            colf[j].w = colors_[j * 4 + 3] / 255.0f;
                        }
                        pc.WriteVAB(attr_name, vif->format, colf.data());
                    }
                }
            }

            primitive_ = pc.Create();
            if (primitive_ == nullptr)
            {
                dirty_ = false;
                return false;
            }

            if (primitive_manager_ != nullptr) primitive_manager_->Add(primitive_);

            if (mesh_manager_ != nullptr)
                mesh_ = mesh_manager_->CreateMesh(primitive_, m_line3d_instance, pipeline_);
            else
                mesh_ = hgl::graph::DirectCreateMesh(primitive_, m_line3d_instance, pipeline_);

            last_uploaded_count_ = static_cast<uint32_t>(line_count_);
            dirty_ = false;

            return mesh_ != nullptr;
        }
    }
} // namespace

