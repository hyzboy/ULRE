#pragma once

#include<hgl/graph/font/FontSource.h>
#include<hgl/type/ValueBuffer.h>
#include<windows.h>

namespace hgl::graph
{
    class WinBitmapFont:public FontBitmapDataSource
    {
        HDC hdc;
        HFONT hfont;    		
            
        GLYPHMETRICS gm;
        MAT2 mat;

        uint ggo;

        ValueBuffer<uint8> buffer;

    public:

        WinBitmapFont(const Font &);
        ~WinBitmapFont();

        bool MakeCharBitmap(FontBitmap *,u32char) override;					///<产生字体数据
    };//class WinBitmapFont
}//namespace hgl::graph
