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

    const ComponentDataType GetDataType()const{return GetFormatBaseType(format);}                   ///<获取数据基本类型
    const uint              GetChannels()const{return GetFormatChannels(format);}                   ///<获取数据通道数量

public:

    DataSource()                    {format=DataFormat::NONE;}
    DataSource(const DataFormat &df){format=df;}
    virtual ~DataSource()=default;
};//class DataSource

/**
 * 固定值数据源
 */
class DataSourceConst:public DataSource
{
public:

    using DataSource::DataSource;
};//class DataSourceConst:public DataSource

/**
 * Uniform 数据源
 */
class DataSourceUniform:public DataSource
{
public:

    using DataSource::DataSource;
};//class DataSourceUniform:public DataSource

/**
 * 函数数据源
 */
 class DataSourceFunction:public DataSource
 {
public:

    using DataSource::DataSource;
 };//class DataSourceFunction:public DataSource

/**
 * 纹理数据源
 */
class DataSourceTexture:public DataSource
{
public:

    using DataSource::DataSource;
};//class DataSourceTexture:public DataSource

class DataSourceTexture1D:public DataSourceTexture
{
public:

    using DataSourceTexture::DataSourceTexture;
};//class DataSourceTexture1D:public DataSourceTexture

class DataSourceTexture1DArrays:public DataSourceTexture
{
public:

    using DataSourceTexture::DataSourceTexture;
};//class DataSourceTexture1DArrays:public DataSourceTexture

class DataSourceTexture2D:public DataSourceTexture
{
public:

    using DataSourceTexture::DataSourceTexture;
};//class DataSourceTexture2D:public DataSourceTexture

class DataSourceTexture2DArrays:public DataSourceTexture
{
public:

    using DataSourceTexture::DataSourceTexture;
};//class DataSourceTexture2DArrays:public DataSourceTexture

class DataSourceTextureCubemap:public DataSourceTexture
{
public:

    using DataSourceTexture::DataSourceTexture;
};//class DataSourceTextureCubemap:public DataSourceTexture

class DataSourceTextureCubemapArrays:public DataSourceTexture
{
public:

    using DataSourceTexture::DataSourceTexture;
};//class DataSourceTextureCubemapArrays:public DataSourceTexture
END_MATERIAL_NAMESPACE
#endif//HGL_GRAPH_MATERIAL_DATA_SOURCE_INCLUDE
