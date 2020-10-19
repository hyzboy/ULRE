#pragma once

#include<hgl/gui/ThemeEngine.h>
#include<hgl/graph/vulkan/VKMaterialInstance.h>
#include<hgl/type/Map.h>

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
            struct IForm
            {
                Form *form;                                 ///<窗体控件
                vulkan::RenderTarget *rt;                   ///<渲染目标
            };//

            MapObject<Form *,IForm> form_list;

            struct
            {
                vulkan::Material *          m;
                vulkan::MaterialInstance *  mi;
            }panel;

        public:

            bool Init() override;
            void Clear() override;

            bool Registry(Form *);
            void Unregistry(Form *);
            void Render(Form *);
        };//class DefaultThemeEngine:public ThemeEngine
    }//namespace gui
}//namespace hgl