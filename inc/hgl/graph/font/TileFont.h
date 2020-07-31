#ifndef HGL_GRAPH_TILE_FONT_INCLUDE
#define HGL_GRAPH_TILE_FONT_INCLUDE

#include<hgl/graph/TileData.h>
#include<hgl/graph/font/FontSource.h>

namespace hgl
{
    namespace graph
    {
        template<typename T> struct RefData
        {
            int ref_count;
            T data;

        public:

            RefData(const T &value)
            {
                ref_count=1;
                data=value;
            }
        };//template<typename T> struct RefData

        template<typename T> class DataPool
        {
        public:

            DataPool();
            virtual ~DataPool()=default;

            virtual bool Acquire(T &)=0;
            virtual void Release(const T)=0;
        };//

        template<typename K,typename V> class ResPool
        {
        protected:

            using ActiveItem=RefData<V>;

            DataPool<V> *pool;

            MapObject<K,ActiveItem> active_items;               ///<活跃的数据
            Map<K,V> idle_items;                                ///<引用计数为0的

        public:

            ResPool(DataPool<V> *dp):pool(dp){}
            virtual ~ResPool()=default;

            bool Get(const K &key,V &value)
            {
                ActiveItem *ai;
                
                if(active_items.Get(key,ai))                    //在活跃列表中找
                {
                    ++ai->ref_count;
                    value=ai->data;
                    return(true);
                }

                if(idle_items.Get(key,value))                   //在限制列表中找
                {
                    active_items.Add(key,new ActiveItem(value));
                    return(true);
                }

                return(false);
            }

            bool Create(const K &key,V &value)
            {
                if(!pool->Acquire(value))
                    return(false);

                active_items.Add(key,new ActiveItem(value));
                return(true);
            }

            void Release(const K &key)
            {
                int pos;
                ActiveItem *ai;

                pos=active_items.GetValueAndSerial(key,ai);

                if(pos>0)
                {
                    --ai->ref_count;

                    if(ai->ref_count==0)
                    {
                        idle_items.Add(ai->data);
                        active_items.DeleteBySerial(pos);
                    }

                    return;
                }
            }
        };//template<typename K,typename V> class ResPool

        using TileResPool=ResPool<u32char,TileObject *>;

        /**
         * Tile字符管理<br>
         * 本模块允许有多个字符数据来源，每个来源也可以对应多个unicode块, 但一个unicode块只能对应一个字体数据来源
         */
        class TileFont
        {
            FontSource *source;
            TileData *tile_data;

            DataPool<TileObject *> *to_pool;
            TileResPool *to_res;

        public:

            FontSource *GetFontSource   (){return source;}
            TileData *  GetTileData     (){return tile_data;}

        public:

            TileFont(TileData *td,FontSource *fs);
            virtual ~TileFont();

            bool Registry(List<TileUVFloat> &,const List<u32char> &);                               ///<注册要使用的字符
            void Unregistry(const List<u32char> &);                                                 ///<注销要使用的字符
        };//class TileFont
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TILE_FONT_INCLUDE
