#include<hgl/db/FieldType.h>
namespace hgl
{
    namespace db
    {
        namespace
        {
            template<typename T>
            struct FieldTypeNameConvert
            {
                const T name[32];
                uint name_length;
                FieldBaseType type;
            };

            constexpr int field_size[(uint)FieldBaseType::RANGE_SIZE]={ 1,
                                                                        1,2,4,8,
                                                                        1,2,4,8,
                                                                        4,8,
                                                                        2,4,
                                                                        0,0};

            /**
             * 字段类型功能类
             */
            template<typename T> class FieldTypeClass
            {
                static FieldTypeNameConvert<T> filed_type_name_convert[];

            public:

                static FieldBaseType ParseType(const T *str,const uint str_size)
                {
                    T lower_name[32];

                    const uint name_len=lower_clip_cpy(lower_name,str,str_size);

                    if(name_len==0)
                        return FieldBaseType::ERROR_TYPE;

                    const FieldTypeNameConvert<T> *p=filed_type_name_convert;

                    for(uint i=(uint)FieldBaseType::BEGIN_RANGE;i<=(uint)FieldBaseType::END_RANGE;i++)
                    {
                        if(name_len==p->name_length)
                        {
                            if(hgl::strcmp(p->name,str,p->name_length)==0)
                                return(p->type);
                        }

                        ++p;
                    }

                    return FieldBaseType::ERROR_TYPE;
                }

                static bool ParseArrayType(FieldArray &fa,const T *str)
                {
                    if(!str)return(false);

                    const T *p=hgl::strchr(str,',');

                    if(!p)
                        return(false);

                    fa.base_type=ParseType(str,p-str);

                    if(fa.base_type==FieldBaseType::ERROR_TYPE)
                        return(false);

                    T num[32];

                    lower_clip_cpy(num,p+1);

                    return stou(num,fa.count);
                }
            };//template<typename T> class FieldTypeClass

            using FieldTypeClassU8=FieldTypeClass<char>;
            using FieldTypeClassU16=FieldTypeClass<u16char>;

        #define FIELD_TYPE_CONVERY(name,type)    {U8_TEXT(name),sizeof(name)-1,FieldBaseType::type}
            template<> FieldTypeNameConvert<char> FieldTypeClass<char>::filed_type_name_convert[]=
            #include"FieldTypeConvert.h"
        #undef FIELD_TYPE_CONVERY

        #define FIELD_TYPE_CONVERY(name,type)    {U16_TEXT(name),sizeof(name)-1,FieldBaseType::type}
            template<> FieldTypeNameConvert<u16char> FieldTypeClass<u16char>::filed_type_name_convert[]=
            #include"FieldTypeConvert.h"
        #undef FIELD_TYPE_CONVERY
        }//namespace

        const int GetFieldSize(const FieldBaseType fbt)
        {
            if(fbt<FieldBaseType::BEGIN_RANGE||fbt>FieldBaseType::END_RANGE)
                return -1;

            return field_size[(uint)fbt];
        }

        const int GetFieldSize(const FieldType *ft)
        {
            if(!ft)return(-1);
            if(ft->base_type<FieldBaseType::BEGIN_RANGE||ft->base_type>FieldBaseType::END_RANGE)
                return -1;

            if(ft->base_type<FieldBaseType::FixedArray)
                return GetFieldSize(ft->base_type);

            if(ft->base_type==FieldBaseType::FixedArray)
            {
                const FieldArray *fa=(const FieldArray *)ft;

                if(fa->count==0)return 0;

                return GetFieldSize(fa->base_type)*fa->count;
            }

            if(ft->base_type==FieldBaseType::VarArray)
                return 0;

            if(ft->base_type==FieldBaseType::Struct)
            {
                const FieldStruct *fs=(const FieldStruct *)ft;

                const uint count=fs->field_list.GetCount();
                FieldType **sft=fs->field_list.GetData();

                uint total=0;
                int size;
                for(uint i=0;i<count;i++)
                {
                    size=GetFieldSize(*sft);
                    if(size<0)
                        return(-1);

                    total+=size;
                    ++sft;
                }

                return total;
            }

            return(-1);
        }

        FieldBaseType ParseFieldType(const char *str)
        {
            return FieldTypeClassU8::ParseType(str,hgl::strlen(str));
        }

        FieldBaseType ParseFieldType(const u16char *str)
        {
            return FieldTypeClassU16::ParseType(str,hgl::strlen(str));
        }

        bool ParseArrayFieldType(FieldArray &fa,const char *str)
        {
            return FieldTypeClassU8::ParseArrayType(fa,str);
        }

        bool ParseArrayFieldType(FieldArray &fa,const u16char *str)
        {
            return FieldTypeClassU16::ParseArrayType(fa,str);
        }
    }//namespace db
}//namespace hgl
