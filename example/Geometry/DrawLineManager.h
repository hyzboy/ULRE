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

namespace hgl::graph
{
    constexpr const size_t MAX_LINE_WIDTH = 16;

    class DrawLineManager
    {        
        struct LineSegmentBuffer
        {
            std::vector<float> position;
            std::vector<uint8> color;

            uint32 count;
        };

    private:

        Color4f color_palette[256] = {};

        bool color_dirty = false;

    private:

        LineSegmentBuffer line_groups[MAX_LINE_WIDTH];      //直接以宽度为访问索引

        bool line_dirty = false;

    private:    

        hgl::graph::MaterialInstance *mi_line3d = nullptr;
        hgl::graph::Pipeline *pipeline = nullptr;
        hgl::graph::MeshManager *mesh_manager = nullptr;

        VkFormat PositionFormat;

        PrimitiveCreater *primitive_creater = nullptr;

        hgl::graph::Mesh *mesh = nullptr;

        void UpdateLines();

    public:

        DrawLineManager(hgl::graph::MaterialInstance *material_instance,
                        hgl::graph::Pipeline *pipeline,
                        hgl::graph::MeshManager *mesh_manager);

        ~DrawLineManager() { ClearLines(); }

        void SetColor(const int index, const Color4f& c)
        {
            if (index < 0 || index >= 256) return;

            color_palette[index] = c;
        }

        void ClearColor(){color_dirty = true;}

        // Now specify colors by an index into the internal palette
        bool AddLine(const Vector3f& from, const Vector3f& to, const uint8_t color_index, uint8_t width = 1);
        void ClearLines();

        using DrawCallback = void(*)(RenderCmdBuffer* cmd, uint8_t width, const void* vertex_data, size_t vertex_count);

        void Draw(RenderCmdBuffer* cmd, DrawCallback draw_callback);

        size_t GetLineCount() const;

        void CreatePrimitives(const AnsiString& name_prefix,
            const std::function<void(hgl::graph::Primitive*)>& on_create);

        bool Update();
    };
}
