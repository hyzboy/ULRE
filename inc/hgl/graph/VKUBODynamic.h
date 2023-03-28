#ifndef HGL_GRAPH_UBO_DYNAMIC_INCLUDE
#define HGL_GRAPH_UBO_DYNAMIC_INCLUDE

#include<hgl/graph/VKArrayBuffer.h>
VK_NAMESPACE_BEGIN

template<typename T> class UBODynamicAccess
{
    uchar *pointer;
    uchar *current;

    uint unit_size;

    uint count;
    uint index;

private:

    UBODynamicAccess()
    {
        Restart();
    }

    void Restart()
    {
        pointer=nullptr;
        current=nullptr;
        unit_size=0;
        count=0;
        index=0;         
    }    

    void Start(uchar *buf,const uint us,const uint c)
    {
        current=pointer=buf;
        unit_size=us;
        count=c;
        index=0;
    }

    friend class GPUArrayBuffer;

public:

    const uint GetCount()const{return count;}
    const uint GetCurrentIndex()const{return index;}
    const uint GetOffsetBytes()const{return index*unit_size;}

    bool Write(uchar *src)
    {
        if(!src)return(false);
        if(index>=count)return(false);

        memcpy(current,src,sizeof(T));
        current+=unit_size;
        ++index;

        return(true);
    }

    bool Write(uchar *src,const uint c)
    {
        if(!src)return(false);
        if(c<=0)return(false);
        if(index+c>count)return(false);

        for(uint i=0;i<c;i++)
        {
            memcpy(current,src,sizeof(T));
            current+=unit_size;
            src+=sizeof(T);
        }

        index+=c;

        return(true);
    }
};//template<typename T> class UBODynamicAccess
VK_NAMESPACE_END
#endif//HGL_GRAPH_UBO_DYNAMIC_INCLUDE
