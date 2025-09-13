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

namespace hgl { namespace graph { namespace example {

static inline uint32_t PackColorFromBytes(const std::array<uint8_t,4> &c)
{
    return (uint32_t(c[3])<<24) | (uint32_t(c[2])<<16) | (uint32_t(c[1])<<8) | uint32_t(c[0]);
}

DrawLineManager::DrawLineManager(VulkanDevice *device,
                                 const VIL *vil,
                                 Material *material,
                                 MaterialInstance *material_instance,
                                 BufferManager *buffer_manager,
                                 PrimitiveManager *primitive_manager,
                                 MeshManager *mesh_manager,
                                 Pipeline *pipeline)
{
    device_ = device;
    vil_ = vil;
    m_line3d = material;
    m_line3d_instance = material_instance;
    buffer_manager_ = buffer_manager;

    primitive_manager_ = primitive_manager;
    mesh_manager_ = mesh_manager;
    pipeline_ = pipeline;

    primitive_ = nullptr;
    mesh_ = nullptr;
    dirty_ = false;

    line_count_ = 0;
    last_uploaded_count_ = 0;
}

// Now specify colors for both endpoints as 4 uint8 components (r,g,b,a)
void DrawLineManager::AddLine(const Vector3f &from, const Vector3f &to, const std::array<uint8_t,4> &color_from, const std::array<uint8_t,4> &color_to, uint8_t width)
{
    uint32_t cfrom32 = PackColorFromBytes(color_from);

    std::lock_guard lock(mutex_);

    // index of new line
    uint32_t line_idx = static_cast<uint32_t>(line_count_);

    // push into group index list
    groups_[width].push_back(line_idx);

    // append to positions (two vertices per line)
    const size_t needed_positions = (line_count_ + 1) * 2 * sizeof(Vector3f);

    if(positions_capacity_bytes_ < needed_positions)
    {
        positions_capacity_bytes_ += kIncreaseBytes;
        positions_.reserve(positions_capacity_bytes_ / sizeof(Vector3f));
    }

    const size_t needed_colors = (line_count_ + 1) * 2 * 4; // 4 bytes per vertex
    if(colors_capacity_bytes_ < needed_colors)
    {
        colors_capacity_bytes_ += kIncreaseBytes;
        colors_.reserve(colors_capacity_bytes_);
    }

    // write positions
    const size_t pos_index = line_idx * 2;
    if(positions_.size() <= pos_index) positions_.push_back(from); else positions_[pos_index] = from;
    if(positions_.size() <= pos_index+1) positions_.push_back(to); else positions_[pos_index+1] = to;

    // write two colors (4 bytes each)
    const size_t col_byte_index = pos_index * 4; // bytes index

    if(colors_.size() <= col_byte_index+3)
    {
        // push r,g,b,a for first vertex
        colors_.push_back(color_from[0]);
        colors_.push_back(color_from[1]);
        colors_.push_back(color_from[2]);
        colors_.push_back(color_from[3]);
        // push r,g,b,a for second vertex
        colors_.push_back(color_to[0]);
        colors_.push_back(color_to[1]);
        colors_.push_back(color_to[2]);
        colors_.push_back(color_to[3]);
    }
    else
    {
        colors_[col_byte_index+0] = color_from[0];
        colors_[col_byte_index+1] = color_from[1];
        colors_[col_byte_index+2] = color_from[2];
        colors_[col_byte_index+3] = color_from[3];

        colors_[col_byte_index+4] = color_to[0];
        colors_[col_byte_index+5] = color_to[1];
        colors_[col_byte_index+6] = color_to[2];
        colors_[col_byte_index+7] = color_to[3];
    }

    // maintain legacy groups' representative color for Draw callback convenience
    LineVertex lv{from,to,cfrom32};
    // replace last stored representative if exists, else push
    // groups_ stores indices; nothing to replace here since groups_ holds indices only

    ++line_count_;

    dirty_ = true;
}

void DrawLineManager::Clear()
{
    std::lock_guard lock(mutex_);
    groups_.clear();

    positions_.clear();
    colors_.clear();

    positions_capacity_bytes_ = 0;
    colors_capacity_bytes_ = 0;

    line_count_ = 0;
    dirty_ = true;
}

void DrawLineManager::Draw(RenderCmdBuffer *cmd, DrawCallback draw_callback)
{
    if(cmd == nullptr || draw_callback == nullptr) return;

    std::lock_guard lock(mutex_);

    // Build temporary per-width LineVertex arrays from indices
    for(auto &kv : groups_)
    {
        uint8_t width = kv.first;
        auto &indices = kv.second;

        if(indices.empty()) continue;

        std::vector<LineVertex> tmp;
        tmp.reserve(indices.size());

        for(uint32_t idx : indices)
        {
            LineVertex lv;
            lv.start = positions_[idx*2];
            lv.end = positions_[idx*2+1];
            // Representative color from first vertex bytes
            size_t bidx = idx*2*4;
            lv.color = (uint32_t)colors_[bidx] | ((uint32_t)colors_[bidx+1]<<8) | ((uint32_t)colors_[bidx+2]<<16) | ((uint32_t)colors_[bidx+3]<<24);
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

// Factory implementation declared in RenderFramework.cpp
DrawLineManager *CreateDrawLineManager(VulkanDevice *device,
                                      const VIL *vil,
                                      MaterialManager *material_manager,
                                      PrimitiveManager *primitive_manager,
                                      MeshManager *mesh_manager,
                                      BufferManager *buffer_manager,
                                      Pipeline *pipeline)
{
    if(device == nullptr || vil == nullptr || material_manager == nullptr || buffer_manager == nullptr || primitive_manager == nullptr || mesh_manager == nullptr || pipeline == nullptr)
        return nullptr;

    // Create Line3D material via factory
    mtl::Line3DMaterialCreateConfig cfg;

    auto *mci = mtl::CreateLine3D(device->GetDevAttr(), &cfg);

    if(mci == nullptr)
        return nullptr;

    Material *mat = material_manager->CreateMaterial("M_Line3D", mci);
    if(mat == nullptr)
        return nullptr;

    MaterialInstance *mi = material_manager->CreateMaterialInstance(mat);
    if(mi == nullptr)
    {
        material_manager->Release(mat);
        return nullptr;
    }

    DrawLineManager *mgr = new DrawLineManager(device, vil, mat, mi, buffer_manager, primitive_manager, mesh_manager, pipeline);

    return mgr;
}

bool DrawLineManager::Update()
{
    std::lock_guard lock(mutex_);

    if(dirty_ == false)
        return true;

    // determine whether we need to upload (size changed)
    const bool need_upload_flag = (last_uploaded_count_ != static_cast<uint32_t>(line_count_));

    if(need_upload_flag == false)
    {
        dirty_ = false; // nothing changed in buffer size
        return true;
    }

    // Release previous primitive/mesh
    if(mesh_ != nullptr)
    {
        if(mesh_manager_ != nullptr) mesh_manager_->Release(mesh_);
        mesh_ = nullptr;
    }

    if(primitive_ != nullptr)
    {
        if(primitive_manager_ != nullptr) primitive_manager_->Release(primitive_);
        else delete primitive_;
        primitive_ = nullptr;
    }

    if(line_count_==0)
    {
        last_uploaded_count_ = 0;
        dirty_ = false;
        return true;
    }

    // vertex_count = line_count * 2
    const uint32_t vertex_count = static_cast<uint32_t>(line_count_ * 2);

    PrimitiveCreater pc(device_, vil_);

    if(pc.Init("DrawLineManager_Prim", vertex_count) == false)
    {
        dirty_ = false;
        return false;
    }

    const uint32_t vabCount = vil_->GetVertexAttribCount();
    for(uint32_t i=0;i<vabCount;i++)
    {
        const VIF *vif=vil_->GetConfig(i);
        if(!vif) continue;
        AnsiString attr_name=vif->name;

        if(attr_name.CaseComp("position")==0 || attr_name.CaseComp("pos")==0 || attr_name.CaseComp("start")==0)
        {
            pc.WriteVAB(attr_name, vif->format, positions_.data());
        }
        else if(attr_name.CaseComp("color")==0)
        {
            if(vif->format==VK_FORMAT_R8G8B8A8_UNORM || vif->format==VK_FORMAT_R8G8B8A8_UINT || vif->format==VK_FORMAT_R8G8B8A8_SINT)
            {
                pc.WriteVAB(attr_name, vif->format, colors_.data());
            }
            else
            {
                std::vector<Vector4f> colf(vertex_count);
                for(uint32_t j=0;j<vertex_count;j++)
                {
                    colf[j].x = colors_[j*4+0] / 255.0f;
                    colf[j].y = colors_[j*4+1] / 255.0f;
                    colf[j].z = colors_[j*4+2] / 255.0f;
                    colf[j].w = colors_[j*4+3] / 255.0f;
                }
                pc.WriteVAB(attr_name, vif->format, colf.data());
            }
        }
    }

    primitive_ = pc.Create();
    if(primitive_ == nullptr)
    {
        dirty_ = false;
        return false;
    }

    if(primitive_manager_ != nullptr) primitive_manager_->Add(primitive_);

    if(mesh_manager_ != nullptr)
        mesh_ = mesh_manager_->CreateMesh(primitive_, m_line3d_instance, pipeline_);
    else
        mesh_ = hgl::graph::DirectCreateMesh(primitive_, m_line3d_instance, pipeline_);

    last_uploaded_count_ = static_cast<uint32_t>(line_count_);
    dirty_ = false;

    return mesh_ != nullptr;
}

}}} // namespace
