#pragma once

#include<hgl/gui/ThemeEngine.h>
#include<hgl/graph/vulkan/VKMaterialInstance.h>

namespace hgl
{
    namespace gui
    {
        using namespace hgl::graph;

        /**
         * 缺省GUI主题引擎
         */
        class DefaultThemeEngine:public ThemeEngine
        {

            struct
            {
                vulkan::Material *          m;
                vulkan::MaterialInstance *  mi;
            }panel;

        public:

            bool Init() override;
            void Clear() override;

            void DrawFrame(const Widget *) override;
        };//class DefaultThemeEngine:public ThemeEngine
    }//namespace gui
}//namespace hgl