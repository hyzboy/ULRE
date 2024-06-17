﻿#pragma once

#include<hgl/graph/VertexAttrib.h>
#include<hgl/graph/VKSamplerType.h>
#include<hgl/graph/VKImageType.h>
#include<hgl/CompOperator.h>

namespace hgl
{
    namespace graph
    {
        enum class ShaderVariableBaseType:uint8
        {
            Scalar=0,
            Vector,
            Materix,
            Sampler,
            Image,
            AtomicCounter,
            ImplicitConversion,

            ENUM_CLASS_RANGE(Scalar,Materix)
        };//enum class ShaderVariableBaseType

        using SVBaseType=ShaderVariableBaseType;
        
    #pragma pack(push,1)

        struct ShaderVariableType
        {
            union
            {
                struct
                {
                    union
                    {
                        struct
                        {
                            SVBaseType base_type;

                            union
                            {
                                struct Scalar
                                {
                                    VABaseType type;
                                }scalar;

                                struct Vector
                                {
                                    VABaseType type;
                                    uint8 vec_size;
                                }vector;

                                struct Matrix
                                {
                                    VABaseType type;
                                    uint8 n;
                                    uint8 m;
                                }matrix;

                                struct Sampler
                                {
                                    SamplerType type;
                                }sampler;

                                struct Image
                                {
                                    ShaderImageType type;
                                }image;
                            };
                        };

                        uint32 type_code;
                    };

                    uint32 array_size;
                };

                uint64 svt_code;
            };

        public:

            ShaderVariableType()
            {
                svt_code=0;
            }

            ShaderVariableType(const ShaderVariableType &svt)
            {
                svt_code=svt.svt_code;
            }

            ShaderVariableType(const VABaseType &vabt,const uint32 count)
            {
                svt_code=0;

                base_type=SVBaseType::Scalar;

                scalar.type=vabt;

                array_size=count;
            }

            ShaderVariableType(const VABaseType &vabt,const uint8 size,const uint32 count)
            {
                svt_code=0;

                base_type=SVBaseType::Vector;

                vector.type=vabt;
                vector.vec_size=size;

                array_size=count;
            }

            ShaderVariableType(const VABaseType &vabt,const uint8 row,const uint8 col,const uint32 count)
            {
                svt_code=0;

                base_type=SVBaseType::Materix;

                matrix.type=vabt;
                matrix.n=col;
                matrix.m=row;

                array_size=count;
            }

            ShaderVariableType(const SamplerType &st,const uint32 count)
            {
                svt_code=0;

                base_type=SVBaseType::Sampler;

                sampler.type=st;

                array_size=count;
            }

            ShaderVariableType(const ShaderImageType &sit,const uint32 count)
            {
                svt_code=0;

                base_type=SVBaseType::Image;

                image.type=sit;

                array_size=count;
            }

            const bool Check()const
            {
                if(!RangeCheck(base_type))return(false);

                RANGE_CHECK_RETURN_FALSE(base_type)

                if(base_type==SVBaseType::Scalar)
                    return(true);

                if(base_type==SVBaseType::Vector)
                {
                    if(vector.vec_size<2||vector.vec_size>4)return(false);

                    return(true);
                }

                if(base_type==SVBaseType::Materix)
                {
                    if(matrix.m==0)
                    {
                        if(matrix.n<2||matrix.n>4)return(false);
                    }
                    else
                    {
                        if(matrix.n<2||matrix.n>4)return(false);
                        if(matrix.m<2||matrix.m>4)return(false);
                    }

                    return(true);
                }

                if(base_type==SVBaseType::Sampler)
                    return RangeCheck(sampler.type);

                if(base_type==SVBaseType::Image)
                    return RangeCheck(image.type);

                return(true);
            }

            int Comp(const ShaderVariableType &svt)const
            {
                int off=(int)base_type-(int)svt.base_type;

                if(off)return(off);

                if(base_type==SVBaseType::Scalar)
                    return int(scalar.type)-int(svt.scalar.type);

                if(base_type==SVBaseType::Vector)
                {
                    off=int(vector.type)-int(svt.vector.type);

                    if(off)return(off);

                    off=vector.vec_size-svt.vector.vec_size;

                    if(off)return(off);

                    return int(vector.type)-int(svt.vector.type);
                }

                if(base_type==SVBaseType::Materix)
                {
                    off=int(matrix.type)-int(svt.matrix.type);

                    if(off)return(off);

                    off=matrix.n-svt.matrix.n;

                    if(off)return(off);

                    return matrix.m-svt.matrix.m;
                }

                if(base_type==SVBaseType::Sampler)
                    return int(sampler.type)-int(svt.sampler.type);

                if(base_type==SVBaseType::Image)
                    return int(image.type)-int(svt.image.type);

                return 0;
            }

            CompOperator(const ShaderVariableType &,Comp)

            const uint64 ToCode()const{return svt_code;}
            const bool FromCode(const uint64 code)
            {
                svt_code=code;

                return Check();
            }

            const bool From(const VAType &vat,const uint16 count=1)
            {
                array_size=count;

                if(vat.vec_size==1)
                {
                    base_type=SVBaseType::Scalar;
                    scalar.type=vat.basetype;

                    return(true);
                }
                else if(vat.vec_size<=4)
                {
                    base_type=SVBaseType::Vector;

                    vector.type=vat.basetype;
                    vector.vec_size=vat.vec_size;

                    return(true);
                }

                return(false);
            }

            static const ShaderVariableType Scalar(const VABaseType &vabt,const uint count=1)
            {
                ShaderVariableType svt;

                svt.base_type   =SVBaseType::Scalar;
                svt.scalar.type =vabt;
                svt.array_size  =count;

                return svt;
            }
        };//struct ShaderVariableType
    #pragma pack(pop)

        using SVType=ShaderVariableType;

        const SVType SVT_BOOL  (VABaseType::Bool,    1);
        const SVType SVT_INT   (VABaseType::Int,     1);
        const SVType SVT_UINT  (VABaseType::UInt,    1);
        const SVType SVT_FLOAT (VABaseType::Float,   1);
        const SVType SVT_DOUBLE(VABaseType::Double,  1);

        const SVType SVT_BVEC2 (VABaseType::Bool,    2,1);
        const SVType SVT_BVEC3 (VABaseType::Bool,    3,1);
        const SVType SVT_BVEC4 (VABaseType::Bool,    4,1);

        const SVType SVT_IVEC2 (VABaseType::Int,     2,1);
        const SVType SVT_IVEC3 (VABaseType::Int,     3,1);
        const SVType SVT_IVEC4 (VABaseType::Int,     4,1);

        const SVType SVT_UVEC2 (VABaseType::UInt,    2,1);
        const SVType SVT_UVEC3 (VABaseType::UInt,    3,1);
        const SVType SVT_UVEC4 (VABaseType::UInt,    4,1);

        const SVType SVT_VEC2  (VABaseType::Float,   2,1);
        const SVType SVT_VEC3  (VABaseType::Float,   3,1);
        const SVType SVT_VEC4  (VABaseType::Float,   4,1);

        const SVType SVT_DVEC2 (VABaseType::Double,  2,1);
        const SVType SVT_DVEC3 (VABaseType::Double,  3,1);
        const SVType SVT_DVEC4 (VABaseType::Double,  4,1);

        const SVType SVT_MAT2  (VABaseType::Float,   2,2,1);
        const SVType SVT_MAT3  (VABaseType::Float,   3,3,1);
        const SVType SVT_MAT4  (VABaseType::Float,   4,4,1);
        const SVType SVT_MAT2x3(VABaseType::Float,   2,3,1);
        const SVType SVT_MAT2x4(VABaseType::Float,   2,4,1);
        const SVType SVT_MAT3x2(VABaseType::Float,   3,2,1);
        const SVType SVT_MAT3x4(VABaseType::Float,   3,4,1);
        const SVType SVT_MAT4x2(VABaseType::Float,   4,2,1);
        const SVType SVT_MAT4x3(VABaseType::Float,   4,3,1);

        const SVType SVT_Sampler1D(SamplerType::Sampler1D,          1);
        const SVType SVT_Sampler2D(SamplerType::Sampler2D,          1);
        const SVType SVT_Sampler3D(SamplerType::Sampler3D,          1);

        const SVType SVT_SamplerCube(SamplerType::SamplerCube,      1);
        const SVType SVT_Sampler2DRect(SamplerType::Sampler2DRect,  1);
        
        const SVType SVT_Sampler1DArray(SamplerType::Sampler1DArray,1);
        const SVType SVT_Sampler2DArray(SamplerType::Sampler2DArray,1);

        const SVType SVT_SamplerCubeArray(SamplerType::SamplerCubeArray,1);

        const SVType SVT_SamplerBuffer(SamplerType::SamplerBuffer,1);

        const SVType SVT_Sampler2DMS(SamplerType::Sampler2DMS,1);
        const SVType SVT_Sampler2DMSArray(SamplerType::Sampler2DMSArray,1);

        const SVType SVT_Sampler1DShadow(SamplerType::Sampler1DShadow,1);
        const SVType SVT_Sampler2DShadow(SamplerType::Sampler2DShadow,1);

        const SVType SVT_SamplerCubeShadow(SamplerType::SamplerCubeShadow,1);
        const SVType SVT_Sampler2DRectShadow(SamplerType::Sampler2DRectShadow,1);

        const SVType SVT_Sampler1DArrayShadow(SamplerType::Sampler1DArrayShadow,1);
        const SVType SVT_Sampler2DArrayShadow(SamplerType::Sampler2DArrayShadow,1);
        const SVType SVT_SamplerCubeArrayShadow(SamplerType::SamplerCubeArrayShadow,1);

        const SVType SVT_Image1D(ShaderImageType::Image1D,1);
        const SVType SVT_Image2D(ShaderImageType::Image2D,1);
        const SVType SVT_Image3D(ShaderImageType::Image3D,1);

        const SVType SVT_ImageCube(ShaderImageType::ImageCube,1);
        const SVType SVT_Image2DRect(ShaderImageType::Image2DRect,1);

        const SVType SVT_Image1DArray(ShaderImageType::Image1DArray,1);
        const SVType SVT_Image2DArray(ShaderImageType::Image2DArray,1);

        const SVType SVT_ImageCubeArray(ShaderImageType::ImageCubeArray,1);

        const SVType SVT_ImageBuffer(ShaderImageType::ImageBuffer,1);

        const SVType SVT_Image2DMS(ShaderImageType::Image2DMS,1);
        const SVType SVT_Image2DMSArray(ShaderImageType::Image2DMSArray,1);
    }//namespace graph
}//namespace hgl
