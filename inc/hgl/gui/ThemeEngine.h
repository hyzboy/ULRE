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

        constexpr VkFormat DefaultRenderTargetFormat=UFMT_ABGR8;                ///<缺省窗体绘图表面格式

        class ThemeEngine
        {
        protected:

            GPUDevice *device;

            MapObject<Form *,ThemeForm> form_list;

            RenderTarget *CreateRenderTarget(const uint32_t,const uint32_t,const VkFormat);

        protected:

            virtual ThemeForm *CreateForm(Form *,RenderTarget *,RenderCommand *)=0;

        public:

            ThemeEngine(GPUDevice *dev){device=dev;}
            virtual ~ThemeEngine()=default;

            virtual bool Init()=0;
            virtual void Clear()=0;

            virtual bool Registry(Form *,const VkFormat format=DefaultRenderTargetFormat);
            virtual void Unregistry(Form *);
            virtual bool Resize(Form *,const uint32_t,const uint32_t,const VkFormat format=DefaultRenderTargetFormat);
            virtual bool Render(Form *);
        };//class ThemeEngine

//        ThemeEngine *CreateThemeEngine();                   
        ThemeEngine *GetDefaultThemeEngine();                                   ///<获取缺省主题引擎
    }//namespace gui
}//namespace hgl
#endif//HGL_GUI_THEME_ENGINE_INCLUDE