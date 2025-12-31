# ULRE
experiment project - Ultra Lightweight Render Engine

ULRE is a project of an experimental nature. Used to experiment with various rendering-related techniques And do some examples.

In the future, its complicated version will be integrated into CMGameEngine.Used to replace the old rendering engine.

Platform: Windows, Linux (WIP: Android, macOS, iOS)

Graphics API: Vulkan

Milestone:

    2. Texture2DArray was integrated into material instances. 
    Multiple render instances of the same model use different material instances and textures but are still merged into one render. 
    Although the changes this time are small, they are more significant.
    
    1. a test was completed, and the entire scene was drawn with only one DrawCall. 
    Although it still has a lot of unfinished work, it is still a significant milestone.

#
ULRE是一个试验性质的工程，用于试验各种渲染相关的技术，以及做一些范例。在未来它的复杂化版本会被整合到CMGameEngine中，用于替代旧的渲染引擎。

平台: Windows, Linux (开发中: Android, macOS, iOS)

图形API: Vulkan

里程碑:

    2. Texture2DArray集成在材质实例中。同一个模型的多个渲染实例使用不同的材质实例以及不同的纹理，
    但它们依然被合并成一次渲染。这次的改变虽小，但意义更为重大。
    
    1. 完成了一个测试，只用了一次DrawCall就绘制出了整个场景。虽然它还有很多未完成的工作，但它依然是一个非常重要的里程碑。
