#include "FontDataSourceFreeType.h"
#include <string>
#include <cstring>

namespace hgl::graph
{
    FreeTypeBitmapFont::FreeTypeBitmapFont(const Font &f) : FontBitmapDataSource(f)
    {
        ft_library = nullptr;
        ft_face = nullptr;
        buffer = nullptr;
        buffer_size = 0;
        
        // Initialize FreeType library
        FT_Error error = FT_Init_FreeType(&ft_library);
        if (error)
        {
            // Failed to initialize FreeType library
            return;
        }

        // Try to load font face
        // First try as a file path, then as a system font name
        error = FT_New_Face(ft_library, reinterpret_cast<const char*>(fnt.name), 0, &ft_face);
        if (error)
        {
            // If loading as file failed, try to find common system fonts
            // This is a simplified approach - a real implementation might use fontconfig
            const char* fallback_fonts[] = {
                "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
                "/usr/share/fonts/TTF/DejaVuSans.ttf",
                "/System/Library/Fonts/Arial.ttf", // macOS
                "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf"
            };
            
            bool loaded = false;
            for (const char* font_path : fallback_fonts)
            {
                error = FT_New_Face(ft_library, font_path, 0, &ft_face);
                if (!error)
                {
                    loaded = true;
                    break;
                }
            }
            
            if (!loaded)
            {
                // Failed to load any font face
                FT_Done_FreeType(ft_library);
                ft_library = nullptr;
                return;
            }
        }

        // Set character size
        error = FT_Set_Pixel_Sizes(ft_face, fnt.width, fnt.height);
        if (error)
        {
            // Failed to set font size, but we can continue with default
        }

        // Set up character encoding
        FT_Select_Charmap(ft_face, FT_ENCODING_UNICODE);

        // Allocate buffer for glyph data
        buffer_size = fnt.width * fnt.height * 4;
        buffer = new uint8[buffer_size];
        memset(buffer, 0, buffer_size);
    }

    FreeTypeBitmapFont::~FreeTypeBitmapFont()
    {
        if (buffer)
        {
            delete[] buffer;
            buffer = nullptr;
        }
        
        if (ft_face)
        {
            FT_Done_Face(ft_face);
            ft_face = nullptr;
        }
        
        if (ft_library)
        {
            FT_Done_FreeType(ft_library);
            ft_library = nullptr;
        }
    }

    bool FreeTypeBitmapFont::MakeCharBitmap(FontBitmap *bmp, u32char ch)
    {
        if (!ft_face || !ft_library)
            return false;

        // Load glyph
        FT_UInt glyph_index = FT_Get_Char_Index(ft_face, ch);
        if (glyph_index == 0)
            return false; // Character not found in font

        FT_Error error = FT_Load_Glyph(ft_face, glyph_index, FT_LOAD_DEFAULT);
        if (error)
            return false;

        // Render glyph to bitmap
        error = FT_Render_Glyph(ft_face->glyph, 
                               fnt.anti ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO);
        if (error)
            return false;

        FT_GlyphSlot slot = ft_face->glyph;
        FT_Bitmap &bitmap = slot->bitmap;

        // Set up metrics info
        bmp->metrics_info.w = bitmap.width;
        bmp->metrics_info.h = bitmap.rows;
        bmp->metrics_info.x = slot->bitmap_left;
        bmp->metrics_info.y = slot->bitmap_top;
        bmp->metrics_info.adv_x = slot->advance.x >> 6; // Convert from 26.6 fixed point
        bmp->metrics_info.adv_y = slot->advance.y >> 6;

        // Align width to 4 bytes as in the original implementation
        // Simple alignment function since hgl_align is not available
        bmp->metrics_info.w = (bmp->metrics_info.w + 3) & ~3;
        bmp->metrics_info.h = (bmp->metrics_info.h + 3) & ~3;

        // Allocate bitmap data - use simple new instead of hgl_zero_new
        const int data_size = bmp->metrics_info.w * bmp->metrics_info.h;
        bmp->data = new uint8[data_size];
        memset(bmp->data, 0, data_size);

        if (!bmp->data)
            return false;

        // Copy bitmap data
        if (bitmap.buffer && bitmap.width > 0 && bitmap.rows > 0)
        {
            if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY)
            {
                // 8-bit grayscale
                for (unsigned int y = 0; y < bitmap.rows && y < bmp->metrics_info.h; ++y)
                {
                    uint8* src_row = bitmap.buffer + y * bitmap.pitch;
                    uint8* dst_row = bmp->data + y * bmp->metrics_info.w;
                    
                    for (unsigned int x = 0; x < bitmap.width && x < bmp->metrics_info.w; ++x)
                    {
                        dst_row[x] = src_row[x];
                    }
                }
            }
            else if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
            {
                // 1-bit monochrome
                for (unsigned int y = 0; y < bitmap.rows && y < bmp->metrics_info.h; ++y)
                {
                    uint8* src_row = bitmap.buffer + y * bitmap.pitch;
                    uint8* dst_row = bmp->data + y * bmp->metrics_info.w;
                    
                    for (unsigned int x = 0; x < bitmap.width && x < bmp->metrics_info.w; ++x)
                    {
                        int src_byte_idx = x >> 3;
                        int src_bit_idx = 7 - (x & 7);
                        uint8 bit = (src_row[src_byte_idx] >> src_bit_idx) & 1;
                        dst_row[x] = bit ? 255 : 0;
                    }
                }
            }
        }

        return true;
    }

    FontDataSource *CreateFontSource(const Font &f)
    {
        return new FreeTypeBitmapFont(f);
    }
} // namespace hgl::graph