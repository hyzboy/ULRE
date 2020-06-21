#ifndef HGL_GRAPH_TILE_DATA_INCLUDE
#define HGL_GRAPH_TILE_DATA_INCLUDE

#include<hgl/graph/vulkan/VKTexture.h>

VK_NAMESPACE_USING

namespace hgl
{
    namespace graph
    {
        /**
         * TileData是一种处理大量等同尺寸及格式贴图的管理机制，程序会自动根据显卡最大贴图处理能力来创建尽可能符合需求的贴图。(注：Tile的大小不必符合2次幂)
         * Tile的增加或删除，程序会自动排序，尽可能小的减少I/O消耗。
         */
        class TileData                                                                               ///Tile纹理管理
        {
            Device *device;

        public:

            struct Object
            {
                int index;

                double fl,ft,fw,fh;

                int width,height;
            };//struct TileObject

        protected:

            vulkan::Buffer *tile_buffer;                                                            ///<Tile暂存缓冲区

            Texture2D *tile_texture;                                                                ///<TileData所用的纹理对象

            TileData::Object **tile_object;                                                         ///<所有的Tile对象

            uint tile_width,tile_height;                                                            ///<Tile的宽和高
            uint32_t tile_bytes;                                                                    ///<一个tile字节数
			uint tile_count,tile_max_count;														    ///<当前Tile数量与最大数量
			uint tile_rows,tile_cols;																///<贴图中可用的Tile行数和列数

        protected:
        
			int FindSpace();																		///<寻找一个空位
			void WriteTile( const int,TileData::Object *,
                            const void *,const uint,const VkFormat,const int,const int);	        ///<写入一个Tile数据
            
		public:

			int			GetWidth	()const{return tile_width;}										///<取得Tile宽
			int			GetHeight	()const{return tile_height;}									///<取得Tile高
			int			GetCount	()const{return tile_count;}										///<取得Tile数量
			int			GetMaxCount	()const{return tile_max_count;}									///<取得Tile最大数量
			int			GetFreeCount()const{return tile_max_count-tile_count;}						///<取得空余Tile数量

			Texture2D *	GetTexture	()const{return tile_texture;}									///<取得贴图

        public:

			TileData(Device *,Texture2D *,const uint tw,const uint th);
			virtual ~TileData();

			TileData::Object *Add(const void *,const uint,const VkFormat,const int=-1,const int=-1);                    ///<增加一个Tile
//			TileData::Object *Add(Bitmap2D *,int=-1,int=-1);										    ///<增加一个Tile

			bool Delete(TileData::Object *);														                    ///<删除一个Tile
			bool Change(TileData::Object *,const void *,const uint,const VkFormat,const int=-1,const int=-1);	        ///<更改一个Tile的数据内容
			void Clear();                                                                                               ///<清除Tile数据
        };//class TileData
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TILE_DATA_INCLUDE
