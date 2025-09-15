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
            VulkanDevice *device;
            MaterialInstance *mtl_inst;
            Pipeline *pipeline;

            uint width;

            std::vector<float> position;
            std::vector<uint8> color;

            uint32 max_count;       // 当前缓冲区可容纳的最大线段数量
            uint32 count;           // 当前线段数量

            Primitive *primitive;
            Mesh *mesh;

        public:

            LineSegmentBuffer(const uint w,VulkanDevice *,MaterialInstance *,Pipeline *p);
            ~LineSegmentBuffer();

            void Clear();
            bool RebuildMesh();
        };

    private:

        Color4f color_palette[256] = {};

        bool color_dirty = true;

    private:

        LineSegmentBuffer line_groups[MAX_LINE_WIDTH];      //直接以宽度为访问索引

        bool line_dirty = true;

    private:    

        RenderTarget *      render_target       = nullptr;
        MaterialInstance *  mi_line3d           = nullptr;
        Pipeline *          pipeline            = nullptr;
        MeshManager *       mesh_manager        = nullptr;


        void UpdateLines();

    public:

        DrawLineManager(RenderTarget *rt,
                        MaterialInstance *mi,
                        MeshManager *mm);

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

        bool Draw(RenderCmdBuffer *);

        size_t GetLineCount() const;

        void CreatePrimitives(const AnsiString& name_prefix,
            const std::function<void(hgl::graph::Primitive*)>& on_create);

        bool Update();
    };
}
