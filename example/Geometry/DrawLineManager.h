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
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKCommandBuffer.h>

namespace hgl::graph
{
    constexpr const size_t MAX_LINE_WIDTH = 16;

    struct LineSegmentInfo
    {
        Vector3f from;
        Vector3f to;

        uint8 color;
    };

    using LineColorPattle=Color4f[256];

    using UBOLineColorPattle=UBOInstance<LineColorPattle>;

    class DrawLineManager
    {
        struct LineSegmentBuffer
        {
            VulkanDevice *      device  =nullptr;
            MaterialInstance *  mtl_inst=nullptr;
            Pipeline *          pipeline=nullptr;

            uint width=0;

            uint32 max_count=0;     // 当前缓冲区可容纳的最大线段数量
            uint32 count    =0;     // 当前线段数量

            Primitive * primitive   =nullptr;
            Mesh *      mesh        =nullptr;
            VABMap3f *  vab_position=nullptr;
            VABMap1u8 * vab_color   =nullptr;

            VB3f *      position    =nullptr;   // 每条线段2个顶点，每个顶点3个float
            VB1u8 *     color       =nullptr;   // 每条线段2个顶点，每个顶点4个uint8

            bool        dirty       =true;

        public:

            ~LineSegmentBuffer();

            void Init(const uint w,VulkanDevice *,MaterialInstance *,Pipeline *p);
            void Clear();
            bool RebuildMesh();
            void AddCount(uint);

            void AddLine(const Vector3f &from,const Vector3f &to,uint8_t color_index);
            void AddLine(const DataArray<LineSegmentInfo> &);

            uint32_t Update();

            void Draw(RenderCmdBuffer *);
        };

    private:

        UBOLineColorPattle *ubo_color;

    private:

        LineSegmentBuffer line_groups[MAX_LINE_WIDTH];      //直接以宽度为访问索引

        uint32_t total_line_count=0;

    private:    

        MaterialInstance *  mi_line3d           = nullptr;
        Pipeline *          pipeline            = nullptr;
        MeshManager *       mesh_manager        = nullptr;

        void UpdateLines();

    public:

        DrawLineManager(MaterialInstance *mi,
                        Pipeline *p,
                        UBOLineColorPattle *lcp,
                        MeshManager *mm);

        ~DrawLineManager();

        void SetColor(const int index, const Color4f& c);

        // Now specify colors by an index into the internal palette
        bool AddLine(const Vector3f& from, const Vector3f& to, const uint8_t color_index, uint8_t width = 1);

        bool AddLine(const uint8_t width,const DataArray<LineSegmentInfo> &);
        void ClearLines();

        bool Draw(RenderCmdBuffer *);

        size_t GetLineCount() const;

        void CreatePrimitives(const AnsiString& name_prefix,
            const std::function<void(hgl::graph::Primitive*)>& on_create);

        void Update();
    };
}
