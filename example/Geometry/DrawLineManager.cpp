#include "DrawLineManager.h"
#include <cstring>
#include <hgl/graph/VKRenderTarget.h>
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

        constexpr const ShaderBufferSource SBS_ColorPattle=
        {
            DescriptorSetType::PerMaterial,

            "color_pattle",
            "ColorPattle",

            "vec4 color[256]"
        };

        constexpr const size_t LINE_COUNT_INCREMENT =       1024;
        constexpr const size_t POSITION_COMPONENT_COUNT =   6;
        constexpr const size_t COLOR_COMPONENT_COUNT =      2;

        constexpr const size_t POSITION_COUNT_INCREMENT = LINE_COUNT_INCREMENT * POSITION_COMPONENT_COUNT;
        constexpr const size_t COLOR_COUNT_INCREMENT    = LINE_COUNT_INCREMENT * COLOR_COMPONENT_COUNT;

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

            Pipeline *p = rf->CreatePipeline(mi,InlinePipeline::DynamicLineWidth3D);

            UBOLineColorPattle *lcp=rf->CreateUBO<UBOLineColorPattle>(&SBS_ColorPattle);

            DrawLineManager *mgr = new DrawLineManager(mi,p,lcp,rf->GetMeshManager());

            return mgr;
        }

        void DrawLineManager::LineSegmentBuffer::Init(const uint w,hgl::graph::VulkanDevice *dev,MaterialInstance *mi,Pipeline *p)
        {
            device = dev;
            mtl_inst = mi;

            pipeline = p;

            width = w;

            max_count = 0;
            count = 0;
        }

        void DrawLineManager::LineSegmentBuffer::Clear()
        {
            SAFE_CLEAR(vab_position)
            SAFE_CLEAR(vab_color)
            SAFE_CLEAR(vab_list)
            SAFE_CLEAR(mesh);
            SAFE_CLEAR(primitive);
        }

        DrawLineManager::LineSegmentBuffer::~LineSegmentBuffer()
        {
            Clear();
        }

        bool DrawLineManager::LineSegmentBuffer::RebuildMesh()
        {
            Clear();

            AnsiString name = "Line3D(Width:" + AnsiString::numberOf(width) + ")";

            primitive = CreatePrimitive(device,mtl_inst->GetVIL(),name,max_count * 2);

            if(!primitive)
                return(false);

            mesh=DirectCreateMesh(primitive,mtl_inst,pipeline);

            {
                vab_list->Restart();
                vab_list->Add(mesh->GetDataBuffer());
            }

            vab_position=new VABMap3f(primitive->GetVABMap(VAN::Position));
            vab_color=new VABMap1u8(primitive->GetVABMap(VAN::Color));

            position=vab_position->Map();
            color=vab_color->Map();

            return(true);
        }

        void DrawLineManager::LineSegmentBuffer::AddCount(uint c)
        {
            if(count<=0)return;

            count+=c;

            if(count >= max_count)
            {
                max_count+=LINE_COUNT_INCREMENT;

                Clear();
                RebuildMesh();
            }
        }

        void DrawLineManager::LineSegmentBuffer::AddLine(const Vector3f &from,const Vector3f &to,uint8_t color_index)
        {
            AddCount(1);

            position->Write(from);
            position->Write(to);

            color->Write(color_index);
            color->Write(color_index);

            mesh->SetDrawCounts(count);

            dirty = true;
        }

        void DrawLineManager::LineSegmentBuffer::AddLine(const DataArray<LineSegmentInfo> &lsi_list)
        {
            AddCount(lsi_list.GetCount());

            for(auto &lsi:lsi_list)
            {
                position->Write(lsi.from);
                position->Write(lsi.to);

                color->Write(lsi.color);
                color->Write(lsi.color);
            }

            mesh->SetDrawCounts(count);            
        }

        void DrawLineManager::LineSegmentBuffer::Update()
        {
            if(count<=0||!dirty)
                return;

            RebuildMesh();
        }

        void DrawLineManager::LineSegmentBuffer::Draw(RenderCmdBuffer *cmd)
        {
            cmd->BindVAB(vab_list);

            cmd->Draw(mesh->GetDataBuffer(),mesh->GetRenderData());
        }

        DrawLineManager::DrawLineManager(MaterialInstance *mi,Pipeline *p,UBOLineColorPattle *lcp,MeshManager *mm)
        {
            mi_line3d = mi;
            pipeline=p;
            ubo_color=lcp;
            mesh_manager = mm;

            for(uint i = 0;i < MAX_LINE_WIDTH;i++)
            {
                line_groups[i].Init(i + 1,mm->GetDevice(),mi,pipeline);
            }
        }

        DrawLineManager::~DrawLineManager()
        {
            delete ubo_color;
        }

        void DrawLineManager::SetColor(const int index, const Color4f& c)
        {
            if (index < 0 || index >= 256) return;

            ubo_color->Write(&c,index*sizeof(Color4f),sizeof(Color4f));
        }

        bool DrawLineManager::AddLine(const Vector3f& from, const Vector3f& to, const uint8_t color_index, uint8_t width)
        {
            if (width == 0
              ||width > MAX_LINE_WIDTH)
                return(false);

            line_groups[width-1].AddLine(from,to,color_index);
        }

        bool DrawLineManager::AddLine(const uint8_t width,const DataArray<LineSegmentInfo> &lsi_list)
        {
            if(width==0
               ||width>MAX_LINE_WIDTH)
                return(false);

            if(lsi_list.IsEmpty())
                return(false);

            line_groups[width-1].AddLine(lsi_list);
        }

        void DrawLineManager::ClearLines()
        {
            for(auto &lsb : line_groups)
                lsb.count = 0;

            line_dirty = true;
        }

        bool DrawLineManager::Draw(RenderCmdBuffer *cmd)
        {
            if (!cmd)
                return(false);

            UpdateLines();

            if(!cmd->Begin())
                return(false);

            mi_line3d->GetMaterial()->BindUBO(&SBS_ColorPattle,ubo_color->ubo());

            cmd->BindPipeline(pipeline);
            cmd->BindDescriptorSets(mi_line3d->GetMaterial());

            for(size_t i = 0;i < MAX_LINE_WIDTH;i++)
            {
                if(line_groups[i].count <= 0)
                    continue;

                cmd->SetLineWidth(i + 1);

                line_groups[i].Draw(cmd);
            }

            for(auto &lsb:line_groups)
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

