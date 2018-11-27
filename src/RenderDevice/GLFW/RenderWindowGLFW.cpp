#include<hgl/render/RenderWindow.h>
#include<GLFW/glfw3.h>

namespace hgl
{
    class RenderWindowGLFW:public RenderWindow
    {
        GLFWwindow *glfw_win;

    public:

        RenderWindowGLFW(GLFWwindow *w,bool is_fullscreen)
        {
            glfw_win=w;
            full_screen=is_fullscreen;

            glfwGetWindowSize(glfw_win,&width,&height);
        }

        ~RenderWindowGLFW()
        {
            glfwDestroyWindow(glfw_win);
        }

        void ToMin()override{glfwIconifyWindow(glfw_win);}
        void ToMax()override{glfwMaximizeWindow(glfw_win);}

        void Show()override{glfwShowWindow(glfw_win);}
        void Hide()override{glfwHideWindow(glfw_win);}

        void SetCaption(const OSString &name) override
        {
#if HGL_OS == HGL_OS_Windows
            glfwSetWindowTitle(glfw_win,to_u8(name));
#else
            glfwSetWindowTitle(glfw_win,name);
#endif//
            this->caption=name;
        }

        void SetSize(int w,int h) override
        {
            glfwSetWindowSize(glfw_win,w,h);
            this->width=w;
            this->height=h;
        }

        void MakeToCurrent()override{glfwMakeContextCurrent(glfw_win);}

        void SwapBuffer()override{glfwSwapBuffers(glfw_win);}

        void WaitEvent(const double &time_out)override
        {
            if(time_out>0)
                glfwWaitEventsTimeout(time_out);
            else
                glfwWaitEvents();
        }

        void PollEvent()override{glfwPollEvents();}

        bool IsOpen()override{return(!glfwWindowShouldClose(glfw_win));}
    };//class RenderWindowGLFW:public RenderWindow

    RenderWindow *CreateRenderWindowGLFW(GLFWwindow *win,bool is_fullscreen)
    {
        return(new RenderWindowGLFW(win,is_fullscreen));
    }
}//namespace hgl
