#ifndef HGL_GUI_THEME_ENGINE_INCLUDE
#define HGL_GUI_THEME_ENGINE_INCLUDE

#include<hgl/type/RectScope.h>
#include<hgl/type/Map.h>
#include<hgl/gui/ThemeForm.h>
namespace hgl
{
    namespace gui
    {
        namespace vulkan
        {
            class GPUDevice;
        }//namespace vulkan

        class ThemeEngine
        {
        protected:

            GPUDevice *device;

            MapObject<Form *,ThemeForm> form_list;

        public:

            ThemeEngine(GPUDevice *dev){device=dev;}
            virtual ~ThemeEngine()=default;

            virtual bool Init()=0;
            virtual void Clear()=0;
            
            virtual bool Registry(Form *)=0;
            virtual void Unregistry(Form *)=0;
            virtual void Render(Form *)=0;
            virtual bool Resize(Form *,const uint32_t,const uint32_t);
        };//class ThemeEngine

//        ThemeEngine *CreateThemeEngine();                   
        ThemeEngine *GetDefaultThemeEngine();                                   ///<获取缺省主题引擎
    }//namespace gui
}//namespace hgl
#endif//HGL_GUI_THEME_ENGINE_INCLUDE