#ifndef HGL_GRAPH_VERTEX_ATTRIB_DATA_ACCESS_INCLUDE
#define HGL_GRAPH_VERTEX_ATTRIB_DATA_ACCESS_INCLUDE

#include<hgl/color/Color3f.h>
#include<hgl/color/Color4f.h>
#include<hgl/type/RectScope.h>
#include<hgl/type/String.h>
#include<hgl/log/ObjectLogger.h>
#include<hgl/graph/VKFormat.h>
#include<hgl/graph/AABB.h>
namespace hgl
{
    namespace graph
    {
        /**
        * 顶点属性数据访问模板
        */
        template<typename T,int C> class VertexAttribDataAccess
        {
        protected:

            T *         data;                                                                       ///<符合当前类型的地址
            T *         data_end;                                                                   ///<内存数据区访问结束地址
            uint32_t    count;                                                                      ///<数据个数

            uint32_t    total_bytes;                                                                ///<数据总字节数

            T *         access;                                                                     ///<当前访问地址
            T *         start_access;                                                               ///<访问起始地址

        public:

            VertexAttribDataAccess(uint32_t _size,T *_data)
            {
                data    =_data;
                count   =_size;
                data_end=_data+_size*C;

                total_bytes =_size*C*sizeof(T);

                access      =nullptr;
                start_access=nullptr;
            }

            virtual ~VertexAttribDataAccess()=default;

            void SetData(T *_data)
            {
                data    =_data;
                data_end=_data+count*C;
            }

            const bool IsValid()const
            {
                return data;
            }

            void Write(const T *ptr)
            {
                if(!ptr)return;

                memcpy(data,ptr,total_bytes);
            }

            /**
            * 取得数据区地址
            * @param start 从第几个数据开始访问
            * @return 访问地址
            */
            T *Get(uint32_t offset=0)
            {
                if(!data||offset>=count)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess::Get() out,start:")+OSString::numberOf(offset));
                    return(nullptr);
                }

                return data+offset*C;
            }

            /**
            * 开始访问数据区
            * @param start 从第几个数据开始访问
            * @return 访问地址
            */
            T *Begin(uint32_t offset=0)
            {
                if(access)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess::Begin() access!=0,start:")+OSString::numberOf(offset));
                    return(nullptr);
                }

                access=Get(offset);

                if(access)
                    start_access=access;

                return access;
            }

            /**
            * 结束访问
            */
            void End()
            {
                access=nullptr;
                start_access=nullptr;
            }

            /**
            * 写入指定数量的数据
            * @param vp 数据指针
            * @param number 数据数量
            */
            bool WriteData(const T *vp,const uint32_t number)
            {
                if(!this->access||this->access+C*number>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess::Write(const T *,number) out,number:")+OSString::numberOf(number));
                    return(false);
                }

                memcpy(access,vp,C*number*sizeof(T));

                access+=C*number;

                return(true);
            }
        };//class VertexAttribDataAccess

        /**
        * 一元数据缓冲区
        */
        template<typename T,VkFormat VKFMT> class VertexAttribDataAccess1:public VertexAttribDataAccess<T,1>
        {
        public:

            using VertexAttribDataAccess<T,1>::VertexAttribDataAccess;
            virtual ~VertexAttribDataAccess1()=default;

            static VkFormat GetVulkanFormat(){return VKFMT;}

            static VertexAttribDataAccess1<T,VKFMT> *   Create(const VkDeviceSize vertices_number,void *vbo_data)
            {
                return(new VertexAttribDataAccess1<T,VKFMT>(vertices_number,(T *)vbo_data));
            }

            /**
            * 计算绑定盒
            * @param min_vertex 最小值坐标
            * @param max_vertex 最大值坐标
            */
            template<typename V>
            void GetBoundingBox(V &min_vertex,V &max_vertex) const
            {
                T *p=this->data;

                //先以corner为最小值,length为最大值，求取最小最大值
                min_vertex.x=*p++;

                max_vertex=min_vertex;

                for(uint32_t i=1;i<this->count;i++)
                {
                    if(*p<min_vertex.x)min_vertex.x=*p;
                    if(*p>max_vertex.x)max_vertex.x=*p;
                    ++p;
                }
            }

            AABB GetAABB()const
            {
                glm::vec4 min_point,max_point;

                GetBoundingBox(min_point,max_point);

                min_point.y=0;
                min_point.z=0;
                min_point.w=0;

                max_point.y=0;
                max_point.z=0;
                max_point.w=0;

                return AABB(min_point,max_point);
            }

            bool Write(const T v1)
            {
                if(!this->access||this->access+1>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess1::Write(const T) out"));
                    return(false);
                }

                *this->access++=v1;
                return(true);
            }

            /**
             * 将一个值重复多次写入缓冲区
             * @param v 值
             * @param count 写入数量
             */
            bool RepeatWrite(const T v,const uint32_t count)
            {
                if(!this->access||this->access+count>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess1::Write(const T,")+OSString::numberOf(count)+OS_TEXT(") out"));
                    return(false);
                }

                hgl_set(this->access,v,count);
                this->access+=count;
                return(true);
            }
        };//class VertexAttribDataAccess1

        /**
        * 二元数据缓冲区
        */
        template<typename T,VkFormat VKFMT> class VertexAttribDataAccess2:public VertexAttribDataAccess<T,2>
        {
        public:

            using VertexAttribDataAccess<T,2>::VertexAttribDataAccess;
            virtual ~VertexAttribDataAccess2()=default;

            static VkFormat GetVulkanFormat(){return VKFMT;}

            static VertexAttribDataAccess2<T,VKFMT> *   Create(const VkDeviceSize vertices_number,void *vbo_data)
            {
                return(new VertexAttribDataAccess2<T,VKFMT>(vertices_number,(T *)vbo_data));
            }

            /**
            * 计算绑定盒
            * @param min_vertex 最小值坐标
            * @param max_vertex 最大值坐标
            */
            template<typename V>
            void GetBoundingBox(V &min_vertex,V &max_vertex) const
            {
                T *p=this->data;

                //先以corner为最小值,length为最大值，求取最小最大值
                min_vertex.x=*p++;
                min_vertex.y=*p++;

                max_vertex=min_vertex;

                for(uint32_t i=1;i<this->count;i++)
                {
                    if(*p<min_vertex.x)min_vertex.x=*p;
                    if(*p>max_vertex.x)max_vertex.x=*p;
                    ++p;

                    if(*p<min_vertex.y)min_vertex.y=*p;
                    if(*p>max_vertex.y)max_vertex.y=*p;
                    ++p;
                }
            }

            AABB GetAABB()const
            {
                glm::vec4 min_point,max_point;

                GetBoundingBox(min_point,max_point);

                min_point.z=0;
                min_point.w=0;
                max_point.z=0;
                max_point.w=0;

                return AABB(min_point,max_point);
            }

            bool Write(const T v1,const T v2)
            {
                if(!this->access||this->access+2>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess2::Write(const T ,const T) out"));
                    return(false);
                }

                *this->access++=v1;
                *this->access++=v2;

                return(true);
            }

            bool Write(const T *v)
            {
                if(!this->access||this->access+2>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess2::Write(T *) out"));
                    return(false);
                }

                *this->access++=*v++;
                *this->access++=*v;

                return(true);
            }

            bool Write(const T *v,const uint count)
            {
                if(!this->access||this->access+2>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess2::Write(T *) out"));
                    return(false);
                }

                for(uint i=0;i<count;i++)
                {
                    *this->access++=*v++;
                    *this->access++=*v++;
                }

                return(true);
            }

            template<typename V2>
            bool Write(const V2 &v)
            {
                if(!this->access||this->access+2>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess2::Write(vec2 &) out"));
                    return(false);
                }

                *this->access++=v.x;
                *this->access++=v.y;

                return(true);
            }

            /**
             * 将一个值重复多次写入缓冲区
             * @param v 值
             * @param count 写入数量
             */
            template<typename V2>
            bool RepeatWrite(const V2 &v,const uint32_t count)
            {
                if(!this->access||this->access+(count<<1)>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess1::Write(const Vector2f &,")+OSString::numberOf(count)+OS_TEXT(") out"));
                    return(false);
                }

                for(uint32_t i=0;i<count;i++)
                {
                    *this->access++=v.x;
                    *this->access++=v.y;
                }

                return(true);
            }

            bool WriteLine(const T start_x,const T start_y,const T end_x,const T end_y)
            {
                if(!this->access||this->access+4>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess2::WriteLine(T,T,T,T) out"));
                    return(false);
                }

                *this->access++=start_x;
                *this->access++=start_y;
                *this->access++=end_x;
                *this->access++=end_y;

                return(true);
            }

            template<typename V2>
            bool WriteLine(const V2 &start,const V2 &end)
            {
                if(!this->access||this->access+4>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess2::WriteLine(vec2,vec2) out"));
                    return(false);
                }

                *this->access++=start.x;
                *this->access++=start.y;
                *this->access++=end.x;
                *this->access++=end.y;

                return(true);
            }

            /**
            * 写入2D三角形
            */
            template<typename V2>
            bool WriteTriangle(const V2 &v1,const V2 &v2,const V2 &v3)
            {
                if(!this->access||this->access+6>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess2::WriteTriangle(vec2,vec2,vec2) out"));
                    return(false);
                }

                *this->access++=v1.x;
                *this->access++=v1.y;

                *this->access++=v2.x;
                *this->access++=v2.y;

                *this->access++=v3.x;
                *this->access++=v3.y;

                return(true);
            }

            /**
            * 写入2D三角形
            */
            template<typename V2>
            bool WriteTriangle(const V2 *v)
            {
                if(!this->access||this->access+6>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess2::WriteTriangle(vec2 *) out"));
                    return(false);
                }

                *this->access++=v->x;
                *this->access++=v->y;
                ++v;

                *this->access++=v->x;
                *this->access++=v->y;
                ++v;

                *this->access++=v->x;
                *this->access++=v->y;

                return(true);
            }

            /**
            * 写入2D四边形坐标数据
            */
            template<typename V2>
            bool WriteQuad(const V2 &lt,const V2 &rt,const V2 &rb,const V2 &lb)
            {
                if(WriteTriangle(lt,lb,rb))
                if(WriteTriangle(lt,rb,rt))
                    return(true);

                GLogHint(OS_TEXT("VertexAttribDataAccess2::WriteQuad(vec2 &,vec2 &,vec2 &,vec2 &) error"));
                return(false);
            }

            /**
            * 写入2D矩形（两个三角形）坐标数据
            */
            template<typename V>
            bool WriteRect(const T left,const T top,const T width,const T height)
            {
                const glm::vec<2,V,glm::defaultp> lt(left      ,top);
                const glm::vec<2,V,glm::defaultp> rt(left+width,top);
                const glm::vec<2,V,glm::defaultp> rb(left+width,top+height);
                const glm::vec<2,V,glm::defaultp> lb(left      ,top+height);

                return WriteQuad(lt,rt,rb,lb);
            }

            template<typename V>
            bool WriteRect(const RectScope2<V> &scope)
            {
                return WriteQuad(   scope.GetLeftTop(),
                                    scope.GetRightTop(),
                                    scope.GetRightBottom(),
                                    scope.GetLeftBottom());
            }

            template<typename V>
            bool WriteRectFan(const RectScope2<V> &scope)
            {
                if(!this->access||this->access+8>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess2::WriteRectFan(RectScope2 *) out"));
                    return(false);
                }

                *this->access++=scope.GetLeft();
                *this->access++=scope.GetBottom();

                *this->access++=scope.GetRight();
                *this->access++=scope.GetBottom();

                *this->access++=scope.GetRight();
                *this->access++=scope.GetTop();

                *this->access++=scope.GetLeft();
                *this->access++=scope.GetTop();

                return(true);
            }

            template<typename V>
            bool WriteRectTriangleStrip(const RectScope2<V> &scope)
            {
                if(!this->access||this->access+8>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess2::WriteRectTriangleStrip(RectScope2 *) out"));
                    return(false);
                }

                *this->access++=scope.GetLeft();
                *this->access++=scope.GetTop();

                *this->access++=scope.GetLeft();
                *this->access++=scope.GetBottom();

                *this->access++=scope.GetRight();
                *this->access++=scope.GetTop();

                *this->access++=scope.GetRight();
                *this->access++=scope.GetBottom();

                return(true);
            }
        };//class VertexAttribDataAccess2

        /**
        * 三元数据缓冲区
        */
        template<typename T,VkFormat VKFMT> class VertexAttribDataAccess3:public VertexAttribDataAccess<T,3>
        {
        public:

            using VertexAttribDataAccess<T,3>::VertexAttribDataAccess;
            virtual ~VertexAttribDataAccess3()=default;

            static VkFormat GetVulkanFormat(){return VKFMT;}

            static VertexAttribDataAccess3<T,VKFMT> *   Create(const VkDeviceSize vertices_number,void *vbo_data)
            {
                return(new VertexAttribDataAccess3<T,VKFMT>(vertices_number,(T *)vbo_data));
            }

            /**
            * 计算绑定盒
            * @param min_vertex 最小值坐标
            * @param max_vertex 最大值坐标
            */
            template<typename V>
            void GetBoundingBox(V &min_vertex,V &max_vertex) const
            {
                T *p=this->data;

                //先以corner为最小值,length为最大值，求取最小最大值
                min_vertex.x=*p++;
                min_vertex.y=*p++;
                min_vertex.z=*p++;

                max_vertex=min_vertex;

                for(uint32_t i=1;i<this->count;i++)
                {
                    if(*p<min_vertex.x)min_vertex.x=*p;
                    if(*p>max_vertex.x)max_vertex.x=*p;
                    ++p;

                    if(*p<min_vertex.y)min_vertex.y=*p;
                    if(*p>max_vertex.y)max_vertex.y=*p;
                    ++p;

                    if(*p<min_vertex.z)min_vertex.z=*p;
                    if(*p>max_vertex.z)max_vertex.z=*p;
                    ++p;
                }
            }

            AABB GetAABB()const
            {
                glm::vec4 min_point,max_point;

                GetBoundingBox(min_point,max_point);

                min_point.w=0;
                max_point.w=0;

                return AABB(min_point,max_point);
            }

            bool Write(const T v1,const T v2,const T v3)
            {
                if(!this->access||this->access+3>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess3::Write(T,T,T) out"));
                    return(false);
                }

                *this->access++=v1;
                *this->access++=v2;
                *this->access++=v3;

                return(true);
            }

            bool Write3(const T *v)
            {
                if(!this->access||this->access+3>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess3::Write(T *) out"));
                    return(false);
                }

                *this->access++=*v++;
                *this->access++=*v++;
                *this->access++=*v;

                return(true);
            }

            template<typename V3>
            bool Write(const V3 &v)
            {
                if(!this->access||this->access+3>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess3::Write(vec3 &) out"));
                    return(false);
                }

                *this->access++=v.x;
                *this->access++=v.y;
                *this->access++=v.z;

                return(true);
            }

            /**
             * 将一个值重复多次写入缓冲区
             * @param v 值
             * @param count 写入数量
             */
            template<typename V3>
            bool RepeatWrite(const V3 &v,const uint32_t count)
            {
                if(!this->access||this->access+(count*3)>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess3::Write(const Vector3f,")+OSString::numberOf(count)+OS_TEXT(") out"));
                    return(false);
                }

                for(uint32_t i=0;i<count;i++)
                {
                    *this->access++=v.x;
                    *this->access++=v.y;
                    *this->access++=v.z;
                }

                return(true);
            }

            /**
            * 写入多个值到缓冲区
            * @param v 值
            * @param count 写入数量
            */
            template<typename V3>
            bool Write(const V3 *v,const uint32_t count)
            {
                if(!this->access||this->access+(count*3)>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess3::Write(const Vector3f,")+OSString::numberOf(count)+OS_TEXT(") out"));
                    return(false);
                }

                for(uint32_t i=0;i<count;i++)
                {
                    *this->access++=v->x;
                    *this->access++=v->y;
                    *this->access++=v->z;

                    ++v;
                }

                return(true);
            }

            bool Write(const Color3f &v)
            {
                if(!this->access||this->access+3>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess3::Write(color3f &) out"));
                    return(false);
                }

                *this->access++=v.r;
                *this->access++=v.g;
                *this->access++=v.b;

                return(true);
            }

            bool WriteLine(const T start_x,const T start_y,const T start_z,const T end_x,const T end_y,const T end_z)
            {
                if(!this->access||this->access+6>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess3::WriteLine(T,T,T,T,T,T) out"));
                    return(false);
                }

                *this->access++=start_x;
                *this->access++=start_y;
                *this->access++=start_z;
                *this->access++=end_x;
                *this->access++=end_y;
                *this->access++=end_z;

                return(true);
            }

            template<typename V3>
            bool WriteLine(const V3 &start,const V3 &end)
            {
                if(!this->access||this->access+6>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess3::WriteLine(vec3,vec3) out"));
                    return(false);
                }

                *this->access++=start.x;
                *this->access++=start.y;
                *this->access++=start.z;
                *this->access++=end.x;
                *this->access++=end.y;
                *this->access++=end.z;

                return(true);
            }

            /**
            * 写入3D三角形
            */
            template<typename V3>
            bool WriteTriangle(const V3 &v1,const V3 &v2,const V3 &v3)
            {
                if(!this->access||this->access+9>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess3::WriteTriangle(vec3,vec3,vec3) out"));
                    return(false);
                }

                *this->access++=v1.x;
                *this->access++=v1.y;
                *this->access++=v1.z;

                *this->access++=v2.x;
                *this->access++=v2.y;
                *this->access++=v2.z;

                *this->access++=v3.x;
                *this->access++=v3.y;
                *this->access++=v3.z;

                return(true);
            }

            /**
            * 写入3D三角形
            */
            template<typename V3>
            bool WriteTriangle(const V3 *v)
            {
                if(!this->access||this->access+9>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess3::WriteTriangle(vec3 *) out"));
                    return(false);
                }

                *this->access++=v->x;
                *this->access++=v->y;
                *this->access++=v->z;
                ++v;

                *this->access++=v->x;
                *this->access++=v->y;
                *this->access++=v->z;
                ++v;

                *this->access++=v->x;
                *this->access++=v->y;
                *this->access++=v->z;

                return(true);
            }
        };//class VertexAttribDataAccess3

        /**
        * 四元数据缓冲区
        */
        template<typename T,VkFormat VKFMT> class VertexAttribDataAccess4:public VertexAttribDataAccess<T,4>
        {
        public:

            using VertexAttribDataAccess<T,4>::VertexAttribDataAccess;
            virtual ~VertexAttribDataAccess4()=default;

            static VkFormat GetVulkanFormat(){return VKFMT;}

            static VertexAttribDataAccess4<T,VKFMT> *   Create(const VkDeviceSize vertices_number,void *vbo_data)
            {
                return(new VertexAttribDataAccess4<T,VKFMT>(vertices_number,(T *)vbo_data));
            }

            /**
            * 计算绑定盒
            * @param min_vertex 最小值坐标
            * @param max_vertex 最大值坐标
            */
            template<typename V>
            void GetBoundingBox(V &min_vertex,V &max_vertex) const
            {
                T *p=this->data;

                //先以corner为最小值,length为最大值，求取最小最大值
                min_vertex.x=*p++;
                min_vertex.y=*p++;
                min_vertex.z=*p++;
                ++p;

                max_vertex=min_vertex;

                for(uint32_t i=1;i<this->count;i++)
                {
                    if(*p<min_vertex.x)min_vertex.x=*p;
                    if(*p>max_vertex.x)max_vertex.x=*p;
                    ++p;

                    if(*p<min_vertex.y)min_vertex.y=*p;
                    if(*p>max_vertex.y)max_vertex.y=*p;
                    ++p;

                    if(*p<min_vertex.z)min_vertex.z=*p;
                    if(*p>max_vertex.z)max_vertex.z=*p;
                    ++p;

                    ++p;
                }
            }

            AABB GetAABB()const
            {
                glm::vec4 min_point,max_point;

                GetBoundingBox(min_point,max_point);

                min_point.w=0;
                max_point.w=0;

                return AABB(min_point,max_point);
            }

            bool Write(const T v1,const T v2,const T v3,const T v4)
            {
                if(!this->access||this->access+4>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess4::Write(T,T,T,T) out"));
                    return(false);
                }

                *this->access++=v1;
                *this->access++=v2;
                *this->access++=v3;
                *this->access++=v4;

                return(true);
            }

            bool Write(const T *v)
            {
                if(!this->access||this->access+4>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess4::Write(T *) out"));
                    return(false);
                }

                *this->access++=*v++;
                *this->access++=*v++;
                *this->access++=*v++;
                *this->access++=*v;

                return(true);
            }

            template<typename V4>
            bool Write(const V4 &v)
            {
                if(!this->access||this->access+4>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess4::Write(color4 &) out"));
                    return(false);
                }

                *this->access++=v.x;
                *this->access++=v.y;
                *this->access++=v.z;
                *this->access++=v.w;

                return(true);
            }

            bool Write(const Color4f &v)
            {
                if(!this->access||this->access+4>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess4::Write(color4 &) out"));
                    return(false);
                }

                *this->access++=v.r;
                *this->access++=v.g;
                *this->access++=v.b;
                *this->access++=v.a;

                return(true);
            }

            bool RepeatWrite(const Color4f &v,const uint32_t count)
            {
                if(count<=0)return(false);

                if(!this->access||this->access+(4*count)>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess4::Write(color4 &,count) out"));
                    return(false);
                }

                for(uint32_t i=0;i<count;i++)
                {
                    *this->access++=v.r;
                    *this->access++=v.g;
                    *this->access++=v.b;
                    *this->access++=v.a;
                }

                return(true);
            }

            /**
             * 将一个值重复多次写入缓冲区
             * @param v 值
             * @param count 写入数量
             */
            template<typename V4>
            bool RepeatWrite(const V4 &v,const uint32_t count)
            {
                if(!this->access||this->access+(count<<2)>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess4::Write(const Vector4f,")+OSString::numberOf(count)+OS_TEXT(") out"));
                    return(false);
                }

                for(uint32_t i=0;i<count;i++)
                {
                    *this->access++=v.x;
                    *this->access++=v.y;
                    *this->access++=v.z;
                    *this->access++=v.w;
                }

                return(true);
            }

            /**
            * 将多个值写入缓冲区
            * @param v 值
            * @param count 写入数量
            */
            template<typename V4>
            bool Write(const V4 *v,const uint32_t count)
            {
                if(!this->access||this->access+(count<<2)>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess4::Write(const Vector4f,")+OSString::numberOf(count)+OS_TEXT(") out"));
                    return(false);
                }

                for(uint32_t i=0;i<count;i++)
                {
                    *this->access++=v->x;
                    *this->access++=v->y;
                    *this->access++=v->z;
                    *this->access++=v->w;

                    ++v;
                }

                return(true);
            }

            bool WriteLine(const T start_x,const T start_y,const T start_z,const T end_x,const T end_y,const T end_z)
            {
                if(!this->access||this->access+8>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess4::WriteLine(T,T,T,T,T,T) out"));
                    return(false);
                }

                *this->access++=start_x;
                *this->access++=start_y;
                *this->access++=start_z;
                *this->access++=1.0f;
                *this->access++=end_x;
                *this->access++=end_y;
                *this->access++=end_z;
                *this->access++=1.0f;

                return(true);
            }

            template<typename V4>
            bool WriteLine(const V4 &start,const V4 &end)
            {
                if(!this->access||this->access+8>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess4::WriteLine(vec3,vec3) out"));
                    return(false);
                }

                *this->access++=start.x;
                *this->access++=start.y;
                *this->access++=start.z;
                *this->access++=1.0f;
                *this->access++=end.x;
                *this->access++=end.y;
                *this->access++=end.z;
                *this->access++=1.0f;

                return(true);
            }

            /**
            * 写入3D三角形
            */
            template<typename V4>
            bool WriteTriangle(const V4 &v1,const V4 &v2,const V4 &v3)
            {
                if(!this->access||this->access+12>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess4::WriteTriangle(vec3,vec3,vec3) out"));
                    return(false);
                }

                *this->access++=v1.x;
                *this->access++=v1.y;
                *this->access++=v1.z;
                *this->access++=1.0f;

                *this->access++=v2.x;
                *this->access++=v2.y;
                *this->access++=v2.z;
                *this->access++=1.0f;

                *this->access++=v3.x;
                *this->access++=v3.y;
                *this->access++=v3.z;
                *this->access++=1.0f;

                return(true);
            }

            /**
            * 写入3D三角形
            */
            template<typename V4>
            bool WriteTriangle(const V4 *v)
            {
                if(!this->access||this->access+12>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess4::WriteTriangle(vec3 *) out"));
                    return(false);
                }

                *this->access++=v->x;
                *this->access++=v->y;
                *this->access++=v->z;
                *this->access++=1.0f;
                ++v;

                *this->access++=v->x;
                *this->access++=v->y;
                *this->access++=v->z;
                *this->access++=1.0f;
                ++v;

                *this->access++=v->x;
                *this->access++=v->y;
                *this->access++=v->z;
                *this->access++=1.0f;

                return(true);
            }

            /**
            * 写入2D矩形,注:这个函数会依次写入Left,Top,Width,Height四个值
            */
            template<typename V>
            bool WriteRectangle2D(const RectScope2<V> &rect)
            {
                if(!this->access||this->access+4>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess4::WriteRectangle2D(RectScope2 ) out"));
                    return(false);
                }

                *this->access++=rect.Left;
                *this->access++=rect.Top;
                *this->access++=rect.Width;
                *this->access++=rect.Height;

                return(true);
            }

            /**
            * 写入2D矩形,注:这个函数会依次写入Left,Top,Width,Height四个值
            */
            template<typename V>
            bool WriteRectangle2D(const RectScope2<V> *rect,const uint32_t count)
            {
                if(!this->access||this->access+(4*count)>this->data_end)
                {
                    GLogHint(OS_TEXT("VertexAttribDataAccess4::WriteRectangle2D(RectScope2 *,count) out"));
                    return(false);
                }

                for(uint32_t i=0;i<count;i++)
                {
                    *this->access++=rect->Left;
                    *this->access++=rect->Top;
                    *this->access++=rect->Width;
                    *this->access++=rect->Height;

                    ++rect;
                }

                return(true);
            }
        };//class VertexAttribDataAccess4

        //缓冲区具体数据类型定义
        typedef VertexAttribDataAccess1<int8  ,PF_R8I    >   VB1i8   ,VB1b;
        typedef VertexAttribDataAccess1<int16 ,PF_R16I   >   VB1i16  ,VB1s;
        typedef VertexAttribDataAccess1<int32 ,PF_R32I   >   VB1i32  ,VB1i;
        typedef VertexAttribDataAccess1<uint8 ,PF_R8U    >   VB1u8   ,VB1ub;    //输入0-255,使用也为0-255
        typedef VertexAttribDataAccess1<uint16,PF_R16U   >   VB1u16  ,VB1us;
        typedef VertexAttribDataAccess1<uint32,PF_R32U   >   VB1u32  ,VB1ui;

        typedef VertexAttribDataAccess1<float ,PF_R32F   >   VB1f;
        typedef VertexAttribDataAccess1<double,PF_R64F   >   VB1d;

        typedef VertexAttribDataAccess1<int8  ,PF_R8SN   >   VB1sf8;            //输入-128 to 127,但使用为-1 to +1
        typedef VertexAttribDataAccess1<int16 ,PF_R16SN  >   VB1sf16;           //输入-32768 to 32767,但使用为-1 to +1
        typedef VertexAttribDataAccess1<uint8 ,PF_R8UN   >   VB1uf8;            //输入0-255,但使用为0-1
        typedef VertexAttribDataAccess1<uint16,PF_R16UN  >   VB1uf16;           //输入0-65535,但使用为0-1
        typedef VertexAttribDataAccess1<half_float,PF_R16F   >   VB1hf;             //half float
        
        typedef VertexAttribDataAccess2<int8  ,PF_RG8I   >   VB2i8   ,VB2b;
        typedef VertexAttribDataAccess2<int16 ,PF_RG16I  >   VB2i16  ,VB2s;
        typedef VertexAttribDataAccess2<int32 ,PF_RG32I  >   VB2i32  ,VB2i;

        typedef VertexAttribDataAccess2<uint8 ,PF_RG8U   >   VB2u8   ,VB2ub;
        typedef VertexAttribDataAccess2<uint16,PF_RG16U  >   VB2u16  ,VB2us;
        typedef VertexAttribDataAccess2<uint32,PF_RG32U  >   VB2u32  ,VB2ui;

        typedef VertexAttribDataAccess2<float ,PF_RG32F  >   VB2f;
        typedef VertexAttribDataAccess2<double,PF_RG64F  >   VB2d;

        typedef VertexAttribDataAccess1<int8  ,PF_RG8SN  >   VB2sf8;            //输入-128 to 127,但使用为-1 to +1
        typedef VertexAttribDataAccess1<int16 ,PF_RG16SN >   VB2sf16;           //输入-32768 to 32767,但使用为-1 to +1
        typedef VertexAttribDataAccess2<uint8 ,PF_RG8UN  >   VB2uf8;            //输入0-255,但使用为0-1
        typedef VertexAttribDataAccess2<uint16,PF_RG16UN >   VB2uf16;           //输入0-65535,但使用为0-1
        typedef VertexAttribDataAccess2<half_float,PF_RG16F  >   VB2hf;             //half float

//        typedef VertexAttribDataAccess3<int8  ,PF_RGB8I  >   VB3i8   ,VB3b;
//        typedef VertexAttribDataAccess3<int16 ,PF_RGB16I >   VB3i16  ,VB3s;
        typedef VertexAttribDataAccess3<int32 ,PF_RGB32I >   VB3i32  ,VB3i;
//        typedef VertexAttribDataAccess3<uint8 ,PF_RGB8U  >   VB3u8   ,VB3ub;
//        typedef VertexAttribDataAccess3<uint16,PF_RGB16U >   VB3u16  ,VB3us;
        typedef VertexAttribDataAccess3<uint32,PF_RGB32U >   VB3u32  ,VB3ui;

        typedef VertexAttribDataAccess3<float ,PF_RGB32F >   VB3f;
        typedef VertexAttribDataAccess3<double,PF_RGB64F >   VB3d;

        typedef VertexAttribDataAccess4<int8  ,PF_RGBA8I >   VB4i8   ,VB4b;
        typedef VertexAttribDataAccess4<int16 ,PF_RGBA16I>   VB4i16  ,VB4s;
        typedef VertexAttribDataAccess4<int32 ,PF_RGBA32I>   VB4i32  ,VB4i;

        typedef VertexAttribDataAccess4<uint8 ,PF_RGBA8U >   VB4u8   ,VB4ub;
        typedef VertexAttribDataAccess4<uint8 ,PF_RGBA8UN>   VB4uf;
        typedef VertexAttribDataAccess4<uint16,PF_RGBA16U>   VB4u16  ,VB4us;
        typedef VertexAttribDataAccess4<uint32,PF_RGBA32U>   VB4u32  ,VB4ui;

        typedef VertexAttribDataAccess4<float ,PF_RGBA32F>   VB4f;
        typedef VertexAttribDataAccess4<double,PF_RGBA64F>   VB4d;

        typedef VertexAttribDataAccess4<int8  ,PF_RGBA8SN >  VB4sf8;            //输入-128 to 127,但使用为-1 to +1
        typedef VertexAttribDataAccess4<int16 ,PF_RGBA16SN>  VB4sf16;           //输入-32768 to 32767,但使用为-1 to +1
        typedef VertexAttribDataAccess4<uint8 ,PF_RGBA8UN >  VB4uf8;            //输入0-255,但使用为0-1
        typedef VertexAttribDataAccess4<uint16,PF_RGBA16UN>  VB4uf16;           //输入0-65535,但使用为0-1
        typedef VertexAttribDataAccess4<half_float,PF_RGBA16F >  VB4hf;             //half float
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VERTEX_ATTRIB_DATA_ACCESS_INCLUDE
