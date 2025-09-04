#include<hgl/graph/TileData.h>
#include<hgl/log/LogInfo.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/module/TextureManager.h>

namespace hgl::graph
{
    TileData::TileData(TextureManager *tm,Texture2D *tt,const uint tw,const uint th)
    {
        tex_manager=tm;

        tile_texture=tt;

        tile_width=tw;
        tile_height=th;

        tile_rows=tile_texture->GetHeight()/tile_height;
        tile_cols=tile_texture->GetWidth()/tile_width;

        tile_max_count=tile_rows*tile_cols;
        tile_count=0;

        to_pool.CreateIdle(tile_max_count);
        {
            int col=0,row=0;
            TileObject to;
            auto &to_idle_ids=to_pool.GetIdleArray();

            for(int idle_id:to_idle_ids)
            {
                to.id=idle_id;

                to.col  =col;
                to.row  =row;

                to.uv_pixel.Set(col*tile_width,
                                row*tile_height,
                                0,
                                0);

                to_pool.WriteData(to,idle_id);

                ++col;

                if(col==tile_cols)
                {
                    ++row;
                    col=0;
                }
            }
        }

        tile_bytes=GetImageBytes(tile_texture->GetFormat(),tile_width*tile_height);

        tile_buffer=tex_manager->CreateTransferSourceBuffer(tile_bytes*tile_max_count);

        commit_ptr=nullptr;
    }

    TileData::~TileData()
    {
        SAFE_CLEAR(tile_buffer);
        //SAFE_CLEAR(tile_texture);     //TextureManager会自动管理Texture对象的生命周期，所以不需要在这里删除
    }

    void TileData::BeginCommit()
    {
        commit_list.Clear();
        commit_ptr=(uint8 *)tile_buffer->Map();

        memset(commit_ptr,255,tile_bytes*tile_max_count);  //清除所有数据
    }

    int TileData::EndCommit()
    {
        const int commit_count=commit_list.GetCount();

        if(commit_count<=0)
            return -1;

        tile_buffer->Unmap();
        commit_ptr=nullptr;

        if(!tex_manager->ChangeTexture2D(tile_texture,tile_buffer,commit_list))
            return -2;

        const int result=commit_list.GetCount();

        commit_list.Clear();
        return result; 
    }
        
    bool TileData::CommitTile(TileObject *obj,const void *data,const uint bytes,int ctw,int cth)
    {
        if(!commit_ptr)return(false);
        if(!obj||!data||!bytes||ctw<=0||cth<=0)
            return(false);

        obj->uv_pixel.SetSize(  (ctw==-1)?tile_width:ctw,
                                (cth==-1)?tile_height:cth);

        UVFloatFromPixel(   obj->uv_float,
                            obj->uv_pixel,
                            tile_texture->GetWidth(),
                            tile_texture->GetHeight());

        const int commit_count=commit_list.GetCount();

        if(commit_count>=tile_max_count)      //理论上不可能
            return(false);

        memcpy(commit_ptr,data,bytes);
        commit_ptr+=bytes;

        Image2DRegion ir;

        ir.left     =obj->uv_pixel.GetLeft();
        ir.top      =obj->uv_pixel.GetTop();
        ir.width    =obj->uv_pixel.GetWidth();
        ir.height   =obj->uv_pixel.GetHeight();
        ir.bytes    =bytes;

        commit_list.Add(ir);

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
    TileObject *TileData::Commit(const void *data,const uint bytes,const int ctw,const int cth)
    {
        if(!commit_ptr)return(nullptr);
        if(!data||!bytes||ctw<=0||cth<=0)
            return(nullptr);

        TileObject *to=to_pool.GetIdle();

        if(!to)
            return(nullptr);

        CommitTile(to,data,bytes,ctw,cth);

        tile_count++;
        return(to);
    }

    /**
        * 获取一个TileObject
        */
    TileObject *TileData::Acquire()
    {
        return to_pool.GetIdle();
    }

    /**
    * 删除一个Tile
    * @param obj 要删除的Tile的对象指针
    * @return 删除是否成功
    */
    bool TileData::Delete(TileObject *obj)
    {
        if(!obj)return(false);

        return to_pool.Release(&(obj->id))>=0;
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
    bool TileData::Change(TileObject *obj,const void *data,const uint bytes,const int ctw,const int cth)
    {
        if(!commit_ptr)return(false);
        if(!obj||!data||!bytes||ctw<=0||cth<=0)
            return(false);

        if(!to_pool.IsActive(obj->id))
            return(false);

        return CommitTile(obj,data,bytes,ctw,cth);
    }

    /**
    * 清除所有Tile数据
    */
    void TileData::Clear()
    {
        to_pool.ReleaseAllActive();
    }
}//namespace hgl::graph
