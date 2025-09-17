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

#include <hgl/graph/geo/line/LineWidthBatch.h>

namespace hgl::graph
{
    constexpr const size_t MAX_LINE_WIDTH = 16;

    using LineColorPalette=Color4f[256];

    using UBOLineColorPalette=UBOInstance<LineColorPalette>;

    class LineRenderManager
    {
        VulkanDevice *device;

    private:

        UBOLineColorPalette *ubo_color;

    private:

        LineWidthBatch line_groups[MAX_LINE_WIDTH];      //直接以宽度为访问索引

        uint32_t total_line_count=0;

    private:    

        MaterialInstance *  mi_line   = nullptr;
        Pipeline *          pipeline  = nullptr;

    public:

        SharedLineBackup *shared_backup = nullptr; // shared backup used by LineWidthBatch

        LineRenderManager(VulkanDevice *dev,MaterialInstance *mi,Pipeline *p,UBOLineColorPalette *lcp);

        ~LineRenderManager();

        void SetColor(const int index, const Color4f& c);

        // Now specify colors by an index into the internal palette
        bool AddLine(const Vector3f& from, const Vector3f& to, const uint8_t color_index, uint8_t width = 1);

        bool AddLine(const uint8_t width,const DataArray<LineSegmentDescriptor> &);
        void ClearLines();

        bool Draw(RenderCmdBuffer *);

        size_t GetLineCount() const {return total_line_count;}

        void Update();
    };
}
