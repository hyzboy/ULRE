#include<hgl/graph/DrawNode.h>
#include<hgl/graph/VertexDataManager.h>
#include<hgl/component/MeshComponent.h>

VK_NAMESPACE_BEGIN
/**
* 
* 理论上讲，我们需要按以下顺序排序
*
*   for(material)
*       for(pipeline)
*           for(material_instance)
*               for(texture)
*                   for(screen_tile)  
*                       for(vab)
* 
* 
*  关于Indirect Command Buffer

建立一个大的IndirectCommandBuffer，用于存放所有的渲染指令，包括那些不能使用Indirect渲染的。
这样就可以保证所有的渲染操作就算要切VBO，也不需要切换INDIRECT缓冲区，定位指令也很方便。
*/
const int DrawNode::compare(const DrawNode &other)const
{
    hgl::int64 off;

    //RectScope2i screen_tile;

    hgl::graph::SubMesh *ri_one=other.mesh_component->GetMesh();
    hgl::graph::SubMesh *ri_two=mesh_component->GetMesh();

    auto *prim_one=ri_one->GetPrimitive();
    auto *prim_two=ri_two->GetPrimitive();

    //比较VDM
    if(prim_one->GetVDM())      //有VDM
    {
        off=prim_one->GetVDM()
           -prim_two->GetVDM();

        if(off)
            return off;

        //比较纹理
        {
        }

        //比较屏幕空间所占tile
        {
        }

        //比较模型
        {
            off=prim_one
               -prim_two;

            if(off)
            {
                off=prim_one->GetVertexOffset()-prim_two->GetVertexOffset();        //保证vertex offset小的在前面

                return off;
            }
        }
    }

    //比较纹理
    {
    }

    //比较屏幕空间所占tile
    {
    }

    //比较距离。。。。。。。。。。。。。。。。。。。。。还不知道这个是正了还是反了，等测出来确认后修改下面的返回值和这里的注释

    float foff=other.to_camera_distance
                    -to_camera_distance;

    if(foff>0)
        return 1;
    else
        return -1;
}

SubMesh *DrawNode::GetMesh()const
{
    return mesh_component?mesh_component->GetMesh():nullptr;
}

MaterialInstance *DrawNode::GetMaterialInstance()const
{
    if(!mesh_component)return(nullptr);
    if(!mesh_component->GetMesh())return(nullptr);

    return mesh_component->GetMaterialInstance();
}
VK_NAMESPACE_END

