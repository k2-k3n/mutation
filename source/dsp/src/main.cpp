// Mutation DSP — input-derived mono tone breeding. Public Resona SDK, MIT.
#include <resona/module.h>
#include <resona/math.h>
#include <resona/parameters.h>
namespace {
constexpr uint32_t N=32768;
const resona_param params[]={
 {"BLEND",0,100,72,"%"},{"A_DRIVE",0,100,10,"%"},{"A_TONE",0,100,76,"%"},{"A_MOD",0,100,5,"%"},{"A_ECHO",0,100,7,"%"},
 {"B_DRIVE",0,100,75,"%"},{"B_TONE",0,100,32,"%"},{"B_MOD",0,100,70,"%"},{"B_ECHO",0,100,62,"%"},
 {"BYPASS",0,1,0,""},{"LEVEL",0,100,84,"%"}};
const resona_buffer_request request[]={{1,RESONA_MEMORY_PSRAM,N*sizeof(float),128}};
const resona_memory_profile memory={RESONA_MEMORY_VERSION,sizeof(resona_memory_profile),1,request};
struct State {float p[11],control[11],smooth[8],target[8],lp,echoLp,phase,dcIn,dcOut;float *line;uint32_t write;resona_parameter_inbox pending;};
float clip(float x){return x/(1.0f+resona_fabsf(x));}
void targets(State&s){float b=s.p[0]*.01f,v[4];for(uint32_t j=0;j<4;++j)v[j]=(s.p[1+j]*(1-b)+s.p[5+j]*b)*.01f;
 s.target[0]=1+v[0]*v[0]*24;s.target[1]=.006f+v[1]*v[1]*.64f;s.target[2]=v[2];s.target[3]=v[3];s.target[4]=s.p[9];s.target[5]=s.p[10]*.01f;
 // Loudness make-up, so every proposal lands near the dry level (fitted on a guitar DI):
 // soft clip x*d/(1+|x*d|) at guitar level ~ (1+.12d)/d, the one-pole tone loss at ~220 Hz,
 // and the chorus/echo mix. Applied before the tone filter so the echo feed is level-matched too.
 const float d=s.target[0],fc=s.target[1]*7639.4f;
 s.target[6]=(1+.12f*d)/d*resona_sqrtf(fc*fc+48400.0f)/fc;s.target[7]=1.2f*resona_db_to_lin(1.6f+3.9f*v[2]-(1.2f+v[2])*v[3]*v[3]*v[3]);}
void init(void*p,const resona_host*h){auto&s=*static_cast<State*>(p);const resona_buffer*b=resona_buffer_find(h->memory,1);s.line=b&&b->bytes>=N*sizeof(float)?static_cast<float*>(b->data):nullptr;
 for(uint32_t i=0;i<11;++i)s.p[i]=s.control[i]=params[i].def;targets(s);for(uint32_t i=0;i<8;++i)s.smooth[i]=s.target[i];}
void reset(void*p){auto&s=*static_cast<State*>(p);s.write=0;s.lp=s.echoLp=s.phase=s.dcIn=s.dcOut=0;for(uint32_t i=0;i<N;++i)if(s.line)s.line[i]=0;targets(s);for(uint32_t i=0;i<8;++i)s.smooth[i]=s.target[i];s.pending={};for(uint32_t i=0;i<11;++i)s.p[i]=s.control[i];targets(s);for(uint32_t i=0;i<8;++i)s.smooth[i]=s.target[i];}
void set(void*p,uint32_t n,float v){auto&s=*static_cast<State*>(p);if(n<11){s.control[n]=v;resona_parameter_publish(&s.pending,n,v);}}
float get(const void*p,uint32_t n){const auto&s=*static_cast<const State*>(p);return n<11?s.control[n]:0;}
float tap(State&s,float frames){float pos=static_cast<float>(s.write)+N-frames;if(pos>=N)pos-=N;uint32_t i=static_cast<uint32_t>(pos);float frac=pos-i;return s.line[i]*(1-frac)+s.line[(i+1)&(N-1)]*frac;}
void process(void*p,const float*const*in,float*const*out,uint32_t frames){auto&s=*static_cast<State*>(p);float values[12];uint32_t mask=resona_parameter_consume(&s.pending,values);if(mask){for(uint32_t i=0;i<11;++i)if(mask&(1u<<i))s.p[i]=values[i];targets(s);}
 for(uint32_t i=0;i<frames;++i){const float dry=in[0][i];for(uint32_t j=0;j<8;++j)s.smooth[j]+=.0009f*(s.target[j]-s.smooth[j]);
 const float drive=s.smooth[0],tone=s.smooth[1],mod=s.smooth[2],echo=s.smooth[3];
 float driven=clip(dry*drive)*s.smooth[6];s.lp+=tone*(driven-s.lp);if(resona_fabsf(s.lp)<1e-20f)s.lp=0;
 s.phase+=(.45f+mod*4.8f)/48000.0f;if(s.phase>=1)s.phase-=1;const float triangle=1-4*resona_fabsf(s.phase-.5f);
 float wet=s.lp*(1-mod*.25f+triangle*mod*.16f);
 if(s.line){const float chorus=tap(s,410.0f+triangle*mod*100.0f);const float delayed=tap(s,4200.0f+echo*10800.0f);
 s.echoLp+=.13f*(delayed-s.echoLp);if(resona_fabsf(s.echoLp)<1e-20f)s.echoLp=0;
 s.line[s.write]=clip(s.lp+s.echoLp*(.1f+echo*.42f));s.write=(s.write+1)&(N-1);
 wet=wet*(1-mod*.28f)+chorus*mod*.28f+s.echoLp*echo*.65f;}
 float z=wet-s.dcIn+.997f*s.dcOut;s.dcIn=wet;s.dcOut=resona_fabsf(z)<1e-20f?0:z;
 wet=clip(z*s.smooth[7])*s.smooth[5];out[0][i]=wet*(1-s.smooth[4])+dry*s.smooth[4];}}
void workload(void*p,uint32_t phase){set(p,0,phase==RESONA_CPU_TRIAL_TRANSITION?35:100);for(uint32_t i=1;i<9;++i)set(p,i,100);set(p,9,0);set(p,10,90);}
const resona_cpu_profile cpu={RESONA_CPU_PROFILE_VERSION,sizeof(resona_cpu_profile),0,workload};
const resona_module module={RESONA_MODULE_ABI,sizeof(resona_module),"org.resona.demo.mutation.dsp","Mutation DSP","0.1.1",RESONA_LAYOUT_MONO,RESONA_RATE_48000,sizeof(State),11,params,nullptr,init,reset,set,process,nullptr,nullptr,get,&cpu,&memory};
}
extern "C" RESONA_EXPORT const resona_module*resona_module_get(){return &module;}
