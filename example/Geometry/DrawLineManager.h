#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <array>

#include<hgl/math/Math.h>
#include<hgl/math/Vector.h>
#include<hgl/color/Color4f.h>
#include<hgl/type/String.h>
#include<hgl/graph/VKCommandBuffer.h>

// Forward declarations for GPU primitive creation
namespace hgl
{
    namespace graph
    {
        class VulkanDevice;
        class Primitive;
        class VIL;
        class Material;
        class MaterialInstance;
        class BufferManager;
        class PrimitiveManager;
        class MeshManager;
        class Pipeline;
        class MaterialManager;
    }
}

namespace hgl::graph::example
{
    // Simple CPU-side batched line manager.
    // Stores submitted lines (start/end in world space, packed 8-bit color per-vertex, 1-byte width).
    // Lines are grouped by width for efficient rendering. The Draw method is called
    // once per-frame by the user with an already configured RenderCmdBuffer.

    struct LineVertex
    {
        Vector3f start;
        Vector3f end;
        uint32_t color; // packed rgba8 (for legacy Draw callback)
    };

    class DrawLineManager
    {
    public:

        DrawLineManager() = default;

        DrawLineManager(hgl::graph::VulkanDevice *device,
                        const hgl::graph::VIL *vil,
                        hgl::graph::Material *material,
                        hgl::graph::MaterialInstance *material_instance,
                        hgl::graph::BufferManager *buffer_manager,
                        hgl::graph::PrimitiveManager *primitive_manager,
                        hgl::graph::MeshManager *mesh_manager,
                        hgl::graph::Pipeline *pipeline);

        ~DrawLineManager() { Clear(); }

        // Now specify colors for both endpoints as 4 uint8 components (r,g,b,a)
        void AddLine(const Vector3f &from, const Vector3f &to, const std::array<uint8_t,4> &color_from, const std::array<uint8_t,4> &color_to, uint8_t width = 1);
        void Clear();

        using DrawCallback = void(*)(RenderCmdBuffer *cmd, uint8_t width, const void *vertex_data, size_t vertex_count);

        void Draw(RenderCmdBuffer *cmd, DrawCallback draw_callback);

        size_t GetLineCount() const;

        void CreatePrimitives(const AnsiString &name_prefix,
                              const std::function<void(hgl::graph::Primitive *)> &on_create);

        bool Update();

    private:
        mutable std::mutex mutex_;
        // map width to list of line indices (index into positions_/colors_ as pairs)
        std::unordered_map<uint8_t, std::vector<uint32_t>> groups_; // key = width

        // internal contiguous memory blocks
        std::vector<Vector3f> positions_; // per-vertex positions (2 vertices per line)
        // per-vertex color bytes: RGBA bytes stored consecutively (vertex_count * 4 entries)
        std::vector<uint8_t> colors_; // 4 bytes per vertex

        // capacities in bytes for the blocks
        size_t positions_capacity_bytes_ = 0;
        size_t colors_capacity_bytes_ = 0;

        // number of lines currently stored
        size_t line_count_ = 0;

        // last uploaded count to GPU primitive
        uint32_t last_uploaded_count_ = 0;

        // GPU helpers provided at construction
        hgl::graph::VulkanDevice *device_ = nullptr;
        const hgl::graph::VIL *vil_ = nullptr;
        hgl::graph::Material *m_line3d = nullptr;
        hgl::graph::MaterialInstance *m_line3d_instance = nullptr;
        hgl::graph::BufferManager *buffer_manager_ = nullptr;

        hgl::graph::PrimitiveManager *primitive_manager_ = nullptr;
        hgl::graph::MeshManager *mesh_manager_ = nullptr;
        hgl::graph::Pipeline *pipeline_ = nullptr;

        hgl::graph::Primitive *primitive_ = nullptr;
        hgl::graph::Mesh *mesh_ = nullptr;

        bool dirty_ = false;

        // palette of 256 colors (packed RGBA8) kept for convenience
        uint32_t color_palette[256] = {};

        static constexpr size_t kIncreaseBytes = 1024 * 1024; // 1 MB increments
    };
}
