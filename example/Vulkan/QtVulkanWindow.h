#pragma once
#include<QVulkanWindow>

class VulkanRenderer:public QVulkanWindowRenderer
{
private:

    QVulkanWindow *m_window;
    QVulkanDeviceFunctions *m_devFuncs;
    
public:

    VulkanRenderer(QVulkanWindow *w):m_window(w){}
    virtual ~VulkanRenderer()=default;

    void initResources() override;
    void initSwapChainResources() override;
    void releaseSwapChainResources() override;
    void releaseResources() override;
    void startNextFrame() override;
};//class VulkanRenderer:public QVulkanWindowRenderer

class QtVulkanWindow:public QVulkanWindow
{
    Q_OBJECT

signals:

    void vulkanInfoReceived(const QString &text);

public:

    using QVulkanWindow::QVulkanWindow;
    virtual ~QtVulkanWindow()=default;

    QVulkanWindowRenderer *createRenderer() override
    {
        return(new VulkanRenderer(this));
    }
};
