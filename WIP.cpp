2019.05.05 00:27

    需建立Mesh类，独立提供数据，但又与Renderable类不同。
    Mesh类提供原始的缓冲区数据与图元类型

    Renderable提供可供Vulkan使用的缓冲区类型，并且其中可能包含多个MESH的数据
    比如有多种形状的石头，它们使用了同一种材质，这种情况下多个mesh就可以合并到一个Renderable中，渲染时不再切换VBO。

    SceneNode提供当前节点的变换矩阵，以及mesh、灯光、摄像机等信息。

    工作计划：

        1.使用yaml保存pipeline信息
        2.建立scene数据文件。
            scene主体依然使用yaml描述
            mesh,信息使用二进制独立保存
