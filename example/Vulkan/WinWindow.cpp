#include"Window.h"
#include<vulkan/vk_sdk_platform.h>
#include<vulkan/vulkan.h>
#include<vulkan/vulkan_win32.h>

namespace hgl
{
	namespace graph
	{
		/**
		 * Windows平台窗口实现
		 */
		class WinWindow :public Window
		{
		public:

			using Window::Window;
			~WinWindow()
			{

			}

			const char* GetVulkanSurfaceExtname()const
			{
				return VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
			}

			bool Create(uint w, uint h) override
			{
			}

			bool Create(uint, uint, uint)override {}
			void Close()override
			{
			}

			void Show()override {}
			void Hide()override {}
		};//class WinWindow :public Window

		Window* CreateRenderWindow(const WideString& win_name)
		{
			return(new WinWindow(win_name));
		}
	}//namespace graph
}//namespace hgl
