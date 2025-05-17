# 程序内嵌材质/Shader
# Material/Shader embedded in program code

问题(Question):

```
即然可以从文件中加载材质了，那为什么还需要有程序代码中内嵌的材质/Shader呢？

I can load a material from a file. Why do we need a few materials/shaders embedded in the code?
```

这个问题很好，答案也很简单：

Good question, the answer is straightforward:

```
我们需要在资产损坏或丢失的情况下，依然可以渲染一些内容, 比如: 一个报错对话框。

We need to be able to render some content, such as an error dialog box, even if the asset is damaged or lost.
```
