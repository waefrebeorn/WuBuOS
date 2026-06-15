/*
 * curriculum_train.c — Full 1→20 Curriculum with Training + Video
 * Uses proven training from bear_cartpole_v1_solve + video recording
 */

#define _POSIX_C_SOURCE 200809L
#include "src/bear/bear_arena.h"
#include "src/bear/bear_env.h"
#include "src/bear/bear_nn.h"
#include "src/bear/bear_ppo.h"
#include "src/bear/bear_opt.h"
#include "src/bear/bear_gaad.h"
#include "src/bear/wubu_math.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <alloca.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_STEPS 500
#define ROLLOUT_LEN 2048
#define VIDEO_FPS 30
#define VIDEO_W 800
#define VIDEO_H 600
#define SOLVED_THRESHOLD 475.0f

/* Geometric Encoder */
typedef struct {
    int num_layers; int* layer_sizes; float* weights; float* biases;
    int input_dim; int output_dim;
} GeometricEncoder;

static GeometricEncoder* geo_encoder_create(BearArena* a, int in_dim, int out_dim, int num_layers) {
    GeometricEncoder* enc = (GeometricEncoder*)bear_arena_alloc(a, sizeof(GeometricEncoder), 1);
    if (!enc) return NULL; enc->num_layers = num_layers; enc->input_dim = in_dim; enc->output_dim = out_dim;
    enc->layer_sizes = (int*)bear_arena_alloc(a, sizeof(int) * (num_layers + 1), 1);
    enc->layer_sizes[0] = in_dim; float phi = 1.6180339887498948482f;
    for (int i = 1; i < num_layers; ++i) {
        float scale = (i % 2 == 0) ? phi : (1.0f / phi);
        float sz = enc->layer_sizes[i-1] * scale;
        enc->layer_sizes[i] = (int)(sz + 0.5f);
        if (enc->layer_sizes[i] < 16) enc->layer_sizes[i] = 16;
        if (enc->layer_sizes[i] > 1024) enc->layer_sizes[i] = 1024;
    }
    enc->layer_sizes[num_layers] = out_dim;
    int tw = 0, tb = 0; for (int i = 0; i < num_layers; ++i) { tw += enc->layer_sizes[i] * enc->layer_sizes[i+1]; tb += enc->layer_sizes[i+1]; }
    enc->weights = (float*)bear_arena_alloc(a, tw * sizeof(float), 1);
    enc->biases = (float*)bear_arena_alloc(a, tb * sizeof(float), 1);
    int wi = 0, bi = 0; uint32_t seed = 0xDEADBEEF;
    for (int i = 0; i < num_layers; ++i) {
        int fi = enc->layer_sizes[i], fo = enc->layer_sizes[i+1];
        float std = sqrtf(2.0f / fi) * (i % 2 == 0 ? 1.618f : 0.618f);
        for (int j = 0; j < fi * fo; ++j) { seed = seed * 1664525 + 1013904223; float r = (seed & 0x7FFFFFFF) / 2147483647.0f * 2.0f - 1.0f; enc->weights[wi++] = r * std; }
        for (int j = 0; j < fo; ++j) enc->biases[bi++] = 0.0f;
    }
    return enc;
}

static void geo_encoder_forward(const GeometricEncoder* enc, const float* in, float* out) {
    int mx = 0; for (int i = 1; i <= enc->num_layers; ++i) if (enc->layer_sizes[i] > mx) mx = enc->layer_sizes[i];
    float* p = (float*)alloca(mx * sizeof(float)); float* c = (float*)alloca(mx * sizeof(float));
    memcpy(p, in, enc->input_dim * sizeof(float));
    int wi = 0, bi = 0;
    for (int L = 0; L < enc->num_layers; ++L) {
        int id = enc->layer_sizes[L], od = enc->layer_sizes[L+1];
        for (int j = 0; j < od; ++j) {
            float sum = enc->biases[bi++];
            for (int k = 0; k < id; ++k) sum += p[k] * enc->weights[wi++];
            float gelu = 0.5f * sum * (1.0f + tanhf(0.79788456f * (sum + 0.044715f * sum * sum * sum)));
            c[j] = gelu * ((L % 2 == 0) ? 1.618f : 0.618f);
        }
        memcpy(p, c, od * sizeof(float));
    }
    memcpy(out, p, enc->output_dim * sizeof(float));
}

/* Video recording */
static void write_ppm(const char* dir, int ep, int step, int W, int H, unsigned char* px) {
    char fn[512]; snprintf(fn, 512, "%s/ep%03d_step%05d.ppm", dir, ep, step);
    FILE* f = fopen(fn, "wb"); if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", W, H); fwrite(px, 1, W * H * 3, f); fclose(f);
}

static void render(unsigned char* px, int W, int H, float cart_x, float cart_vx, float* angles, int npoles, float max_x) {
    memset(px, 240, W * H * 3);
    int cx = (int)((cart_x / max_x + 1) * 0.5f * W), cy = H - 80;
    for (int x = 0; x < W; ++x) { int i = (H-50)*W*3+x*3; px[i]=100; px[i+1]=100; px[i+2]=100; }
    int cw=60, ch=30;
    for (int y=cy-ch; y<cy; ++y) for (int x=cx-cw/2; x<cx+cw/2; ++x) if (x>=0&&x<W&&y>=0&&y<H) { int i=y*W*3+x*3; px[i]=50; px[i+1]=100; px[i+2]=200; }
    for (int w=-1; w<=1; w+=2) { int wx=cx+w*cw/2, wy=cy+5;
        for (int dy=-8; dy<=8; ++dy) for (int dx=-8; dx<=8; ++dx) if (dx*dx+dy*dy<=64) {
            int px_=wx+dx, py_=wy+dy; if (px_>=0&&px_<W&&py_>=0&&py_<H) { int i=py_*W*3+px_*3; px[i]=20; px[i+1]=20; px[i+2]=20; }
        }
    }
    /* INDEPENDENT poles: each pole attaches directly to cart, not chained */
    for (int p=0; p<npoles; ++p) {
        float a = angles[p]; int ph = 120 - p*15;
        int sx = cx, sy = cy - ch;  /* All poles start at cart center */
        int ex = sx + (int)(sinf(a)*ph), ey = sy - (int)(cosf(a)*ph);
        int steps = abs(ex-sx) > abs(ey-sy) ? abs(ex-sx) : abs(ey-sy); if (!steps) steps=1;
        for (int i=0;i<=steps;++i) { float t=(float)i/steps; int px_=sx+(int)((ex-sx)*t), py_=sy+(int)((ey-sy)*t);
            if (px_>=0&&px_<W&&py_>=0&&py_<H) { int i=py_*W*3+px_*3; float h=(p*0.618f)-floorf(p*0.618f); px[i]=(unsigned char)(128+127*sinf(h*6.28f)); px[i+1]=(unsigned char)(128+127*sinf(h*6.28f+2.09f)); px[i+2]=(unsigned char)(128+127*sinf(h*6.28f+4.19f)); }
        }
        if (ex>=0&&ex<W&&ey>=0&&ey<H) { int i=ey*W*3+ex*3; px[i]=255; px[i+1]=255; px[i+2]=0; }
    }
    for (int y=cy-ch-150; y<cy-ch; ++y) { int i=y*W*3+cx*3; if (i>=0&&i<W*H*3) { px[i]=0; px[i+1]=200; px[i+2]=0; } }
}

static void encode_and_send(const char* dir, int poles) {
    char cmd[1024]; snprintf(cmd, 1024, "cd %s && ffmpeg -y -framerate %d -pattern_type glob -i 'ep*.ppm' -c:v libx264 -pix_fmt yuv420p -crf 23 ../cartpole_%dpole.mp4 2>/dev/null && cp ../cartpole_%dpole.mp4 /home/wubu/.hermes/profiles/mind-palace/home/myseed/cartpole_%dpole.mp4", dir, VIDEO_FPS, poles, poles, poles);
    int ret = system(cmd);
    if (ret != 0) { printf("[VIDEO] Encode failed for %d-pole\n", poles); return; }
    printf("[VIDEO] ✓ cartpole_%dpole.mp4 ready\n", poles);
}

/* Trainer state */
typedef struct { BearArena g,r,s; BearEnv *tr,*ev; BearPolicyNet pl; BearValueNet cr; BearGAADOptimizer* gaad; BearPPOConfig cfg; BearTrajectory trj; GeometricEncoder *pe,*ve; int steps, iter; float best; int nenv; int poles; char vdir[512]; unsigned char* fbuf; } Trainer;

static int init_trainer(Trainer* T, int nenv, int poles) {
    memset(T,0,sizeof(Trainer)); T->nenv=nenv; T->poles=poles; T->fbuf=(unsigned char*)malloc(VIDEO_W*VIDEO_H*3);
    snprintf(T->vdir,512,"/tmp/curriculum_%dpole_%ld",poles,time(NULL)); mkdir(T->vdir,0755);
    if (bear_arena_create(&T->g,256*1024*1024)||bear_arena_create(&T->r,64*1024*1024)||bear_arena_create(&T->s,16*1024*1024)) return -1;
    T->tr = bear_env_create_npole(poles, nenv, &T->g); if (!T->tr) return -1; bear_npole_set_episode_length_max(T->tr,MAX_STEPS); T->tr->spec.max_episode_steps=MAX_STEPS;
    T->ev = bear_env_create_npole(poles, 1, &T->g); if (!T->ev) return -1; T->ev->spec.max_episode_steps=MAX_STEPS;
    int od=T->tr->spec.obs_dim, ad=1;
    T->pe=geo_encoder_create(&T->g,od,128,4); if (!T->pe) return -1;
    int ph[]={128,128}; if (bear_policy_create_mlp(&T->pl,&T->g,128,ad,0,ph,2)) return -1; bear_orthogonal_init_params(&T->pl,1.0f); T->pl.logstd=NULL; T->pl.logstd_fixed=0.0f;
    T->ve=geo_encoder_create(&T->g,od,64,3);
    int vh[]={64,64}; if (bear_value_create(&T->cr,&T->g,64,vh,2)) return -1; bear_value_orthogonal_init(&T->cr,1.0f);
    BearGAADConfig gc=bear_gaad_default_config(); gc.base_lr=1e-4f; gc.model_complexity=1; gc.use_log_g_scaling=1; gc.use_anisotropic=1; gc.use_resonant=1; gc.use_poincare=1; gc.use_q_controller=0;
    int pc=0; for (int i=0;i<T->pl.num_layers;++i) if (T->pl.layers[i].param&&T->pl.layers[i].param->weight.data) pc+=T->pl.layers[i].param->weight.shape[0]*T->pl.layers[i].param->weight.shape[1];
    for (int i=0;i<T->cr.num_layers;++i) if (T->cr.layers[i].param&&T->cr.layers[i].param->weight.data) pc+=T->cr.layers[i].param->weight.shape[0]*T->cr.layers[i].param->weight.shape[1];
    T->gaad=bear_gaad_create(&T->g,&gc,pc); if (!T->gaad) return -1;
    T->cfg=bear_ppo_default_config(); T->cfg.lr=1e-4f; T->cfg.epochs_per_iter=4; T->cfg.minibatch_size=64; T->cfg.ent_coef=0.01f;
    if (bear_traj_init(&T->trj,&T->g,ROLLOUT_LEN,nenv,1,od,ad,0)) return -1;
    T->steps=0; T->iter=0; T->best=-INFINITY; return 0;
}

static void destroy_trainer(Trainer* T) {
    if (T->tr) bear_env_close(T->tr);
    if (T->ev) bear_env_close(T->ev);
    /* Skip bear_gaad_destroy - uses free() on arena alloc; OS reclaims on exit */
    (void)T->g; (void)T->r; (void)T->s;
    free(T->fbuf);
}

static int record_solved(Trainer* T, int ep) {
    BearArena sa; bear_arena_create(&sa, 2*1024*1024); bear_env_reset_all(T->ev,&T->g);
    float ret=0; int done=0, step=0, od=T->ev->spec.obs_dim; float* obs=malloc(od*sizeof(float)); float* enc=malloc(128*sizeof(float));
    while (!done && step<MAX_STEPS) {
        bear_arena_reset(&sa); memcpy(obs,T->ev->obs.data,od*sizeof(float)); geo_encoder_forward(T->pe,obs,enc);
        BearTensor et,ac,lp,vl,ho; int64_t es[2]={1,128},as[2]={1,1},sc[1]={1}; bear_tensor_create(&sa,&et,es,2,BEAR_DTYPE_F32,"e"); memcpy(et.data,enc,128*sizeof(float)); bear_tensor_create(&sa,&ac,as,2,BEAR_DTYPE_F32,"a"); bear_tensor_create(&sa,&lp,sc,1,BEAR_DTYPE_F32,"l"); bear_tensor_create(&sa,&vl,sc,1,BEAR_DTYPE_F32,"v"); bear_tensor_create(&sa,&ho,(int64_t[]){1,128},2,BEAR_DTYPE_F32,"h");
        bear_policy_forward(&T->pl,&et,NULL,&ac,&lp,&vl,&ho,&sa); bear_policy_deterministic(&T->pl,&ac);
        float f=((float*)ac.data)[0];
        BearTensor rw,dn,no; bear_tensor_create(&sa,&rw,sc,1,BEAR_DTYPE_F32,"r"); bear_tensor_create(&sa,&dn,sc,1,BEAR_DTYPE_U8,"d"); int64_t ns[2]={1,od}; bear_tensor_create(&sa,&no,ns,2,BEAR_DTYPE_F32,"n");
        float* ea=(float*)T->ev->actions.data; ea[0]=f; T->ev->step(T->ev,&T->ev->actions,&rw,&dn,&no,&sa);
        ret+=((float*)rw.data)[0]; done=((uint8_t*)dn.data)[0]; memcpy(T->ev->obs.data,no.data,od*sizeof(float));
        float ag[20]; for (int p=0;p<T->poles;++p) ag[p]=atan2f(obs[2+p*4],obs[2+p*4+1]);
        render(T->fbuf,VIDEO_W,VIDEO_H,obs[0],obs[1],ag,T->poles,2.5f); write_ppm(T->vdir,ep,step,VIDEO_W,VIDEO_H,T->fbuf);
        step++; 
    }
    int s=step; if (ret>=MAX_STEPS*0.95f) printf("[RECORD] Ep %d: SOLVED (%.1f, %d steps)\n",ep,ret,step); else printf("[RECORD] Ep %d: %.1f return, %d steps\n",ep,ret,step);
    bear_arena_destroy(&sa); free(obs); free(enc); return s;
}

/* Training iteration (adapted from bear_cartpole_v1_solve) */
static float train_iter(Trainer* T, uint64_t rng[2]) {
    BearEnv* e=T->tr; BearTrajectory* tj=&T->trj; BearPolicyNet* pl=&T->pl; BearValueNet* cr=&T->cr; BearGAADOptimizer* ga=T->gaad;
    bear_traj_reset(tj); bear_env_reset_all(e,&T->r);
    double epsum=0; int epcnt=0;
    for (int st=0; st<tj->rollout_len; ++st) {
        float* ob=(float*)e->obs.data; int B=e->spec.num_envs, od=e->spec.obs_dim;
        BearTensor et,ac,lp,vl,ho; int64_t es[2]={B,128},as[2]={B,1},sc[1]={B}; bear_tensor_create(&T->s,&et,es,2,BEAR_DTYPE_F32,"e"); float* eo=(float*)et.data;
        for (int b=0;b<B;++b) geo_encoder_forward(T->pe,ob+b*od,eo+b*128);
        bear_tensor_create(&T->s,&ac,as,2,BEAR_DTYPE_F32,"a"); bear_tensor_create(&T->s,&lp,sc,1,BEAR_DTYPE_F32,"l"); bear_tensor_create(&T->s,&vl,sc,1,BEAR_DTYPE_F32,"v"); bear_tensor_create(&T->s,&ho,(int64_t[]){B,128},2,BEAR_DTYPE_F32,"h");
        bear_policy_forward(pl,&et,NULL,&ac,&lp,NULL,&ho,&T->s); bear_value_forward(cr,&et,&vl,&T->s); bear_policy_sample(pl,&ac,&lp,rng);
        bear_env_step(e,&ac,&e->rewards,&e->dones,&e->obs,&T->s); bear_traj_store(tj,st,&et,&ac,&lp,&e->rewards,&e->dones,&vl);
        float* rv=(float*)e->rewards.data; uint8_t* dn=(uint8_t*)e->dones.data;
        for (int i=0;i<B;++i) if (dn[i]) { float rt=e->episode_return_snapshot?e->episode_return_snapshot[i]:e->episode_return[i]; epsum+=rt; epcnt++; }
        bear_arena_reset(&T->s);
    }
    bear_compute_advantages(tj,&T->cfg,&T->r);
    float tp=0,tv=0,te=0; int mbc=0;
    for (int ep=0; ep<T->cfg.epochs_per_iter; ++ep) {
        BearMinibatchSampler sm; bear_sampler_init(&sm,tj,T->cfg.minibatch_size,rng);
        BearTensor mbO,mbA,mbLP,mbAD,mbR,mbV,mbOLP;
        while (bear_sampler_next(&sm,tj,&mbO,&mbA,&mbLP,&mbAD,&mbR,&mbV,&mbOLP,&T->s)) {
            bear_arena_reset(&T->s); int mz=mbO.shape[0];
            BearTensor fa,nlp,ho; bear_tensor_create(&T->s,&fa,(int64_t[]){mz,1},2,BEAR_DTYPE_F32,"f"); bear_tensor_create(&T->s,&nlp,(int64_t[]){mz},1,BEAR_DTYPE_F32,"nl"); bear_tensor_create(&T->s,&ho,(int64_t[]){mz,128},2,BEAR_DTYPE_F32,"h");
            bear_policy_forward(pl,&mbO,NULL,&fa,&nlp,NULL,&ho,&T->s);
            if (!pl->act_discrete) { float* mu=(float*)pl->layers[pl->num_layers-1].z_pre.data; float* sa=(float*)mbA.data; float* nl=(float*)nlp.data; float ls=pl->logstd?0:pl->logstd_fixed; float vr=expf(2*ls); float ln=-0.5f*logf(2*3.14159265f*vr); for (int i=0;i<mz;++i){ float l=0; float df=sa[i]-mu[i]; l+=-0.5f*df*df/vr+ln; nl[i]=l; } }
            BearTensor vp; bear_tensor_create(&T->s,&vp,(int64_t[]){mz},1,BEAR_DTYPE_F32,"vp"); bear_value_forward(cr,&mbO,&vp,&T->s);
            float pls=0,vl=0,en=0; float* nlp_=(float*)nlp.data,*olp_=(float*)mbOLP.data,*ad_=(float*)mbAD.data;
            for (int i=0;i<mz;++i){ float d=nlp_[i]-olp_[i]; if(d>20)d=20; if(d<-20)d=-20; float r=expf(d); float c=fmaxf(fminf(r,1+T->cfg.clip_coef),1-T->cfg.clip_coef); pls+=-fminf(r*ad_[i],c*ad_[i]); }
            pls/=mz; float* vp_=(float*)vp.data,*mr_=(float*)mbR.data; for (int i=0;i<mz;++i){ float d=vp_[i]-mr_[i]; vl+=0.5f*d*d; } vl/=mz;
            if(pl->act_discrete){}else{float ls=pl->logstd?0:pl->logstd_fixed; en=0.5f*(logf(2*3.14159265f*expf(1))+2*ls);}
            tp+=pls; tv+=vl; te+=en;
            bear_policy_backward(pl,&mbO,&mbA,&mbLP,&mbAD,T->cfg.clip_coef,1.0f,&T->s); bear_value_backward(cr,&mbO,&vp,&mbR,T->cfg.vf_coef,&T->s);
            int tp=0; for(int i=0;i<pl->num_layers;++i){BearParam* p=pl->layers[i].param; if(p&&p->grad.data&&p->weight.data)tp+=(int)bear_tensor_numel(&p->grad);} for(int i=0;i<cr->num_layers;++i){BearParam* p=cr->layers[i].param; if(p&&p->grad.data&&p->weight.data)tp+=(int)bear_tensor_numel(&p->grad);}
            float* ag=(float*)bear_arena_alloc(&T->s,tp*sizeof(float),16); float* ap=(float*)bear_arena_alloc(&T->s,tp*sizeof(float),16); int pi=0;
            for(int i=0;i<pl->num_layers;++i){BearParam* p=pl->layers[i].param; if(p&&p->grad.data&&p->weight.data){int n=(int)bear_tensor_numel(&p->grad); memcpy(ag+pi,p->grad.data,n*sizeof(float)); memcpy(ap+pi,p->weight.data,n*sizeof(float)); pi+=n;}}
            for(int i=0;i<cr->num_layers;++i){BearParam* p=cr->layers[i].param; if(p&&p->grad.data&&p->weight.data){int n=(int)bear_tensor_numel(&p->grad); memcpy(ag+pi,p->grad.data,n*sizeof(float)); memcpy(ap+pi,p->weight.data,n*sizeof(float)); pi+=n;}}
            if(pi>0&&ga){bear_gaad_step(ga,ap,ag,pi,&T->s); int xi=0; for(int i=0;i<pl->num_layers;++i){BearParam* p=pl->layers[i].param; if(p&&p->weight.data){int n=(int)bear_tensor_numel(&p->weight); memcpy(p->weight.data,ap+xi,n*sizeof(float)); xi+=n;}} for(int i=0;i<cr->num_layers;++i){BearParam* p=cr->layers[i].param; if(p&&p->weight.data){int n=(int)bear_tensor_numel(&p->weight); memcpy(p->weight.data,ap+xi,n*sizeof(float)); xi+=n;}}}
            mbc++;
        } free(sm.indices);
    }
    float av=epcnt?(float)(epsum/epcnt):0; if(av>T->best)T->best=av;
    printf("Iter %4d | Steps %8d | Ret %7.2f | PL %7.4f | VL %7.4f | Ent %7.4f | LR %.2e\n", T->iter, T->steps, av,
           tp/mbc, tv/mbc, te/mbc, bear_gaad_get_lr(ga)); fflush(stdout);
    if (T->iter%5==0 && T->iter>0) { for(int ep=0;ep<3;++ep) record_solved(T,ep); }
    T->iter++; T->steps+=tj->rollout_len*e->spec.num_envs; bear_arena_reset(&T->r);
    return av;
}

int main(int argc,char**argv){
    int nenv=16; int from=1,to=10; int iters=40; int seed=(int)time(NULL);
    for(int i=1;i<argc;++i){if(!strcmp(argv[i],"--envs"))nenv=atoi(argv[++i]);else if(!strcmp(argv[i],"--from"))from=atoi(argv[++i]);else if(!strcmp(argv[i],"--to"))to=atoi(argv[++i]);else if(!strcmp(argv[i],"--iters"))iters=atoi(argv[++i]);else if(!strcmp(argv[i],"--seed"))seed=atoi(argv[++i]);else if(!strcmp(argv[i],"--help")){printf("Usage: %s [--envs N] [--from N] [--to N] [--iters N] [--seed N]\n",argv[0]);return 0;}}
    srand(seed);
    printf("═══════════════════════════════════════════════════════════════\n"); printf("  WUBUOS CARTPOLE %d-%d CURRICULUM — GAAD + Geometric + VIDEO\n",from,to); printf("═══════════════════════════════════════════════════════════════\n"); printf("  Envs: %d | Iters/pole: %d | Seed: %d\n",nenv,iters,seed); printf("  Training: N-pole shaped → Video proof each pole\n"); printf("  Physics: cartpole8 exact (m*l, 80N, RK4, hanging)\n"); printf("═══════════════════════════════════════════════════════════════\n\n");
    for(int p=from;p<=to;++p){
        printf("\n=== POLE %d/%d ==============================================\n",p,to);
        Trainer T; if(init_trainer(&T,nenv,p)!=0){fprintf(stderr,"Init failed %d\n",p);continue;}
        for(int it=0;it<iters;++it){uint64_t rg[2]={0xDEADBEEFDEADBEEFull^(uint64_t)seed,0xCAFEBABECAFEBABEull^(uint64_t)time(NULL)^it}; float r=train_iter(&T,rg); if(r>=SOLVED_THRESHOLD)break;}
        printf("Recording solved episodes...\n"); for(int ep=0;ep<3;++ep) record_solved(&T,ep);
        encode_and_send(T.vdir,p);
        destroy_trainer(&T);
        printf("✓ Pole %d complete\n",p);
    }
    printf("\n═══════════════════════════════════════════════════════════════\n"); printf("  CURRICULUM %d-%d COMPLETE\n",from,to); printf("═══════════════════════════════════════════════════════════════\n"); return 0;
}
