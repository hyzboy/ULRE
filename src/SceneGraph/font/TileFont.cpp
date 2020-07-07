#include<hgl/graph/font/TileFont.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKFormat.h>

namespace hgl
{
    namespace graph
    {
		FontSource *AcquireFontSource(const Font &f);
        
        TileFont::TileFont(TileData *td,FontSource *fs)
        {
            tile_data=td;

            if(fs)
            {
                source=fs;
                source->RefAcquire(this);                
            }
        }

        TileFont::~TileFont()
        {
            if(source)
                source->RefRelease(this);

            SAFE_CLEAR(tile_data);
        }
        
        /**
         * 创建只使用一种字符的Tile字符管理对象
         * @param f 字体需求信息
         * @param limit_count 缓冲字符数量上限
         */
        TileFont *CreateTileFont(VK_NAMESPACE::Device *device,const Font &f,int limit_count=-1)
        {
            int height=(f.height+3)>>2;
            
            height<<=2;     //保证可以被4整除
            height+=2;      //上下左右各空一个象素

            if(limit_count<=0)
            {
                const VkExtent2D &ext=device->GetSwapchainSize();

                limit_count=(ext.width/height)*(ext.height/height);     //按全屏幕放满不一样的字符为上限
            }

            FontSource *fs=AcquireFontSource(f);

            if(!fs)
                return(nullptr);

            TileData *td=device->CreateTileData(UFMT_R8,height,height,limit_count);

            if(!td)
                return nullptr;

            return(new TileFont(td,fs));
        }
    }//namespace graph
}//namespace hgl
