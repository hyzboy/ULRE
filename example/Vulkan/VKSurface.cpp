#include"VKSurface.h"

VK_NAMESPACE_BEGIN
Surface::~Surface()
{
	vkDestroySurfaceKHR(instance, surface, nullptr);
}
VK_NAMESPACE_END
