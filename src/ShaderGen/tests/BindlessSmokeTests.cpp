// BindlessSmokeTests.cpp - C7 smoke tests for BuiltinBindlessSamplerArrayCodegen.
#include "../ColorSource/BuiltinSamplerCodegen.h"
#include <hgl/shadergen/ColorSourcePipeline.h>
#include <hgl/shadergen/ColorSource.h>
#include <hgl/shadergen/ShaderWriter.h>
#include <hgl/mtl/SamplerSlot.h>
#include <hgl/shadergen/DescriptorRequirement.h>
#include <cstdio>
#include <string>
#include <vector>
static int g_failures = 0;
static void check_true(bool c,const char*e,const char*f,int l){if(!c){std::fprintf(stderr,"FAIL [%s:%d] CHECK(%s)\n",f,l,e);++g_failures;}}
static void check_contains(const std::string&h,const char*n,const char*f,int l){if(h.find(n)==std::string::npos){std::fprintf(stderr,"FAIL [%s:%d] expected \"%s\" in:\n%s\n",f,l,n,h.c_str());++g_failures;}}
#define CHECK(e) check_true((e),#e,__FILE__,__LINE__)
#define CHECK_CONTAINS(s,n) check_contains((s),(n),__FILE__,__LINE__)
static hgl::graph::ColorSource MakeBindlessSource(hgl::graph::mtl::SamplerSlot slot=hgl::graph::mtl::SamplerSlot::BaseColor){hgl::graph::ColorSource cs;cs.kind=hgl::graph::ColorSourceKind::BuiltinBindlessSamplerArray;cs.slot=slot;cs.builtin.output_format=hgl::graph::ColorSourceOutputFormat::RGBA;return cs;}
static void Test1(){using namespace hgl::graph;BuiltinBindlessSamplerArrayCodegen c;auto src=MakeBindlessSource();ResolvedBindings r;ResolvedBinding rb;rb.debug_name="Sampler_BaseColor_Bindless";rb.set=3;rb.binding=0;r.push_back(rb);std::string o;ShaderWriter w(o);c.EmitDeclarations(w,src,r);CHECK_CONTAINS(o,"GL_EXT_nonuniform_qualifier");CHECK_CONTAINS(o,"Sampler_BaseColor_Bindless[]");CHECK_CONTAINS(o,"set=3");CHECK_CONTAINS(o,"binding=0");}
static void Test2(){using namespace hgl::graph;BuiltinBindlessSamplerArrayCodegen c;auto src=MakeBindlessSource();ResolvedBindings r;std::string o;ShaderWriter w(o);c.EmitGetterFunction(w,src,r);CHECK_CONTAINS(o,"nonuniformEXT");CHECK_CONTAINS(o,"GetSamplerBaseColor");CHECK_CONTAINS(o,"Sampler_BaseColor_Bindless");}
static void Test3(){using namespace hgl::graph;std::vector<ColorSource> s;s.push_back(MakeBindlessSource());auto res=FinalizeColorSources(s,"SmokeTest",false);CHECK(res.ok);bool hw=false;for(const auto&d:res.diags)if(d.level==ColorSourcePipelineResult::Diag::Level::Warning&&d.message.find("fallback")!=std::string::npos)hw=true;CHECK(hw);}
static void Test4(){using namespace hgl::graph;std::vector<ColorSource> s;s.push_back(MakeBindlessSource());auto res=FinalizeColorSources(s,"SmokeTest",true);CHECK(res.ok);for(const auto&d:res.diags)if(d.level==ColorSourcePipelineResult::Diag::Level::Warning&&d.message.find("fallback")!=std::string::npos)check_true(false,"unexpected fallback warning",__FILE__,__LINE__);bool fb=false;for(const auto&rb:res.bindings)if(rb.debug_name.find("Bindless")!=std::string::npos)fb=true;CHECK(fb);}
int main(){Test1();Test2();Test3();Test4();if(g_failures==0)std::fprintf(stdout,"BindlessSmokeTests: all tests passed.\n");else std::fprintf(stdout,"BindlessSmokeTests: %d test(s) FAILED.\n",g_failures);return g_failures>0?1:0;}
