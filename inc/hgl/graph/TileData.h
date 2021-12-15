#ifndef HGL_GRAPH_TILE_DATA_INCLUDE
#define HGL_GRAPH_TILE_DATA_INCLUDE

#include<hgl/type/Map.h>
#include<hgl/type/Pool.h>
#include<hgl/type/RectScope.h>
#include<hgl/graph/Bitmap.h>
#include<hgl/graph/VKTexture.h>

VK_NAMESPACE_USING

namespace hgl
{
    namespace graph
    {
        using TileUVPixel=RectScope2i;
        using TileUVFloat=RectScope2d;

        using TileUVFloatList=List<TileUVFloat>;
        using TileUVFloatMap=Map<u32char,TileUVFloat>;

        struct TileObject
        {
            int col,row;            //当前tile在整个纹理中的tile位置

            TileUVPixel uv_pixel;   //以象素为单位的tile位置和尺寸
            TileUVFloat uv_float;   //以浮点为单位的tile位置和尺寸
        };//struct TileObject

        /**
         * TileData是一种处理大量等同尺寸及格式贴图的管理机制，程序会自动根据显卡最大贴图处理能力来创建尽可能符合需求的贴图。(注：Tile的大小不必符合2次幂)
         * Tile的增加或删除，程序会自动排序，尽可能小的减少I/O消耗。
         */
        class TileData                                                                                                 ///Tile纹理管理
        {
            GPUDevice *device;

        protected:

            Texture2D *tile_texture;                                                                                    ///<TileData所用的纹理对象

            ObjectPool<TileObject> to_pool;                                                                             ///<Tile对象池

            uint tile_width,tile_height;                                                                                ///<Tile的宽和高
            uint tile_bytes;                                                                                            ///<一个tile字节数
            uint tile_count,tile_max_count;														                        ///<当前Tile数量与最大数量
            uint tile_rows,tile_cols;																                    ///<贴图中可用的Tile行数和列数

        protected:

            GPUBuffer *tile_buffer;                                                                                     ///<Tile暂存缓冲区

            List<Image2DRegion> commit_list;
            uint8 *commit_ptr;

            bool CommitTile(TileObject *,const void *,const uint,const int,const int);	                                ///<提交一个Tile数据

        public:

            int			GetWidth	()const{return tile_width;}										                    ///<取得Tile宽
            int			GetHeight	()const{return tile_height;}									                    ///<取得Tile高
            int			GetCount	()const{return tile_count;}										                    ///<取得Tile数量
            int			GetMaxCount	()const{return tile_max_count;}									                    ///<取得Tile最大数量
            int			GetFreeCount()const{return tile_max_count-tile_count;}						                    ///<取得空余Tile数量

            Texture2D *	GetTexture	()const{return tile_texture;}									                    ///<取得贴图

        public:

            TileData(GPUDevice *,Texture2D *,const uint tw,const uint th);
            virtual ~TileData();

            void BeginCommit();

            TileObject *Commit(const void *,const uint,const int=-1,const int=-1);                                      ///<提交一个Tile
            TileObject *Commit(BitmapData *bmp){return this->Commit(bmp->data,bmp->total_bytes,bmp->width,bmp->height);}///<提交一个Tile

            TileObject *Acquire();                                                                                      ///<请求一个Tile
            bool        Change(TileObject *,const void *,const uint,const int=-1,const int=-1);	                        ///<更改一个Tile的数据内容

            int  EndCommit();

            bool Delete(TileObject *);														                            ///<删除一个Tile
            void Clear();                                                                                               ///<清除Tile数据
        };//class TileData
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TILE_DATA_INCLUDE
