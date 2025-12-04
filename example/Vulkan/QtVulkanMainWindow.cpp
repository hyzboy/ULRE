#include"QtVulkanMainWindow.h"

MainWindow::MainWindow(QtVulkanWindow *w):m_window(w)
{
    this->setWindowTitle(QString("Vulkan and QT"));

    QWidget *wrapper = QWidget::createWindowContainer(w);
    QVBoxLayout *layout = new QVBoxLayout;

    m_info=new QPlainTextEdit;
    m_info->setReadOnly(true);

    layout->addWidget(m_info,1);
    layout->addWidget(wrapper,5);
    setLayout(layout);
}

void MainWindow::onVulkanInfoReceived(const QString &text)
{
    m_info->setPlainText(text);
}