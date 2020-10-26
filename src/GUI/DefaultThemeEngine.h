#pragma once

#include<hgl/gui/ThemeEngine.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/type/Map.h>
#include"DTForm.h"

namespace hgl
{
    namespace gui
    {
        using namespace hgl::graph;

        class Form;     ///<窗体

        namespace default_theme
        {
            /**
             * 缺省GUI主题引擎
             */
            class DefaultThemeEngine:public ThemeEngine
            {

                //struct
                //{
                //    Material *          m;
                //    MaterialInstance *  mi;
                //}panel;

            public:

                DefaultThemeEngine(GPUDevice *dev):ThemeEngine(dev){}
                virtual ~DefaultThemeEngine() override;

                bool Init() override;
                void Clear() override;
                
                bool ThemeRender(Form *) override;
            };//class DefaultThemeEngine:public ThemeEngine
        }//namespace default_theme
    }//namespace gui
}//namespace hgl