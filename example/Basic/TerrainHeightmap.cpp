#include<hgl/math/Vector.h>

using namespace hgl;

struct TerrainBlock
{
    alignas(8)  Vector2f    size;                   ///<地形块宽度和高度(单位:格数)
    alignas(16) Vector3f    scale;                  ///<地形块缩放比例
};

constexpr const size_t TerrainBlockBytes=sizeof(TerrainBlock);
