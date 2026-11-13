#include <cmath>
#include <cstdio>
#include <vector>
#include <cassert>
#include "../dsp/src/main.cpp"
int main(){
 std::vector<float>buffer(N),x(96000),a(96000),b(96000),samebuf(96000);
 for(uint32_t i=0;i<x.size();++i){float t=(float)i/48000;float q=std::fmod(t,.33f);x[i]=.12f*std::sin(6.2831853f*196*t)*std::exp(-q*11)+.045f*std::sin(6.2831853f*392*t)*std::exp(-q*18);}
 resona_buffer buf={1,RESONA_MEMORY_PSRAM,N*sizeof(float),buffer.data()};resona_memory_context mem={RESONA_MEMORY_VERSION,sizeof(resona_memory_context),1,&buf};resona_host host={};host.memory=&mem;
 State s={};init(&s,&host);
 auto run=[&](int blend,std::vector<float>&out,bool inplace){set(&s,0,(float)blend);reset(&s);out=x;for(uint32_t i=0;i<x.size();i+=64){const float*in[]={inplace?out.data()+i:x.data()+i};float*o[]={out.data()+i};process(&s,in,o,64);}};
 run(0,a,false);run(100,b,false);run(100,samebuf,true);
 double diff=0,energy=0,ip=0;float peak=0;
 for(uint32_t i=0;i<x.size();++i){assert(std::isfinite(a[i])&&std::isfinite(b[i]));diff+=(a[i]-b[i])*(a[i]-b[i]);energy+=b[i]*b[i];ip+=std::fabs(b[i]-samebuf[i]);peak=std::fmax(peak,std::fabs(b[i]));}
 assert(diff/energy>.02);assert(peak<1);assert(ip==0);
 std::printf("{\"endpoint_relative_difference\":%.8f,\"peak\":%.8f,\"in_place_absolute_error\":%.8f,\"finite\":true}\n",std::sqrt(diff/energy),peak,ip);
}
