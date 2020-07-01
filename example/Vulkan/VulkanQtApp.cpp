#include<hgl/QT.h>
#include<QObject>
#include<QApplication>
#include<QVulkanInstance>
#include"QtVulkanMainWindow.h"
#include<hgl/graph/vulkan/VKDevice.h>

using namespace hgl;
using namespace hgl::graph;

//vulkan::Device *CreateRenderDevice(QVulkanWindow *qvw)
//{
//    VkDevice vk_device=qvw->device();
//    VkPhysicalDevice vk_pdevice=qvw->physicalDevice();
//
//
//
//}

//class VulkanQtAppFramework
//{
//    QVulkanInstance qv_inst;
//    QtVulkanWindow *qv_win;
//    
//public:
//
//    VulkanQtAppFramework()
//    {
//    }
//
//};//

int main(int argc,char **argv)
{
    QApplication qt_app(argc,argv);

    QVulkanInstance qv_inst;

    if(!qv_inst.create())
        return -1;

    QtVulkanWindow *qv_win=new QtVulkanWindow;
    qv_win->setVulkanInstance(&qv_inst);

    MainWindow win(qv_win);
    
    QObject::connect(qv_win,&QtVulkanWindow::vulkanInfoReceived,&win,&MainWindow::onVulkanInfoReceived);

    win.resize(1280,720);
    win.show();

    qt_app.exec();
    return 0;
}

