# 静态渲染管理

## 静态渲染的意义是什么?

    静态渲染管理中的“静态”并不是指在场景中静止不动的所有物件，而是说大部分时间都会出现在场景中的物件。它包含地形、花草树木、房屋、常见人物，武器装备等等。如果归划好，可以将绝大部分的物件都划入静态渲染中。

    它的意义为的是所有添加进管理器的模型原始资源，都会一直存在。与它们本身是否显示在画面中，并无关联。为的是避开对显存资源的动态调配。

    在静态渲染管理器中的所有模型，会根据顶点数据格式(Vertex Input Format)分类。每种格式所有模型共用一套超大VBO(Vertex Buffer Object)，以达到在渲染时渲染不同模型不会切换VBO的效果，并进一步达成“一个格式，一次Drawcall全部画完”的极致效果。

## 材质的定义

    1.Material 材质可以简单理解为是Shader的包装。

    2.Material Instance 材质实例是指同一材质下，不同的参数配置。

## 我们需要做什么?

1.在Shader中，建立两个UBO或SSBO

```glsl
//一个用于存所有Local to World矩阵的数组

#define MI_MAX_COUNT (UBORange/sizeof(mat4))

layout(set=?,binding=?) uniform LocalToWorldData
{
    mat4 mats[L2W_MAX_COUNT];
}l2w;
```

```glsl
//一个用于存所有MaterialInstance数据的数组

#define MI_MAX_COUNT (UBORange/sizeof(MaterialInstance))

layout(set=?,binding=?) uniform MaterialInstanceData
{
    MaterialInstance mi_array[MI_MAX_COUNT];
}mi;
```

2.建立一个RG8UI或RG16UI的Vertex Input Stream,用于保存访问LocalToWorld/MaterialInstance的索引

```glsl
layout(location=?) in uvec2 Assign;     //Local To World矩阵ID与材质ID输入数据流

mat4 GetLocalToWorld()
{
    return l2w.mats[Assign.x];
}

MaterialInstance GetMaterialInstance()
{
    return mi.mi_array[Assign.y];
}
```

