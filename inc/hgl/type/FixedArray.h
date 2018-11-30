#ifndef HGL_TYPE_FIXED_ARRAY_INCLUDE
#define HGL_TYPE_FIXED_ARRAY_INCLUDE

#include<hgl/TypeFunc.h>
namespace hgl
{
    /**
     * 固定阵列数据模板
     */
    template<typename T> class FixedArray
    {
    protected:

        uint max_count;
        T default_value;

        T *items;
        uint item_count;

    public:

        FixedArray()
        {
            max_count=0;
            items=nullptr;
            item_count=0;
        }

        virtual ~FixedArray()
        {
            ClearAll();
            SAFE_FREE(items);
        }

        virtual void SetDefaultValue(const T &value)
        {
            default_value=value;
        }

        virtual bool SetMaxCount(const uint size)
        {
            if(size<=0)return(false);

            if(!items)
            {
                max_count=size;
                items=(T *)hgl_malloc(max_count*sizeof(T));

                T *p=items;
                for(uint i=0;i<=max_count;i++)
                    *p++=default_value;
            }
            else
            {
                if(size==max_count)return(true);

                items=(T *)hgl_realloc(items,size*sizeof(T));

                if(size>max_count)
                {
                    T *p=items+max_count;
                    for(uint i=max_count;i<size;i++)
                        *p++=default_value;
                }

                max_count=size;
            }

            return(true);
        }

        const uint GetCount()const{return item_count;}
        const uint GetMaxCount()const{return max_count;}

        T operator[](uint n)
        {
            if(!items||n>=max_count)return default_value;

            return items[n];
        }

        const T operator[](uint n)const
        {
            if(!items||n>=max_count)return default_value;

            return items[n];
        }

        virtual bool Set(uint n,T &data)
        {
            if(!items||n>max_count)return(false);

            if(data==default_value)return(false);

            if(items[n]!=default_value)
            {
                if(items[n]==data)
                    return(true);

                return(false);
            }

            items[n]=data;
            ++item_count;
            return(true);
        }

        virtual bool Clear(uint n)
        {
            if(!items||n>max_count)return(false);

            if(items[n]==default_value)return(true);

            items[n]=default_value;
            --item_count;
            return(true);
        }

        virtual void ClearAll()
        {
            if(!items)return;

            for(uint i=0;i<max_count;i++)
                items[i]=default_value;

            item_count=0;
        }
    };//class FixedArray

    /**
     * 固定对象数据阵列模板
     */
    template<typename T> class ObjectFixedArray:public FixedArray<T *>
    {
    public:

        ObjectFixedArray():FixedArray<T *>()
        {
            this->default_value=nullptr;
        }

        ~ObjectFixedArray() override
        {
            ClearAll();
        }

        bool Clear(uint n) override
        {
            if(!this->items||n>this->max_count)return(false);

            if(!this->items[n])return(true);

            delete this->items[n];

            this->items[n]=nullptr;
            --this->item_count;
            return(true);
        }

        void ClearAll() override
        {
            if(!this->items)
                return;

            for(uint i=0;i<this->max_count;i++)
            {
                if(this->items[i])delete this->items[i];
                this->items[i]=nullptr;
            }

            this->item_count=0;
        }
    };//class ObjectFixedArray
}//namespace hgl
#endif//HGL_TYPE_FIXED_ARRAY_INCLUDE
