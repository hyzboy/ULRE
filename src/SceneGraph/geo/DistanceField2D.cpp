// The MIT License
// Copyright © 2020 Inigo Quilez
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

// origin webpage: https://www.iquilezles.org/www/articles/distfunctions2d/distfunctions2d.htm

#include<hgl/math/Math.h>
#include<hgl/math/bvec.h>

namespace hgl
{
    namespace graph
    {
        float sdCircle( const Vector2f &p, const float r )
        {
            return Length(p) - r;
        }

        /**
         * 判断当前点是否在多边形内部
         */
        float sdPoly( const Vector2f &p,const Vector2f *v,const uint num)
        {
            float d = Dot(p-v[0],p-v[0]);
            float s = 1.0;
            Vector2f e,w,b;
            bvec3 cond;

            for( uint i=0, j=num-1; i<num; j=i, i++ )
            {
                // distance
                e = v[j] - v[i];
                w =    p - v[i];
                b = w - e*clamp( Dot(w,e)/Dot(e,e), 0.0f, 1.0f );
                d = hgl_min( d, Dot(b,b) );

                // winding number from http://geomalgorithms.com/a03-_inclusion.html
                cond = bvec3(p.y>=v[i].y,p.y<v[j].y,e.x*w.y>e.y*w.x);

                if( cond.all() || cond.not().all() ) s*=-1.0;
            }

            return s*sqrt(d);
        }
    }//namespace graph
}//namespace hgl
