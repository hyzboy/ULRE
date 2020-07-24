#include<hgl/graph/TileData.h>
#include<hgl/log/LogInfo.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKBuffer.h>

namespace hgl
{
    namespace graph
    {
        TileData::TileData(Device *dev,Texture2D *tt,const uint tw,const uint th)
        {
            device=dev;

            tile_texture=tt;

            tile_width=tw;
            tile_height=th;

            tile_rows=tile_texture->GetHeight()/tile_height;
            tile_cols=tile_texture->GetWidth()/tile_width;

            tile_max_count=tile_rows*tile_cols;
            tile_count=0;

            to_pool.PreMalloc(tile_max_count);
            {
                int col=0,row=0;
                TileData::Object **to=to_pool.GetInactiveData();

                for(uint i=0;i<tile_max_count;i++)
                {
                    (*to)->col  =col;
                    (*to)->row  =row;
                    (*to)->left =col*tile_width;
                    (*to)->top  =row*tile_height;

                    ++to;

                    ++col;
                    if(col==tile_cols)
                    {
                        ++row;
                        col=0;
                    }
                }
            }

            tile_bytes=tile_width*tile_height*GetStrideByFormat(tile_texture->GetFormat());

            tile_buffer=device->CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,tile_bytes,nullptr);
        }

        TileData::~TileData()
        {
            SAFE_CLEAR(tile_buffer);
            SAFE_CLEAR(tile_texture);
        }

        TileData::Object *TileData::FindSpace()
        {
            TileData::Object *obj;

            if(!to_pool.Get(obj))
                return(nullptr);

            return obj;
        }
        
        bool TileData::WriteTile(TileData::Object *obj,const void *data,const uint bytes,int ctw,int cth)
        {
            if(!obj||!data||!bytes||ctw<=0||cth<=0)
                return(false);

            obj->width    =(ctw==-1)?tile_width:ctw;
            obj->height   =(cth==-1)?tile_height:cth;

            obj->fl=obj->left/double(tile_texture->GetWidth());
            obj->ft=obj->top /double(tile_texture->GetHeight());
            obj->fw=double(obj->width)/double(tile_texture->GetWidth());
            obj->fh=double(obj->height)/double(tile_texture->GetHeight());

            tile_buffer->Write(data,0,bytes);

            device->ChangeTexture2D(tile_texture,
                                    tile_buffer,
                                    obj->left,
                                    obj->top,
                                    tile_width,
                                    tile_height);

            //请保留这段代码，以便未来使用时该数据时不会使用
            //{
            //    vertex->Begin(index*6);
            //    texcoord->Begin(index*6);

            //        texcoord->WriteRect(obj->fl,obj->ft,obj->fw,obj->fh);
            //        vertex->WriteRect(0,0,obj->width,obj->height);

            //    texcoord->End();
            //    vertex->End();
            //}

            return(true);
        }

        /**
        * 增加一个Tile
        * @param data 图形原始数据
        * @param bytes 图形原始数据字节数
        * @param ctw 当前tile宽度,-1表示等同全局设置
        * @param cth 当前tile高度,-1表示等同全局设置
        * @return 为增加的Tile创建的对象
        */
        TileData::Object *TileData::Add(const void *data,const uint bytes,const int ctw,const int cth)
        {
            if(!data||!bytes||ctw<=0||cth<=0)
                return(nullptr);

            TileData::Object *obj=FindSpace();

            if(!obj)
                return(nullptr);

            WriteTile(obj,data,bytes,ctw,cth);

            tile_count++;
            return(obj);
        }

        /**
        * 删除一个Tile
        * @param obj 要删除的Tile的对象指针
        * @return 删除是否成功
        */
        bool TileData::Delete(TileData::Object *obj)
        {
            if(!obj)return(false);

            return to_pool.Release(obj);
        }

        /**
        * 更改一个Tile的数据内容
        * @param obj 要更改的Tile的对象指针
        * @param data 图形原始数据
        * @param bytes 图形原始数据字节数
        * @param ctw 当前tile宽度,-1表示等同全局设置
        * @param cth 当前tile高度,-1表示等同全局设置
        * @return 更改是否成功
        */
        bool TileData::Change(TileData::Object *obj,const void *data,const uint bytes,const int ctw,const int cth)
        {
            if(!obj||!data||!bytes||ctw<=0||cth<=0)
                return(false);

            if(!to_pool.IsActive(obj))
                return(false);

            return WriteTile(obj,data,bytes,ctw,cth);
        }

        /**
        * 清除所有Tile数据
        */
        void TileData::Clear()
        {
            to_pool.ClearAll();
        }
    }//namespace graph
}//namespace hgl
