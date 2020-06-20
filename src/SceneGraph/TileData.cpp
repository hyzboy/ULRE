#include<hgl/graph/TileData.h>
#include<hgl/log/LogInfo.h>

namespace hgl
{
    namespace graph
    {
        TileData::TileData(Texture2D *tt,const uint tw,const uint th)
        {
            tile_texture=tt;

            tile_width=tw;
            tile_height=th;

            tile_rows=tile_texture->GetHeight()/tile_height;
            tile_cols=tile_texture->GetWidth()/tile_width;

            tile_max_count=tile_rows*tile_cols;
            tile_count=0;

            NEW_NULL_ARRAY(tile_object,TileData::Object *,tile_max_count);
        }

        TileData::~TileData()
        {
            SAFE_CLEAR(tile_texture);
            SAFE_CLEAR_OBJECT_ARRAY(tile_object,tile_max_count);
        }

        int TileData::FindSpace()
        {
            if(!tile_object)return(-1);
            if(tile_count>=tile_max_count)return(-1);

            int n=tile_max_count;

            while(n--)
                if(!(tile_object[n]))
                    return(n);

            LOG_PROBLEM(OS_TEXT("无法在Tile数据区内找到足够的空间！"));
            return(-1);
        }
        
        void TileData::WriteTile(int index,TileData::Object *obj,void *data,uint bytes,VkFormat format,int ctw,int cth)
        {
            int col,row;
            double left,top;

            col=index%tile_cols;
            row=index/tile_cols;

            left=tile_width *col;
            top =tile_height*row;

            obj->index  =index;

            obj->width    =(ctw==-1)?tile_width:ctw;
            obj->height   =(cth==-1)?tile_height:cth;

            obj->fl=left/double(tile_texture->GetWidth());
            obj->ft=top /double(tile_texture->GetHeight());
            obj->fw=double(obj->width)/double(tile_texture->GetWidth());
            obj->fh=double(obj->height)/double(tile_texture->GetHeight());

            tile_object[index]=obj;

            tile_texture->ChangeImage(    left,
                                        top,
                                        tile_width,
                                        tile_height,
                                        data,
                                        bytes,
                                        format);
            //请保留这段代码，以便未来使用时该数据时不会使用
            //{
            //    vertex->Begin(index*6);
            //    texcoord->Begin(index*6);

            //        texcoord->WriteRect(obj->fl,obj->ft,obj->fw,obj->fh);
            //        vertex->WriteRect(0,0,obj->width,obj->height);

            //    texcoord->End();
            //    vertex->End();
            //}
        }
    }//namespace graph
}//namespace hgl
