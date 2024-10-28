#pragma once

#include<hgl/graph/VKNamespace.h>
#include<hgl/TypeFunc.h>

VK_NAMESPACE_BEGIN

enum class WindowMode
{
    /**
    * 全屏模式是一种独占模式，它会占用整个屏幕，不允许其它程序显示在其上。
    * 这种模式下，程序的画面会直接输出到屏幕，不经过桌面合成器，拥有最高效能。
    * 但这种模式下，如果频率切换到其它程序，可能会导致屏幕闪烁或是设备丢失，严重会程序崩溃。
    */
    FullScreen,             ///<全屏模式

    /**
    * 窗口模式
    * 调试时期最常用的模式。画面会输出到一个FBO，再经桌面管理器合成整个桌面画面再输出到屏幕。
    */
    Windowed,               ///<窗口模式

    /**
    * 无边框窗口模式
    * 只不过是去掉了标题框和边框的窗口模式而己，这些东西一般经过窗口管理器或桌面管理器绘制。
    */
    Borderless,             ///<无边框窗口模式

    /**
    * 为什么需要有全屏无边框模式?
    *     这个模式虽然看起来是全屏的，但其实它只是一个占满屏幕的窗口而己，所以它的画面依然需要经过桌面合成器，才会输出到屏幕。
    * 这种模式对于需要频率切换到其它程序而言(比如修改器，或是需要挂机到后台的网游)，拥有良好的兼容性。
    */
    FullScreenBorderless,   ///<全屏无边框模式

    ENUM_CLASS_RANGE(FullScreen,FullScreenBorderless)
};//enum class WindowMode

VK_NAMESPACE_END
