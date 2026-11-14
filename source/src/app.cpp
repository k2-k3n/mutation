// Mutation: a native mono-guitar tone discovery app. MIT, public SDK only.
// The Web, Mobile and MIDI controllers (controller/, CCs on channel 1, see README.md) reach what the knobs reach,
// plus the mutant's four genes, LEVEL and four hold pads: A (hear endpoint A), B (hear B), SMEAR, DRY.
#include <resona/platform.h>
#include <resona/startup_helpers.h>
#include "splash_data.h"
#include "demo_capture.h"
#include <resona/ui.h>
#include <resona/math.h>
#include "art.h"
#include "art_words.h"
#include "own_audio.h"
#include "trip_guard.h"
#include "controller_link.h"
namespace {
const resona_platform*P;
struct Tone{int v[4];};
struct Saved{uint32_t magic,version,count,seed;Tone tone[8];};
// The session (A, B, blend, strain, seed) lives after the favourites so older stores stay valid.
struct SessionV1{uint32_t magic,size;Tone a,b;int32_t blend,strain;uint32_t seed;};
struct Session{uint32_t magic,size;Tone a,b;int32_t blend,strain;uint32_t seed;int32_t level;};
constexpr uint32_t kSessionAt=160,kSessionMagicV1=0x4d555332,kSessionMagic=0x4d555333;
// The controllers' MIDI map: CC on channel 1 (controller/shared/profile.js says the same).
enum{CC_BLEND=20,CC_DRY,CC_FAV,CC_DRIVE,CC_TONE,CC_MOD,CC_ECHO,CC_LEVEL,CC_STRAIN,CC_KEPT,CC_PARENT=30,CC_MUTANT,CC_SMEAR,CC_HOLD_DRY,CC_NEW,CC_KEEP,CC_BACK};
struct State{Tone a,b;Saved saved;int blend,strain,compare,ready,error,stage,scan,count,down[2],held[2],cursor;uint32_t now,nextSetup,keepUntil,nextDraw;float activity,budPhase;uint32_t changed,saveFailUntil;int sessionDirty;trip_guard::Guard trip;
 int level,pick,kept,parent,mutant,smear,dry;ctl::Link link;}; // pick/kept mirror the cursor and favourite count for the controllers
int clamp(int x,int lo,int hi){return x<lo?lo:(x>hi?hi:x);}
own_audio::Graph g_audio; // Guitar -> Mutation DSP -> Output, owned by the App
void checked(State*s,int result){if(result<0)s->error=result;}
void touched(State*s){s->changed=s->now;s->sessionDirty=1;}
uint32_t random(State*s){uint32_t x=s->saved.seed;x^=x<<13;x^=x>>17;x^=x<<5;s->saved.seed=x?x:0x6d2b79f5;return x;}
constexpr int kBinds=17;ctl::Bind kBind[kBinds];
void bind(State*s){const ctl::Bind b[kBinds]={{CC_BLEND,ctl::RANGE,0,100,&s->blend},{CC_DRY,ctl::STEPS,0,1,&s->compare},{CC_FAV,ctl::STEPS,0,8,&s->pick},
 {CC_DRIVE,ctl::RANGE,0,100,&s->b.v[0]},{CC_TONE,ctl::RANGE,0,100,&s->b.v[1]},{CC_MOD,ctl::RANGE,0,100,&s->b.v[2]},{CC_ECHO,ctl::RANGE,0,100,&s->b.v[3]},
 {CC_LEVEL,ctl::RANGE,0,100,&s->level},{CC_STRAIN,ctl::READ_RANGE,0,99,&s->strain},{CC_KEPT,ctl::READ_RANGE,0,8,&s->kept},
 {CC_PARENT,ctl::TRIGGER,0,1,nullptr},{CC_MUTANT,ctl::TRIGGER,0,1,nullptr},{CC_SMEAR,ctl::TRIGGER,0,1,nullptr},{CC_HOLD_DRY,ctl::TRIGGER,0,1,nullptr},
 {CC_NEW,ctl::TRIGGER,0,1,nullptr},{CC_KEEP,ctl::TRIGGER,0,1,nullptr},{CC_BACK,ctl::TRIGGER,0,1,nullptr}};for(int i=0;i<kBinds;++i)kBind[i]=b[i];}
// The pads override what is pushed while held: A wins over B; SMEAR floods both endpoints' chorus and echo.
int blendNow(const State*s){return s->parent?0:(s->mutant?100:s->blend);}
int compareNow(const State*s){return s->compare||s->dry;}
Tone current(State*s){Tone t;for(int i=0;i<4;++i)t.v[i]=(s->a.v[i]*(100-s->blend)+s->b.v[i]*s->blend+50)/100;return t;}
void params(State*s){if(!s->ready)return;checked(s,P->effect_param_set(g_audio.h[0],0,(float)blendNow(s)));for(uint32_t i=0;i<4;++i){const int lift=s->smear&&i>=2?(i==2?90:95):0;checked(s,P->effect_param_set(g_audio.h[0],1+i,(float)(s->a.v[i]>lift?s->a.v[i]:lift)));checked(s,P->effect_param_set(g_audio.h[0],5+i,(float)(s->b.v[i]>lift?s->b.v[i]:lift)));}checked(s,P->effect_param_set(g_audio.h[0],9,compareNow(s)?1:0));checked(s,P->effect_param_set(g_audio.h[0],10,(float)s->level));}
void propose(State*s){s->a=current(s);for(int i=0;i<4;++i)s->b.v[i]=12+(int)(random(s)%85);s->blend=0;s->strain=(s->strain+1)%100;s->cursor=-1;s->compare=0;params(s);}
void keep(State*s){Tone t=current(s);const Saved before=s->saved;uint32_t slot=s->saved.count<8?s->saved.count:7;
 if(s->saved.count>=8){for(int i=0;i<7;++i)s->saved.tone[i]=s->saved.tone[i+1];}else ++s->saved.count;
 s->saved.tone[slot]=t;s->saved.magic=0x4d555431;s->saved.version=1;
 int rc=P->app_store_write(0,&s->saved,sizeof s->saved);if(rc>=0){s->a=s->b=t;s->blend=100;s->cursor=slot;s->keepUntil=s->now+1500;touched(s);}else{s->saved=before;s->saveFailUntil=s->now+1500;}params(s);}
void input(State*s,uint32_t kind,uint32_t which,int delta){
 if(which==RESONA_INPUT_SELECTOR){if(kind==RESONA_EVENT_TURN){if(s->saved.count){s->cursor=(s->cursor+delta%(int)s->saved.count+(int)s->saved.count)%(int)s->saved.count;s->a=current(s);s->b=s->saved.tone[s->cursor];s->blend=100;s->compare=0;}}else if(kind==RESONA_EVENT_CLICK)s->compare=!s->compare;else if(kind==RESONA_EVENT_LONG){s->compare=0;s->blend=0;}}
 else{if(kind==RESONA_EVENT_TURN){s->blend=clamp(s->blend+delta*3,0,100);s->compare=0;}else if(kind==RESONA_EVENT_CLICK){propose(s);s->held[1]=0;}else if(kind==RESONA_EVENT_LONG){s->held[1]=1;keep(s);}}params(s);touched(s);}
// A controller changed a value or pressed a pad: do what the knobs do for that setting.
void control(State*s,const ctl::Msg&m){
 if(m.cc>=CC_PARENT&&m.cc<=CC_HOLD_DRY){(m.cc==CC_PARENT?s->parent:m.cc==CC_MUTANT?s->mutant:m.cc==CC_SMEAR?s->smear:s->dry)=m.down;params(s);return;}
 if(m.cc>=CC_NEW){if(m.down)input(s,m.cc==CC_NEW?RESONA_EVENT_CLICK:RESONA_EVENT_LONG,m.cc==CC_BACK?RESONA_INPUT_SELECTOR:RESONA_INPUT_CUSTOMIZE,1);return;}
 if(m.cc==CC_FAV){if(s->pick<1||s->pick>(int)s->saved.count){s->pick=s->cursor+1;return;}s->cursor=s->pick-1;s->a=current(s);s->b=s->saved.tone[s->cursor];s->blend=100;s->compare=0;}
 else if(m.cc==CC_BLEND)s->compare=0;
 else if(m.cc>=CC_DRIVE&&m.cc<=CC_ECHO)s->cursor=-1;   // an edited mutant is no longer the favourite
 params(s);touched(s);}
void key(State*s,uint32_t which,int down){s->down[which]=down;if(down)s->held[which]=0;}
// The OS bypassed the DSP (CPU time guard or fault): drop the graph and rebuild it after a growing wait.
void trip(State*s){s->ready=0;s->stage=1;s->error=RESONA_ERR_BUDGET;s->nextSetup=s->now+trip_guard::tripped(s->trip,s->now);own_audio::stop(P,g_audio);}
void setup(State*s){
 if(s->now<s->nextSetup)return;s->nextSetup=s->now+1000;
 if(s->ready&&trip_guard::faulted(P,g_audio.h[0])){trip(s);return;}
 int rc=own_audio::start(P,g_audio,"org.resona.demo.mutation.dsp");if(rc<0){s->ready=0;s->error=rc;return;} // dsp/resona.project.json
 resona_effect_info fi={};rc=P->effect_info(g_audio.h[0],&fi);
 if(rc!=RESONA_OK||fi.param_count!=11){s->ready=0;s->error=rc<0?rc:RESONA_ERR_STATE;return;}
 if(!s->ready){rc=P->effect_enable(g_audio.h[0],1);if(rc<0){s->error=rc;return;}s->error=0;s->ready=1;params(s);}
 else if(s->error){s->error=0;params(s);} // audio is up: drop a stale error and retry the writes it interrupted
}
uint16_t blend(uint16_t a,uint16_t b,int t){int r=(((a>>11)&31)*(255-t)+((b>>11)&31)*t)/255,g=(((a>>5)&63)*(255-t)+((b>>5)&63)*t)/255,bl=((a&31)*(255-t)+(b&31)*t)/255;return(uint16_t)((r<<11)|(g<<5)|bl);}
void glyph(uint16_t*px,uint32_t stride,int x,int y,const uint8_t*mask,int w,int h,uint16_t ink){for(int j=0;j<h;++j)for(int i=0;i<w;++i){int xx=x+i,yy=y+j;if(xx>=0&&xx<320&&yy>=0&&yy<172){int a=mask[j*w+i];if(a)px[yy*stride+xx]=blend(px[yy*stride+xx],ink,a);}}}
void label(uint16_t*px,uint32_t stride,int x,int y,const char*t,uint16_t ink){for(int n=0;n<32&&t[n];++n){int k=0;for(int i=0;small_chars[i];++i)if(small_chars[i]==t[n]){k=i;break;}glyph(px,stride,x+n*7,y,small_alpha+k*135,9,15,ink);}}
void number(uint16_t*px,uint32_t stride,int x,int y,int n,uint16_t ink){n=clamp(n,0,100);if(n==100){glyph(px,stride,x,y,digit_alpha+1288,28,46,ink);x+=19;n=0;}glyph(px,stride,x,y,digit_alpha+(n/10)*1288,28,46,ink);glyph(px,stride,x+24,y,digit_alpha+(n%10)*1288,28,46,ink);}
void patch(uint16_t*px,uint32_t stride,int x,int y,int w,int h,int edge){for(int j=y;j<y+h&&j<172;++j){uint16_t c=art_pixel(j*320+edge);for(int i=x;i<x+w&&i<320;++i)px[j*stride+i]=c;}}
void draw(State*s){uint16_t*px=nullptr;uint32_t stride=0;resona_ui*ui=P->screen_begin(&px,&stride);if(!ui||!px)return;art_copy(px,stride);
 // Bend the central bud around its stem. Leave the title and side leaves outside the warp.
 const float sway=8.0f*s->activity*resona_sinf(s->budPhase);
 if(sway>.25f||sway<-.25f){
  for(int y=28;y<115;++y){
   const int left=y<37?145:117;
   const int right=y<37?170:207;
   const float offset=(115-y)*sway/87.0f;
   for(int x=left;x<right;++x){
    const int edge=x-left<right-1-x?x-left:right-1-x;
    const float weight=edge<8?edge*.125f:1.0f;
    const int shift=(int)(offset*weight+(offset<0?-.5f:.5f));
    px[y*stride+x]=art_pixel(y*320+x+shift);
   }
  }
 }
 const uint16_t ink=RESONA_RGB(35,18,43),cream=RESONA_RGB(249,242,196);
 patch(px,stride,4,126,67,46,0);number(px,stride,7,126,blendNow(s),ink);
 patch(px,stride,253,0,67,40,319); // strain displayed smaller than blend, leaving the title intact
 char text[12];P->format(text,sizeof text,"%02d",s->cursor>=0?s->cursor+1:s->strain);number(px,stride,267,0,s->cursor>=0?s->cursor+1:s->strain,ink);
 if(compareNow(s)){patch(px,stride,1,116,68,13,0);label(px,stride,7,116,"DRY",ink);}
 if(s->saveFailUntil>s->now){patch(px,stride,238,136,82,31,319);label(px,stride,247,140,"NOT",ink);label(px,stride,247,152,"SAVED",ink);}
 if(s->keepUntil>s->now){patch(px,stride,238,136,82,31,319);label(px,stride,247,140,"KEPT",ink);P->format(text,sizeof text,"%d / 8",s->saved.count);label(px,stride,247,152,text,ink);}
 // Readable physical-encoder hints are part of the screen, never touch targets.
 resona_ui_dots(ui,161,96,30,13,65,230,18,(uint32_t)(blendNow(s)*18/100),2,cream);
 if(s->error){ui->rect(ui->ctx,0,164,320,8,RESONA_RGB(201,169,215));char status[48];if(s->error==RESONA_ERR_PACK_ID)P->format(status,sizeof status,"INSTALL MUTATION DSP IN LAB");else if(s->error==RESONA_ERR_BUDGET)P->format(status,sizeof status,"CPU LIMIT");else P->format(status,sizeof status,"DSP STATUS %d",s->error);ui->text(ui->ctx,6,165,1,ink,status);}
 demo_capture::remember(px,stride);P->screen_present(0,0,0,0);}
int init(void*state,const resona_platform*platform){P=platform;
    const int startup = resona_startup_splash_apply(P, kSplash, sizeof kSplash);
    if (startup < 0 && startup != RESONA_ERR_UNSUPPORTED)
        P->log("Startup animation could not be saved");
    State*s=static_cast<State*>(state);s->a={{10,76,5,7}};s->b={{75,32,70,62}};s->blend=72;s->strain=4;s->cursor=-1;s->saved.seed=0x61cba913;s->level=84;
 Saved saved={};int rc=P->app_store_read(0,&saved,sizeof saved);bool valid=rc==RESONA_OK&&saved.magic==0x4d555431&&saved.version==1&&saved.count<=8&&saved.seed;
 for(uint32_t i=0;i<8;++i)for(uint32_t j=0;j<4;++j)if(saved.tone[i].v[j]<0||saved.tone[i].v[j]>100)valid=false;
 if(valid){s->saved=saved;}
 Session ses={};bool ok=P->app_store_read(kSessionAt,&ses,sizeof ses)==RESONA_OK&&ses.magic==kSessionMagic&&ses.size==sizeof ses;
 if(!ok){SessionV1 v1={};ok=P->app_store_read(kSessionAt,&v1,sizeof v1)==RESONA_OK&&v1.magic==kSessionMagicV1&&v1.size==sizeof v1;if(ok){ses={kSessionMagic,sizeof ses,v1.a,v1.b,v1.blend,v1.strain,v1.seed,84};}} // 0.1 session: LEVEL was fixed at 84
 ok=ok&&ses.seed&&ses.blend>=0&&ses.blend<=100&&ses.strain>=0&&ses.strain<100;
 for(uint32_t j=0;j<4;++j)if(ses.a.v[j]<0||ses.a.v[j]>100||ses.b.v[j]<0||ses.b.v[j]>100)ok=false;
 if(ok){s->a=ses.a;s->b=ses.b;s->blend=ses.blend;s->strain=ses.strain;s->saved.seed=ses.seed;s->level=clamp(ses.level,0,100);}bind(s);P->input_config(0,0);P->input_config(1,0);P->log("Mutation: mono tone breeding, App-owned audio");return 0;}
void event(void*state,const resona_app_event*ev){ if(demo_capture::event(P,ev))return;State*s=static_cast<State*>(state);
 ctl::Msg m;if(ctl::receive(P,s->link,kBind,kBinds,ev,m)){if(m.changed)control(s,m);return;}
 if(ev->kind==RESONA_APP_EV_INPUT)input(s,ev->u.input.kind,ev->u.input.input,ev->u.input.value);
 else if(ev->kind==RESONA_APP_EV_KEY&&ev->u.key.key<2)key(s,ev->u.key.key,ev->u.key.down!=0);
#ifdef RESONA_DEMO_BENCH
 // Bench-only: MIDI channel 16 CC20..27 emulate the encoders and keys.
 else if(ev->kind==RESONA_APP_EV_MIDI&&ev->u.midi.len>=3){const uint8_t*m=ev->u.midi.data;if(m[0]!=0xbf)return;if(m[1]==20||m[1]==21)input(s,RESONA_EVENT_TURN,m[1]-20,(int)m[2]-64);else if(m[1]==22||m[1]==23)key(s,m[1]-22,m[2]>=64);else if(m[1]>=24&&m[1]<=27&&m[2])input(s,m[1]>=26?RESONA_EVENT_LONG:RESONA_EVENT_CLICK,(m[1]-24)%2,1);}
#endif
 else if(ev->kind==RESONA_APP_EV_AUDIO){if(ev->u.audio.tripped_role>=0||(s->ready&&trip_guard::faulted(P,g_audio.h[0])))trip(s);}
 else if(ev->kind==RESONA_APP_EV_CHANGED&&ev->u.changed.what==RESONA_CHANGED_MODELS&&!s->ready&&s->error!=RESONA_ERR_BUDGET){s->stage=1;s->nextSetup=s->now;}}
void frame(void*state,uint32_t ms){ if(demo_capture::paused(P))return;State*s=static_cast<State*>(state);s->now+=ms;setup(s);
 if(s->sessionDirty&&s->now-s->changed>1500){Session ses={kSessionMagic,sizeof(Session),s->a,s->b,s->blend,s->strain,s->saved.seed,s->level};s->sessionDirty=0;if(P->app_store_write(kSessionAt,&ses,sizeof ses)<0)s->saveFailUntil=s->now+1500;}
s->pick=s->cursor+1;s->kept=(int)s->saved.count;ctl::sync(P,s->link,kBind,kBinds,s->now);
 uint8_t in=0;P->meters(&in,nullptr,nullptr);
 const float target=(float)clamp(in,0,9)/9.0f;
 const float elapsed=(float)ms;
 const float response=target>s->activity?24.0f:170.0f;
 s->activity+=(target-s->activity)*elapsed/(response+elapsed);
 if(s->activity<.015f){
  s->activity=0;
  s->budPhase=0;
 }else{
  s->budPhase+=.018f*elapsed;
  s->budPhase-=6.2831853f*(uint32_t)(s->budPhase/6.2831853f);
 }
 if((int32_t)(s->now-s->nextDraw)>=0){
  s->nextDraw=s->now+(s->activity>0?33:80);
  draw(s);
 }
}
const resona_app app={RESONA_PLATFORM_VERSION,sizeof(resona_app),RESONA_APP_ID,RESONA_APP_NAME,RESONA_APP_VERSION,sizeof(State),1024,init,event,frame};
}
extern "C" const resona_app*resona_app_get(){return &app;}
