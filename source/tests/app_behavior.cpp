#include <cassert>
#include <cstdio>
#include <cstring>
#define RESONA_APP_ID "org.resona.demo.mutation"
#define RESONA_APP_NAME "Mutation"
#define RESONA_APP_VERSION "0.1.0"
#include "../src/app.cpp"
static int writes=0;static Saved persisted={};
static int store(uint32_t off,const void*p,uint32_t n){assert(off==0&&n==sizeof(Saved));++writes;std::memcpy(&persisted,p,n);return 0;}
int main(){resona_platform platform={};platform.app_store_write=store;P=&platform;State s={};s.a={{10,76,5,7}};s.b={{75,32,70,62}};s.blend=72;s.saved.seed=0x61cba913;s.cursor=-1;
 Tone expected=current(&s);propose(&s);for(int i=0;i<4;++i)assert(s.a.v[i]==expected.v[i]);assert(s.blend==0&&writes==0);
 s.blend=100;Tone favorite=current(&s);input(&s,RESONA_EVENT_LONG,1,1);assert(writes==1&&s.saved.count==1);for(int i=0;i<4;++i)assert(persisted.tone[0].v[i]==favorite.v[i]);
 input(&s,RESONA_EVENT_CLICK,1,1);assert(s.blend==0&&writes==1);
 input(&s,RESONA_EVENT_TURN,1,20);assert(s.blend==60&&writes==1);
 input(&s,RESONA_EVENT_TURN,0,1);for(int i=0;i<4;++i)assert(s.b.v[i]==favorite.v[i]);assert(s.blend==100&&writes==1);
 for(int j=0;j<10;++j){propose(&s);s.blend=100;keep(&s);}assert(s.saved.count==8&&persisted.count==8&&writes==11);
 std::puts("{\"propose_continuity\":true,\"saved_recall\":true,\"favorite_capacity\":8,\"writes_only_on_keep\":true}");
}
