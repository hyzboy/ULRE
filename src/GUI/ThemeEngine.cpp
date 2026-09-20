// ============================================================================
// [已停用 / 待重写] 请勿直接启用本文件
//
// src/GUI 已在 src/CMakeLists.txt 中被注释掉（#add_subdirectory(GUI)），
// 本文件长期未参与构建，已与当前引擎 API 脱节。已知问题（不止一处）：
//
//   1. CreateRT() 调用 device->CreateRT()，该方法在 VulkanDevice 上并不存在。
//      离屏 RT 现在的标准入口是 RenderTargetManager::Create(desc)，但
//      ThemeEngine 只持有 VulkanDevice，拿不到 GraphicsContext，
//      启用 GUI 前必须先给 ThemeEngine 注入 GraphicsContext。
//   2. Resize() 中 `if(!old_rt)` 后却解引用 old_rt —— 条件写反（应为 if(old_rt)），
//      是空指针解引用。
//   3. Resize() 用新 RT 覆盖 old_rt 前未释放旧 RT —— 内存泄漏。
//   4. Render(ThemeForm*) 缺少 return 语句 —— 未定义行为。
//   5. CreateThemeEngine() 调 GetDefaultThemeEngine() 少传 dev 参数。
//   6. CreateForm(f, rt) 少传 RenderCmdBuffer 参数（声明为三参数）。
//
// 结论：重新启用 GUI 前，本文件需连同 inc/hgl/gui/* 一并重写，
// 并以 RenderTargetDesc + OffscreenWorld 为新基线。参见
// doc/render-target-standardization-design.md 阶段 D。
// ============================================================================

#include<hgl/gui/ThemeEngine.h>
#include<hgl/gui/ThemeForm.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKDevice.h>

namespace hgl
{
    namespace gui
    {
        namespace
        {
            ThemeEngine *default_theme_engine=nullptr;
        }//namespace

        ThemeEngine *CreateDefaultThemeEngine(VulkanDevice *dev);

        ThemeEngine *GetDefaultThemeEngine(VulkanDevice *dev)
        {
            if(!default_theme_engine)
                default_theme_engine=CreateDefaultThemeEngine(dev);

            return default_theme_engine;
        }

        ThemeEngine *CreateThemeEngine(VulkanDevice *dev)
        {
            return GetDefaultThemeEngine();
        }

        RenderTarget *ThemeEngine::CreateRT(const uint32_t w,const uint32_t h,const VkFormat format)
        {
            const uint width=power_to_2(w);
            const uint height=power_to_2(h);

            FramebufferInfo fbi(format,w,h);

            return device->CreateRT(&fbi);
        }

        bool ThemeEngine::Registry(Form *f,const VkFormat format)
        {
            if(!f)return(false);

            if(form_list.ContainsKey(f))
                return(false);

            Vector2f size=f->GetSize();

            RenderTarget *rt=CreateRT(size.x,size.y,format);

            if(!rt)return(false);

            ThemeForm *tf=CreateForm(f,rt);

            form_list.Add(f,tf);

            return(true);
        }

        void ThemeEngine::Unregistry(Form *f)
        {
            if(!f)return;

            ThemeForm *tf;

            if(!form_list.Get(f,tf))
                return;

            delete tf;
            form_list.DeleteByKey(f);
        }

        bool ThemeEngine::Resize(Form *f,const uint32_t w,const uint32_t h,const VkFormat format)
        {
            if(!f)return(false);

            ThemeForm *tf;

            if(!form_list.Get(f,tf))return(false);

            if(w<=0||h<=0)
            {
                tf->SetRenderTarget(nullptr);
                return(true);
            }

            RenderTarget *old_rt=tf->GetRenderTarget();

            if(old_rt)      // 原为 if(!old_rt)，条件写反会导致空指针解引用
            {
                const VkExtent2D old_size=old_rt->GetExtent();

                if(old_size.width>=w
                 &&old_size.height>=h)
                {
                    tf->Resize(w,h);
                    return(true);
                }
            }

            graph::RenderTarget *rt=CreateRT(w,h,format);

            if(!rt)return(false);

            tf->SetRenderTarget(rt);
            tf->Resize(w,h);
            return(true);
        }

        bool ThemeEngine::Render(ThemeForm *tf)
        {
            tf->BeginRender();

            tf->Render();

            tf->EndRender();
        }

        bool ThemeEngine::Render(Form *f)
        {
            if(!f)return(false);

            const Vector2f &size=f->GetSize();

            if(size.x==0&&size.y==0)return(false);

            ThemeForm *tf;

            if(!form_list.Get(f,tf))
                return(false);

            return Render(tf);
        }
    }//namespace gui
}//namespace hgl
