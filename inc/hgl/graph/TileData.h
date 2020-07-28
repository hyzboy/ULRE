#ifndef HGL_GRAPH_TILE_DATA_INCLUDE
#define HGL_GRAPH_TILE_DATA_INCLUDE

#include<hgl/graph/vulkan/VKTexture.h>
#include<hgl/type/Pool.h>
#include<hgl/type/RectScope.h>
#include<hgl/graph/Bitmap.h>

VK_NAMESPACE_USING

namespace hgl
{
    namespace graph
    {
        struct TileObject
        {
            int col,row;            //当前tile在整个纹理中的tile位置

            RectScope2d uv_pixel;   //以象素为单位的tile位置和尺寸
            RectScope2d uv_float;   //以浮点为单位的tile位置和尺寸
        };//struct TileObject

        /**
         * TileData是一种处理大量等同尺寸及格式贴图的管理机制，程序会自动根据显卡最大贴图处理能力来创建尽可能符合需求的贴图。(注：Tile的大小不必符合2次幂)
         * Tile的增加或删除，程序会自动排序，尽可能小的减少I/O消耗。
         */
        class TileData                                                                                                 ///Tile纹理管理
        {
            Device *device;

        protected:

            vulkan::Buffer *tile_buffer;                                                                                ///<Tile暂存缓冲区

            Texture2D *tile_texture;                                                                                    ///<TileData所用的纹理对象

            ObjectPool<TileObject> to_pool;                                                                             ///<Tile对象池

            uint tile_width,tile_height;                                                                                ///<Tile的宽和高
            uint32_t tile_bytes;                                                                                        ///<一个tile字节数
			uint tile_count,tile_max_count;														                        ///<当前Tile数量与最大数量
			uint tile_rows,tile_cols;																                    ///<贴图中可用的Tile行数和列数

        protected:
        
			bool WriteTile(TileObject *,const void *,const uint,const int,const int);	                                ///<写入一个Tile数据
            
		public:

			int			GetWidth	()const{return tile_width;}										                    ///<取得Tile宽
			int			GetHeight	()const{return tile_height;}									                    ///<取得Tile高
			int			GetCount	()const{return tile_count;}										                    ///<取得Tile数量
			int			GetMaxCount	()const{return tile_max_count;}									                    ///<取得Tile最大数量
			int			GetFreeCount()const{return tile_max_count-tile_count;}						                    ///<取得空余Tile数量

			Texture2D *	GetTexture	()const{return tile_texture;}									                    ///<取得贴图

        public:

			TileData(Device *,Texture2D *,const uint tw,const uint th);
			virtual ~TileData();

			TileObject *Add(const void *,const uint,const int=-1,const int=-1);                                         ///<增加一个Tile
			TileObject *Add(BitmapData *bmp){return this->Add(bmp->data,bmp->total_bytes,bmp->width,bmp->height);}		///<增加一个Tile

			bool Delete(TileObject *);														                            ///<删除一个Tile
			bool Change(TileObject *,const void *,const uint,const int=-1,const int=-1);	                            ///<更改一个Tile的数据内容
			void Clear();                                                                                               ///<清除Tile数据
        };//class TileData
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TILE_DATA_INCLUDE
