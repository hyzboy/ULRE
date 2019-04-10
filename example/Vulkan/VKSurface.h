#ifndef HGL_GRAPH_VULKAN_SURFACE_INCLUDE
#define HGL_GRAPH_VULKAN_SURFACE_INCLUDE

#include"VK.h"

VK_NAMESPACE_BEGIN

class Surface
{
	VkInstance instance;
	VkSurfaceKHR surface;

public:

	Surface(VkInstance inst,VkSurfaceKHR surf)
	{
		instance = inst;
		surface = surf;
	}

	virtual ~Surface();
};//class Surface

VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_SURFACE_INCLUDE
