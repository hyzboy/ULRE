#ifndef HGL_RENDER_WINDOW_INCLUDE
#define HGL_RENDER_WINDOW_INCLUDE

#include<hgl/type/Vertex2.h>
#include<hgl/type/BaseString.h>
namespace hgl
{
    /**
    * 渲染窗口基类
    */
    class RenderWindow
    {
    protected:

        OSString caption;
        bool full_screen;

        int width,height;

    public:

        const uint      GetWidth()const{return width;}
        const uint      GetHeight()const{return height;}
        const bool      GetFullScreen()const{return full_screen;}

    public:	//方法

        RenderWindow()=default;                                                                     ///<本类构造函数
        virtual ~RenderWindow()=default;                                                            ///<本类析构函数

        virtual void ToMin()=0;																        ///<窗口最小化
        virtual void ToMax()=0;																        ///<窗口最大化

        virtual void Show()=0;																		///<显示窗口
        virtual void Hide()=0;																		///<隐藏窗口

        virtual const OSString &GetCaption()const{return caption;}
        virtual void SetCaption(const OSString &)=0;

    public:	//被实际操作系统接口层所调用的函数，在不了解的情况下请不要使用

        virtual void SetSize(int w,int h)=0;														///<设置窗口大小

        virtual void MakeToCurrent()=0;																///<切换到当前
        virtual void SwapBuffer()=0;																///<交换缓冲区
        virtual void WaitEvent(const double &time_out=0)=0;                                         ///<等待下一个事件
        virtual void PollEvent()=0;                                                                 ///<轮询事件
        virtual bool IsOpen()=0;                                                                    ///<是否依然存在
    };//class RenderWindow
}//namespace hgl
#endif//HGL_RENDER_WINDOW_INCLUDE
