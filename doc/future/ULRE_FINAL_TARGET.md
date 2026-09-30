ULRE ECS 设计终极形态畅想

注：以下均为设计畅想，如有问题请及时提出并修改本文档。

# 1. Component Data

所有的Component都会有自己的数据，这个数据必须定长，并且这个数据需要符合GPU访问最小对齐单元。

可以对应一个C++结构和一个GLSL结构，在后文中我们称之为struct ComponentData;

我们整体机制为预算制，除Editor模式外，正式版不允许有扩容机制。Editor模式下不考虑性能问题，所以可以扩容。

运行时，会根据预算自动分配足够大的SSBO，或内存空间.UMA机型上肯定是只有SSBO，独显下有可能需要StagingBuffer，也有可能是ringbuffer之类。

每当我们要增加一个Component时，是直接从这个大SSBO的数据队列中申请一个ID用于读写。
该机制可参考现有的GlobalSSBOBufferRegistry类,它在‘E:\ULRE\inc\hgl\graph\module\GlobalSSBOBufferRegistry.h’中，其就是我们所说的用途。未来甚至可以考虑将该机制抽像，让多处共用。

也就是说，我们定义一个新的Component，一是需要在ComponentType中增加一个枚举，二是需要指定好数据长度即可。用不用那是另一回事不重要。

# 2. Component 访问

假设一个Transform Component的数据类是这样的:

```C++
alignas(64) struct TransformComponentData
{
    Matrix4f local_to_world;
};
```

而我们在整个引擎的最基础部分会有一个定义

```C++
struct ComponentDataAccessor //数据访问器
{
protected:

    ActiveRowPool * data_pool;

    int32 data_id;
    int32 data_count;
};

// 类型化数据访问器
template<typename T> struct TypedComponentDataAccessor:public ComponentDataAccessor
{
public:

    const bool IsValid()const
    {
        return data_pool;
    }

    T *Get()
    {
        return data_pool->RowCPU(data_id);
    }

    //各种写入函数
    bool Write(const T *source_data){...}
    bool Write(const T *source_data,const uint32 count){...}

    //各种读取函数
    bool Read(...
};
```

那么最终你在定义 TransformComponentAccessor 时就这样写

```C++
struct TransformComponentAccessor:public TypedComponentDataAccessor<TransformComponentData>
{
public:

    //各种该结构的自有访问方式
};
```


# 3. Entity

```C++
enum class ComponentType
{
    None=0,

    Geometry,           ///<提供几何体
    Transform,
    Material,

    HLODProxy,          ///<HLOD代理信息

    //Physical,
    //Audio,

    End,
};

constexpr const URLE_MAX_COMPONENT_COUNT=8;     //每个Entity最多8个Component

struct Entity
{
    uint64 persistent_id;           ///< 持久性id，用于存档之类

    uint32 flag_bitmasks;           ///< 功能掩码(有位置,有体积,是否可移动,有transform,有几何体,有渲染,有碰撞,有物理，有音频...)

    uint8 component_type[URLE_MAX_COMPONENT_COUNT];     //所有component的类型
    uint32 component_ids[URLE_MAX_COMPONENT_COUNT];     //所有component的ID号

    uint32 work_flags;              ///< 工作用FLAGS，比如快速确定是要渲染是mesh还是billboard，或者就不是要渲染的。
};
```

同样的，Entity最终也是平凡类型的纯数据，最终形态，也会放入SSBO供Compute Shader访问。终极形态下Entity列表可整个存入文件，用的时候一次性载入内存无需一个个new，以提升解析速度。


如一个要渲染普通Mesh的Entity，那么它下面的component自然是有：GeometryComponent提供几何体，MaterialComponent提供材质信息，TransformComponent提供Transform信息，等等。

而如果是一个相机Entity，那么它下面的Component自然是有CameraComponent,TransformComponent。

但这些Component都不是C++对象，而只是存在于component_type,component_ids中的UINT型数据。C++访问时，使用访问器来进行访问。


# 4.全场景Transform双层展开设计

如果使用现有的展开方式，将必须严格按顺序执行，这样无法充分发挥GPU效能。
所以，我们使用双层展开设计：

假设我们是由积木mesh组成一个房子，再由一堆房子组成一个小区。

那么我们在第一个渲染对象收集分配时要这样做：

- 根据当前相机位置，确定要渲染的对象是什么。

- 如果这个积木在超近距离，那么Transform展开是 World->积木->积木Meshlet

- 如果离远一点： World->房子->积木(已被LOD退化成一个meshlet顶点数)

- 再远一点：World->房子HLOD->房子HLOD Meshlet

- 再远一点：World->小区->房子(已退出成一个Meshlet的HLOD模型)

也就是说：最终进行渲染分发时，永远保持只有2级的Transform L2W展开。
