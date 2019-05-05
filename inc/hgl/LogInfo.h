#ifndef HGL_LOGINFO_INCLUDE
#define HGL_LOGINFO_INCLUDE

#include<iostream>
#include<hgl/CodePage.h>

#if (HGL_COMPILER == HGL_COMPILER_GNU)||(HGL_COMPILER == HGL_COMPILER_LLVM)
    #define __HGL_FUNC__    __PRETTY_FUNCTION__
#elif HGL_COMPILER == HGL_COMPILER_Microsoft
    #define __HGL_FUNC__    __FUNCSIG__
#else
    #define __HGL_FUNC__    __FUNCTION__
#endif//

namespace hgl
{
    namespace logger
    {
        enum LogLevel
        {
            llError=0,      //错误，肯定出对话框
            llProblem,      //问题，默认出对话框
            llHint,         //提示，不重要，debug状态默认出对话框
            llLog           //记录一下
        };//enum LogLevel

        inline  void Log(LogLevel ll,const UTF16String &str)
        {
            std::wcout<<str.c_str()<<std::endl;
        }

        inline  void Log(LogLevel ll,const UTF8String &str)
        {
            std::cout<<str.c_str()<<std::endl;
        }

        inline  void DebugLog(LogLevel ll,const UTF16String &str,const char *filename,int line,const char *funcname)
        {
            Log(ll,str+U16_TEXT(">>LogFrom(\"")+to_u16(filename)+U16_TEXT("\", ")+UTF16String(line)+U16_TEXT(" line,func:\"")+to_u16(funcname)+U16_TEXT("\")"));
        }

        inline  void DebugLog(LogLevel ll,const UTF8String &str,const char *filename,int line,const char *funcname)
        {
            Log(ll,str+U8_TEXT(">>LogFrom(\"")+UTF8String(filename)+U8_TEXT("\", ")+UTF8String(line)+U8_TEXT(" line,func:\"")+UTF8String(funcname)+U8_TEXT("\")"));
        }

        #define LOG_INFO(str)       {Log(llLog,     str);}
        #define LOG_HINT(str)       {Log(llHint,    str);}
        #define LOG_PROBLEM(str)    {Log(llProblem, str);}
        #define LOG_ERROR(str)      {Log(llError,   str);}

        #define RETURN_FALSE        {DebugLog(llLog,OS_TEXT("return(false)"                         ),__FILE__,__LINE__,__HGL_FUNC__);return(false);}
        #define RETURN_ERROR(v)     {DebugLog(llLog,OS_TEXT("return error(")+OSString(v)+OS_TEXT(")"),__FILE__,__LINE__,__HGL_FUNC__);return(v);}
        #define RETURN_ERROR_NULL   {DebugLog(llLog,OS_TEXT("return error(nullptr)"                 ),__FILE__,__LINE__,__HGL_FUNC__);return(nullptr);}

        #define RETURN_BOOL(proc)   {if(proc)return(true);RETURN_FALSE}

        #define IF_FALSE_RETURN(str)    if(!str)RETURN_FALSE;
    }//namespace logger

    using namespace logger;
}//namespace hgl
#endif//HGL_LOGINFO_INCLUDE
