#pragma once

#include<hgl/gui/ThemeEngine.h>

namespace hgl
{
    namespace gui
    {
        /**
         * 缺省GUI主题引擎
         */
        class DefaultThemeEngine:public ThemeEngine
        {
        public:

            bool Init() override;
            void Clear() override;

            void DrawFrame(const RectScope2f &) override;

        };//class DefaultThemeEngine:public ThemeEngine
    }//namespace gui
}//namespace hgl