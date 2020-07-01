#include"QtVulkanWindow.h"

void VulkanRenderer::initResources()
{
    m_devFuncs=m_window->vulkanInstance()->deviceFunctions(m_window->device());

}

void VulkanRenderer::initSwapChainResources()
{
}

void VulkanRenderer::releaseSwapChainResources()
{
}

void VulkanRenderer::releaseResources()
{
}

void VulkanRenderer::startNextFrame()
{
    m_window->frameReady();
    m_window->requestUpdate(); // render continuously, throttled by the presentation rate        
}