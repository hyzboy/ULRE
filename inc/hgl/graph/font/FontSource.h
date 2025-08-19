#pragma once

#include<hgl/type/StrChar.h>
#include<hgl/type/Map.h>
#include<hgl/type/SortedSet.h>
#include<hgl/graph/font/Font.h>
#include<hgl/type/UnicodeBlocks.h>

using namespace hgl;

namespace hgl::graph
{
    struct CharMetricsInfo
    {
        int x,y;		//图像显示偏移
        int w,h;		//图像尺寸

        int adv_x,adv_y;//字符尺寸
    };//struct CharMetricsInfo

    /**
    * 字体位图数据
    */
    struct FontBitmap
    {
        int count;		//使用次数

        CharMetricsInfo metrics_info;

        uint8 *data;
    };//struct FontBitmap

    struct CharAttributes
    {
        u32char ch;             ///<字符

        bool space;			    ///<是否属于空格
                
        bool is_cjk;            ///<是否是中日韩文字
        bool is_emoji;          ///<是否是表情符号
        bool is_currency;       ///<是否货币符号

        bool is_punctuation;    ///<是否是标点符号

        bool begin_disable;     ///<是否行首禁用符号
        bool end_disable;       ///<是否行尾禁用符号
        bool vrotate;           ///<竖排时是否需要旋转
    };//
        
    /**
    * 字符排版属性
    */
    struct CharLayoutAttributes
    {
        CharAttributes *attr;       ///<字符基本信息

        bool visible;               ///<在当前字体下是否需要绘制

        CharMetricsInfo metrics;    ///<字符绘制信息
    };//struct CharLayoutAttributes

    using CLA=CharLayoutAttributes;

    /**
    * 字体数据源
    */
    class FontDataSource
    {
    protected:

        SortedSet<void *> ref_object;

        ObjectMap<u32char,CLA> cla_cache;

    public:

        virtual ~FontDataSource()=default;

    public:

        virtual			FontBitmap *GetCharBitmap	(const u32char &)=0;						///<取得字符位图数据
        virtual const	bool		GetCharMetrics	(CharMetricsInfo &,const u32char &)=0;		///<取得字符绘制信息
                const	CLA *		GetCLA			(const u32char &);							///<取得字符排版信息
        virtual			int			GetCharHeight	()const=0;									///<取得字符高度

        void RefAcquire(void *);																///<引用请求
        void RefRelease(void *);																///<引用释放
        int  RefCount()const{return ref_object.GetCount();}										///<获取引用对象数量
    };//class FontDataSource

    /**
    * 位图字体数据源
    */
    class FontBitmapDataSource:public FontDataSource
    {
    protected:

        Font fnt;

        ObjectMap<u32char,FontBitmap> chars_bitmap;												///<字符位图

    protected:

        virtual bool MakeCharBitmap(FontBitmap *,u32char)=0;									///<产生字符位图数据

    public:

        FontBitmapDataSource(const Font &f){fnt=f;}

    protected:

        virtual ~FontBitmapDataSource()=default;

    public:

                        FontBitmap *GetCharBitmap	(const u32char &ch) override;				///<取得字符位图数据
                const	bool		GetCharMetrics	(CharMetricsInfo &,const u32char &)override;///<取得字符绘制信息
        virtual			int			GetCharHeight	()const override{return fnt.height;}		///<取得字符高度
    };//class FontBitmapDataSource:public FontDataSource

    FontDataSource *AcquireFontSource(const Font &f);

    /**
    * 文字位图多重数据源
    */
    class FontSourceMulti:public FontDataSource
    {
        using FontSourcePointer=FontDataSource *;

        FontDataSource *default_source;
        Map<UnicodeBlock,FontSourcePointer> source_map;

        int max_char_height;

        void RefreshMaxCharHeight();

    protected:

        FontDataSource *GetFontSource(const u32char &ch);

    public:

        /**
        * @param dfs 缺省字符数据源
        */
        FontSourceMulti(FontDataSource *dfs);
        virtual ~FontSourceMulti();

        void Add(UnicodeBlock,FontDataSource *);

        void AddCJK(FontDataSource *chs_fs)
        {
            Add(UnicodeBlock::cjk_radicals_supplement,chs_fs);
            Add(UnicodeBlock::cjk_symbols_and_punctuation,chs_fs);
            Add(UnicodeBlock::cjk_strokes,chs_fs);
            Add(UnicodeBlock::enclosed_cjk_letters_and_months,chs_fs);
            Add(UnicodeBlock::cjk_compatibility,chs_fs);
            Add(UnicodeBlock::cjk_unified_ideographs_extension_a,chs_fs);
            Add(UnicodeBlock::cjk_unified_ideographs,chs_fs);
            Add(UnicodeBlock::cjk_compatibility_ideographs,chs_fs);
            Add(UnicodeBlock::cjk_compatibility_forms,chs_fs);
            Add(UnicodeBlock::cjk_unified_ideographs_extension_b,chs_fs);
            Add(UnicodeBlock::cjk_unified_ideographs_extension_c,chs_fs);
            Add(UnicodeBlock::cjk_unified_ideographs_extension_d,chs_fs);
            Add(UnicodeBlock::cjk_unified_ideographs_extension_e,chs_fs);
            Add(UnicodeBlock::cjk_unified_ideographs_extension_f,chs_fs);
            Add(UnicodeBlock::cjk_compatibility_ideographs_supplement,chs_fs);
            Add(UnicodeBlock::cjk_unified_ideographs_extension_g,chs_fs);
        }

        void Remove(UnicodeBlock);
        void Remove(FontDataSource *);

    public:

                FontBitmap *GetCharBitmap	(const u32char &ch) override;
        const	bool		GetCharMetrics	(CharMetricsInfo &,const u32char &)override;		///<取得字符绘制信息
                int			GetCharHeight	()const override{return max_char_height;}			///<取得字符高度
    };//class FontSourceMulti:public FontDataSource

    /**
    * 创建一个CJK字体源
    * @param latin_font Latin字体名称
    * @param cjk_font CJK字体名称
    * @param size 字体象素高度
    */
    FontDataSource *CreateCJKFontSource(const os_char *latin_font,const os_char *cjk_font,const uint32_t size);

    /**
    * 创建一个字体源
    * @param name 字体名称
    * @param size 字体象素高度
    */
    FontDataSource *AcquireFontSource(const os_char *name,const uint32_t size);

    void ReleaseFontSource(FontDataSource *fs);
}//namespace hgl::graph
