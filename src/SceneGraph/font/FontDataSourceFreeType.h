#pragma once

#include <hgl/graph/font/FontSource.h>

// Forward declarations to avoid pulling in complex dependencies
namespace hgl { template<typename T> class DataArray; }

#include <ft2build.h>
#include FT_FREETYPE_H

namespace hgl::graph
{
    class FreeTypeBitmapFont : public FontBitmapDataSource
    {
        FT_Library ft_library;
        FT_Face ft_face;
        
        // Simple buffer for glyph data - will implement as needed
        uint8 *buffer;
        int buffer_size;

    public:

        FreeTypeBitmapFont(const Font &f);
        ~FreeTypeBitmapFont();

        bool MakeCharBitmap(FontBitmap *bmp, u32char ch) override;  ///< 产生字体数据
    }; // class FreeTypeBitmapFont
} // namespace hgl::graph