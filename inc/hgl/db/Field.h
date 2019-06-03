#ifndef HGL_DB_FIELD_INCLUDE
#define HGL_DB_FIELD_INCLUDE

#include<hgl/db/FieldType.h>
namespace hgl
{
    namespace db
    {
        /**
         * 字段基类
         */
        class Field
        {
            FieldType *field_type;

        public:

            const FieldType *GetFieldType()const{return field_type;}                         ///<获取字段类型

        public:

            Field(FieldType *ft):field_type(ft){}
            virtual ~Field()
            {
                if(field_type)
                    delete field_type;
            }
        };//class Field

        /**
         * 单个数据字段模板基类
         */
        template<typename T> class FieldSingleValue:public Field
        {
        public:

            using Field::Field;
            virtual ~FieldSingleValue()=default;
        };//template<typename T> class FieldSingleValue:public Field

        class FieldArrayBase:public Field
        {
        protected:

            FieldArrayType *array_type;

        public:

            FieldArrayBase(FieldArrayType *fa):Field(fa),array_type(fa){}
            virtual ~FieldArrayBase()=default;

            const FieldBaseType GetMemberType   ()const{return array_type?array_type->GetMemberType():FieldBaseType::ERROR_TYPE;}
            const uint64        GetLength       ()const{return array_type?array_type->GetLength():-1;}
        };//class FieldVarArray:public Field

        template<typename T> class FieldArray:public FieldArrayBase
        {
        public:

            using FieldArrayBase::FieldArrayBase;
            virtual ~FieldArray()=default;
        };//template<typename T> class FieldArray:public FieldArrayBase

        #define FIELD_TYPE_DEFINE(type_name,type)   using Field##type_name=FieldSingleValue<type>;  \
                                                    using Field##type_name##Array=FieldArray<type>;

            FIELD_TYPE_DEFINE(Bool,     bool)

            FIELD_TYPE_DEFINE(Int8,     int8)
            FIELD_TYPE_DEFINE(Int16,    int16)
            FIELD_TYPE_DEFINE(Int32,    int32)
            FIELD_TYPE_DEFINE(Int64,    int64)

            FIELD_TYPE_DEFINE(Uint8,    uint8)
            FIELD_TYPE_DEFINE(Uint16,   uint16)
            FIELD_TYPE_DEFINE(Uint32,   uint32)
            FIELD_TYPE_DEFINE(Uint64,   uint64)

            FIELD_TYPE_DEFINE(Float,    float)
            FIELD_TYPE_DEFINE(Double,   double)

            FIELD_TYPE_DEFINE(Char,     char)
            FIELD_TYPE_DEFINE(UTF16,    u16char)

        #undef FIELD_TYPE_DEFINE

        class FieldStruct:public Field
        {
            FieldStructType *struct_type;

        public:

            FieldStruct(FieldStructType *fs):Field(fs),struct_type(fs){}
            virtual ~FieldStruct()=default;
        };//class FieldStruct:public Field
    }//namespace db
}//namespace hgl
#endif//HGL_DB_FIELD_INCLUDE
