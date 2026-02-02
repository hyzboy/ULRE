#pragma once

#include<hgl/CoreType.h>
namespace hgl
{
    namespace graph
    {
        struct Image2DRegion
        {
            int left,top,width,height;
            uint bytes;

            bool operator==(const Image2DRegion& other) const
            {
                return left == other.left &&
                       top == other.top &&
                       width == other.width &&
                       height == other.height &&
                       bytes == other.bytes;
            }
        };//struct Image2DRegion
    }//namespace graph
}//namespace hgl
