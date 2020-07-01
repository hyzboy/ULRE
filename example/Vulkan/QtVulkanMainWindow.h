#pragma once
#include<QWidget>
#include<QPlainTextEdit>
#include<QVBoxLayout>
#include"QtVulkanWindow.h"

class MainWindow:public QWidget
{
    Q_OBJECT

private:

    QtVulkanWindow *m_window;
    QPlainTextEdit *m_info;

public slots:

    void onVulkanInfoReceived(const QString &text);

public:

    MainWindow(QtVulkanWindow *w);
    virtual ~MainWindow()=default;
};//