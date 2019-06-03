#ifndef HGL_DB_FIELD_TYPE_INCLUDE
#define HGL_DB_FIELD_TYPE_INCLUDE

#include<hgl/type/List.h>
#include<hgl/type/StringList.h>
namespace hgl
{
    namespace db
    {
        /**
        * 字段基本类型枚举
        */
        enum class FieldBaseType
        {
            Bool=0,

            Int8,            Int16,           Int32,           Int64,
            Uint8,           Uint16,          Uint32,          Uint64,

            Float,
            Double,

            UTF16LE,
            UTF32LE,

            Array,          ///<阵列
            Struct,         ///<结构体

            BEGIN_RANGE =Bool,
            END_RANGE   =Struct,
            RANGE_SIZE  =END_RANGE-BEGIN_RANGE+1,
            ERROR_TYPE=0xFF
        };//enum class FieldBaseType

        struct FieldType
        {
            FieldBaseType base_type;

        public:

            FieldType():base_type(FieldBaseType::ERROR_TYPE){}
            FieldType(const FieldBaseType &bt):base_type(bt){}
            virtual ~FieldType()=default;

            FieldBaseType GetFieldType()const{return base_type;}
        };

        struct FieldArrayType:public FieldType
        {
            FieldBaseType member_type;
            uint64 length;

        public:

            FieldArrayType():FieldType(FieldBaseType::Array),member_type(FieldBaseType::ERROR_TYPE),length(0){}
            FieldArrayType(const FieldBaseType &fbt,const uint64 &c):FieldType(FieldBaseType::Array),member_type(fbt),length(c){}
            FieldArrayType(const FieldArrayType &fa):FieldType(FieldBaseType::Array),member_type(fa.member_type),length(fa.length){}
            virtual ~FieldArrayType()=default;

            const FieldBaseType GetMemberType   ()const{return member_type;}
            const uint64        GetLength       ()const{return length;}
        };//struct FieldArrayType

        struct FieldStructType:public FieldType
        {
            ObjectList<FieldType> field_list;

        public:

            FieldStructType():FieldType(FieldBaseType::Struct){}
            virtual ~FieldStructType()=default;
        };//struct FieldStructType

        const int GetFieldSize(const FieldBaseType fbt);
        const int GetFieldSize(const FieldType *ft);

        FieldBaseType ParseFieldType(const char *str);
        FieldBaseType ParseFieldType(const u16char *str);

        bool ParseArrayFieldType(FieldArrayType &,const char *);
        bool ParseArrayFieldType(FieldArrayType &,const u16char *);
    }//namespace db
}//namespace hgl
#endif//HGL_DB_FIELD_TYPE_INCLUDE
