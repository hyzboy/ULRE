#ifndef HGL_GRAPH_MATERIAL_DATA_SOURCE_INCLUDE
#define HGL_GRAPH_MATERIAL_DATA_SOURCE_INCLUDE

#include<hgl/graph/material/Material.h>
#include<hgl/type/BaseString.h>

BEGIN_MATERIAL_NAMESPACE

/**
 * 数据源
 */
class DataSource
{
    DataFormat format;

public:

    DataSource()
    {
        format=DataFormat::NONE;
    }

    DataSource(const DataFormat &df)
    {
        format=df;
    }

    virtual ~DataSource()=default;

    virtual bool SetX(UTF8String &,const UTF8String &)const{return false;}
    virtual bool SetY(UTF8String &,const UTF8String &)const{return false;}
    virtual bool SetZ(UTF8String &,const UTF8String &)const{return false;}
    virtual bool SetW(UTF8String &,const UTF8String &)const{return false;}

    virtual bool SetXY(UTF8String &,const UTF8String &)const{return false;}
    virtual bool SetYZ(UTF8String &,const UTF8String &)const{return false;}
    virtual bool SetZW(UTF8String &,const UTF8String &)const{return false;}

    virtual bool SetXYZ(UTF8String &,const UTF8String &)const{return false;}
    virtual bool SetXYZW(UTF8String &,const UTF8String &)const{return false;}

    bool SetR(UTF8String &result,const UTF8String &str)const{return this->SetX(result,str);}
    bool SetG(UTF8String &result,const UTF8String &str)const{return this->SetY(result,str);}
    bool SetB(UTF8String &result,const UTF8String &str)const{return this->SetZ(result,str);}
    bool SetA(UTF8String &result,const UTF8String &str)const{return this->SetW(result,str);}

    bool SetRG(UTF8String &result,const UTF8String &str)const{return this->SetXY(result,str);}
    bool SetGB(UTF8String &result,const UTF8String &str)const{return this->SetYZ(result,str);}
    bool SetBA(UTF8String &result,const UTF8String &str)const{return this->SetZW(result,str);}

    bool SetRGB(UTF8String &result,const UTF8String &str)const{return this->SetXYZ(result,str);}
    bool SetRGBA(UTF8String &result,const UTF8String &str)const{return this->SetXYZW(result,str);}

    virtual bool GetX(UTF8String &)const{return false;}
    virtual bool GetY(UTF8String &)const{return false;}
    virtual bool GetZ(UTF8String &)const{return false;}
    virtual bool GetW(UTF8String &)const{return false;}

    virtual bool GetXY(UTF8String &)const{return false;}
    virtual bool GetYZ(UTF8String &)const{return false;}
    virtual bool GetZW(UTF8String &)const{return false;}

    virtual bool GetXYZ(UTF8String &)const{return false;}
    virtual bool GetXYZW(UTF8String &)const{return false;}

    bool GetR(UTF8String &result)const{return this->GetX(result);}
    bool GetG(UTF8String &result)const{return this->GetY(result);}
    bool GetB(UTF8String &result)const{return this->GetZ(result);}
    bool GetA(UTF8String &result)const{return this->GetW(result);}

    bool GetRG(UTF8String &result)const{return this->GetXY(result);}
    bool GetGB(UTF8String &result)const{return this->GetYZ(result);}
    bool GetBA(UTF8String &result)const{return this->GetZW(result);}

    bool GetRGB(UTF8String &result)const{return this->GetXYZ(result);}
    bool GetRGBA(UTF8String &result)const{return this->GetXYZW(result);}
};//class DataSource

/**
 * 固定值数据源
 */
class DataSourceConst:public DataSource
{
};//class DataSourceConst:public DataSource

/**
 * Uniform数据源
 */
class DataSourceUniform:public DataSource
{
};//class DataSourceUniform:public DataSource

/**
 * 纹理数据源
 */
class DataSourceTexture:public DataSource
{
};//class DataSourceTexture:public DataSource

class DataSourceTexture1D:public DataSourceTexture
{
};//class DataSourceTexture1D:public DataSourceTexture

class DataSourceTexture2D:public DataSourceTexture
{
};//class DataSourceTexture2D:public DataSourceTexture

class DataSourceTexture2DArrays:public DataSourceTexture
{
};//class DataSourceTexture2DArrays:public DataSourceTexture

class DataSourceTextureCubemap:public DataSourceTexture
{
};//class DataSourceTextureCubemap:public DataSourceTexture

class DataSourceTextureCubemapArrays:public DataSourceTexture
{
};//class DataSourceTextureCubemapArrays:public DataSourceTexture
END_MATERIAL_NAMESPACE
#endif//HGL_GRAPH_MATERIAL_DATA_SOURCE_INCLUDE
