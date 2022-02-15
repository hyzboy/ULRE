#pragma once

#include<hgl/gui/ThemeEngine.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/type/Map.h>
#include"DefaultThemeForm.h"

namespace hgl
{
    namespace gui
    {
        using namespace hgl::graph;

        class Form;     ///<窗体

        /**
         * 缺省GUI主题引擎
         */
        class DefaultThemeEngine:public ThemeEngine
        {
            struct
            {
                Material *material;
                Pipeline *pieline;
            }panel;

        public:

            using ThemeEngine::ThemeEngine;
            virtual ~DefaultThemeEngine() override;

            bool Init() override;
            void Clear() override;

            ThemeForm *CreateForm(Form *,RenderTarget *,RenderCmdBuffer *) override;

        public:

            void DrawPanel(RenderCmdBuffer *,const RectScope2f &);
        };//class DefaultThemeEngine:public ThemeEngine
    }//namespace gui
}//namespace hgl
