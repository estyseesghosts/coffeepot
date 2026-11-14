/*
 * PICA Lab phase-1 hardware suite.
 * No arbitrary register writes. Offscreen RGBA8 readback through GX PPF.
 * Reference API use: devkitPro/3ds-examples/graphics/gpu/simple_tri.
 * WARNING: Build and on-device execution have not yet been validated.
 */
#include <3ds.h>
#include <citro3d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>
#include "vshader_shbin.h"

#define SIZE 256
#define CLEAR 0x000000FFu
#define PIXELS (SIZE * SIZE)
#define BYTES (PIXELS * 4)
#define MAX_VERTS 49152  /* 16384 triangles: largest perf2 variant */
#define OUTPUT_DIR "sdmc:/3ds/pica-lab"
#define TRANSFER_FLAGS (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | \
    GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | \
    GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

typedef struct { float x, y, z; } Vertex;
static C3D_RenderTarget *target;
static DVLB_s *dvlb;
static shaderProgram_s program;
static C3D_Mtx projection;
static Vertex *vertices;
static void *staging;
static int proj_loc;
static FILE *logfile;
static unsigned case_id;
static bool error_flag;
static bool calibrated;
static char run_dir[96];
static FILE *runlog;
static u64 t0_tick;

/* Timestamped log to run.log (always) and the console (if console=true). */
static void logmsg(bool console, const char *fmt, ...) {
    char line[200];
    va_list ap; va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (console) printf("%s\n", line);
    if (runlog) {
        double s = (double)(svcGetSystemTick() - t0_tick) / SYSCLOCK_ARM11;
        fprintf(runlog, "[%9.3f] %s\n", s, line);
        fflush(runlog);
    }
}
static void make_run_dir(void) {
    mkdir(OUTPUT_DIR, 0777); /* EEXIST is fine */
    for (int i = 1; i < 1000; i++) {
        snprintf(run_dir, sizeof(run_dir), OUTPUT_DIR "/run_%03d", i);
        if (mkdir(run_dir, 0777) == 0) return;
        if (errno != EEXIST) break;
    }
    run_dir[0] = 0;
}

/* FNV-1a: used ONLY for the shader-binary id in manifest.txt. Never for image
 * identity (it collided on two different images); images use crc32_buf. */
static u32 hash32(const u8 *data, size_t n) {
    u32 h = 2166136261u;
    for (size_t i = 0; i < n; i++) { h ^= data[i]; h *= 16777619u; }
    return h;
}
/* p3d_us: GSP-measured command-list time in microseconds, -1 when not measured. */
static void log_case(const char *family, const char *variant, int parameter,
                     u32 hash, int marked, u32 center, double gpu_ms, long p3d_us) {
    if (!logfile) return;
    fprintf(logfile, "%u,%s,%s,%d,%08lX,%d,%08lX,%.6f,%ld\n",
            case_id, family, variant, parameter, (unsigned long)hash,
            marked, (unsigned long)center, gpu_ms, p3d_us);
    fflush(logfile);
}

/*
 * Vertex slots. The GPU reads the VBO when the command list executes (after
 * C3D_FrameEnd), not when C3D_DrawArrays is called, so every draw in a frame
 * must own distinct vertices. (v0.2 reused slot 0 for every triangle, so the
 * edge test drew its second triangle twice.)
 */
static int vcur;
static Vertex *vtx_alloc(int n) {
    if (vcur + n > MAX_VERTS) { error_flag = true; return NULL; }
    Vertex *p = vertices + vcur;
    vcur += n;
    return p;
}
static void rect(float x0, float y0, float x1, float y1, float z) {
    const Vertex q[6] = {
        {x0,y0,z}, {x1,y0,z}, {x0,y1,z},
        {x0,y1,z}, {x1,y0,z}, {x1,y1,z}
    };
    Vertex *v = vtx_alloc(6); if (!v) return;
    memcpy(v,q,sizeof(q));
    C3D_DrawArrays(GPU_TRIANGLES,(int)(v-vertices),6);
}
static void triangle(float x0, float y0, float x1, float y1, float x2, float y2) {
    Vertex *v = vtx_alloc(3); if (!v) return;
    v[0] = (Vertex){x0,y0,0.5f};
    v[1] = (Vertex){x1,y1,0.5f};
    v[2] = (Vertex){x2,y2,0.5f};
    C3D_DrawArrays(GPU_TRIANGLES,(int)(v-vertices),3);
}

/* ---- Uncompressed (STORE) zip writer ------------------------------------
 * Captures are appended straight into one .zip so a run is a single file to
 * copy. The central directory is rewritten every ZIP_SYNC_EVERY entries and after
 * each family, so the archive stays readable if the program dies mid-run. Timestamps are fixed
 * (1980-01-01) so identical runs give identical archives. */
#define ZIP_MAX 1024
typedef struct { char name[64]; u32 crc, size, off; } ZipEnt;
static FILE *zipf;
static ZipEnt zents[ZIP_MAX];
static int znent;
static u32 zend;
static char zprefix[32];
static u32 crc_table[256];

static void crc_init(void) {
    for (u32 n = 0; n < 256; n++) {
        u32 c = n;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_table[n] = c;
    }
}
static u32 crc32_buf(const u8 *d, size_t n) {
    u32 c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = crc_table[(c ^ d[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
static void w16(FILE *f, u32 v) { fputc(v & 0xFF, f); fputc((v >> 8) & 0xFF, f); }
static void w32(FILE *f, u32 v) { w16(f, v & 0xFFFF); w16(f, v >> 16); }
#define ZIP_SYNC_EVERY 32   /* entries between central-directory rewrites */
static int zdirty;
static bool zip_sync(void) {
    zdirty = 0;
    if (fseek(zipf, zend, SEEK_SET)) return false;
    u32 cd = 0;
    for (int i = 0; i < znent; i++) {
        size_t nl = strlen(zents[i].name);
        w32(zipf, 0x02014b50); w16(zipf, 20); w16(zipf, 10); w16(zipf, 0); w16(zipf, 0);
        w16(zipf, 0); w16(zipf, 0x21);
        w32(zipf, zents[i].crc); w32(zipf, zents[i].size); w32(zipf, zents[i].size);
        w16(zipf, (u32)nl); w16(zipf, 0); w16(zipf, 0); w16(zipf, 0); w16(zipf, 0);
        w32(zipf, 0); w32(zipf, zents[i].off);
        fwrite(zents[i].name, 1, nl, zipf);
        cd += 46 + (u32)nl;
    }
    w32(zipf, 0x06054b50); w16(zipf, 0); w16(zipf, 0);
    w16(zipf, (u32)znent); w16(zipf, (u32)znent); w32(zipf, cd); w32(zipf, zend); w16(zipf, 0);
    if (fflush(zipf)) return false;
    return !ferror(zipf);
}
static bool zip_add(const char *name, const void *data, size_t len) {
    if (!zipf || znent >= ZIP_MAX) return false;
    ZipEnt *e = &zents[znent];
    snprintf(e->name, sizeof(e->name), "%s/%s", zprefix, name);
    e->crc = crc32_buf((const u8*)data, len);
    e->size = (u32)len;
    e->off = zend;
    size_t nl = strlen(e->name);
    if (fseek(zipf, zend, SEEK_SET)) return false;
    /* Pad the local header (zip "extra" field) so the data starts on a 512-byte
     * boundary. Unaligned starts made every other 256 KB write take ~10 s on
     * the SD driver. */
    u32 pad = (512 - ((zend + 30 + (u32)nl) & 511)) & 511;
    if (pad && pad < 4) pad += 512;
    w32(zipf, 0x04034b50); w16(zipf, 10); w16(zipf, 0); w16(zipf, 0);
    w16(zipf, 0); w16(zipf, 0x21);
    w32(zipf, e->crc); w32(zipf, e->size); w32(zipf, e->size);
    w16(zipf, (u32)nl); w16(zipf, pad);
    fwrite(e->name, 1, nl, zipf);
    if (pad) {
        static const u8 zeros[512] = {0};
        w16(zipf, 0x4150); w16(zipf, pad - 4);   /* id 'PA', length, zeros */
        if (pad > 4) fwrite(zeros, 1, pad - 4, zipf);
    }
    if (len && fwrite(data, 1, len, zipf) != len) return false;
    zend += 30 + (u32)nl + pad + (u32)len;
    znent++;
    /* Batched: a crash loses at most ZIP_SYNC_EVERY entries from the index
     * (still recoverable with `zip -FF`); families also sync when they finish. */
    return (++zdirty >= ZIP_SYNC_EVERY) ? zip_sync() : !ferror(zipf);
}
static void tev_passthrough(void) {
    for (int i=0;i<6;i++) C3D_TexEnvInit(C3D_GetTexEnv(i));
    C3D_TexEnv *env = C3D_GetTexEnv(0);
    C3D_TexEnvSrc(env,C3D_Both,GPU_PRIMARY_COLOR,GPU_PRIMARY_COLOR,GPU_PRIMARY_COLOR);
    C3D_TexEnvFunc(env,C3D_Both,GPU_REPLACE);
}
static void basic_state(void) {
    C3D_BindProgram(&program);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,proj_loc,&projection);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_DepthTest(false,GPU_ALWAYS,GPU_WRITE_COLOR);
    C3D_AlphaTest(false,GPU_ALWAYS,0);
    C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_ONE,GPU_ZERO,GPU_ONE,GPU_ZERO);
    tev_passthrough();
}
static bool begin_case(void) {
    vcur = 0;
    if (!C3D_FrameBegin(C3D_FRAME_SYNCDRAW)) { error_flag = true; return false; }
    C3D_RenderTargetClear(target,C3D_CLEAR_ALL,CLEAR,0);
    if (!C3D_FrameDrawOn(target)) { error_flag=true; C3D_FrameEnd(0); return false; }
    basic_state();
    return true;
}
static bool capture(void) {
    // FrameEnd submits the P3D list; C3D_SyncDisplayTransfer waits for the
    // preceding queue, submits PPF, then waits for its completion.
    C3D_SyncDisplayTransfer((u32*)target->frameBuf.colorBuf,
        GX_BUFFER_DIM(SIZE,SIZE),(u32*)staging,GX_BUFFER_DIM(SIZE,SIZE),TRANSFER_FLAGS);
    Result rc = GSPGPU_InvalidateDataCache(staging,BYTES);
    if (R_FAILED(rc)) { logmsg(true,"Invalidate failed %08lX",(unsigned long)rc); error_flag=true; return false; }
    return true;
}
static void save_capture(const char *family, unsigned id) {
    char name[64];
    snprintf(name,sizeof(name),"%s_%04u.rgba",family,id);
    if (!zip_add(name,staging,BYTES)) { logmsg(true,"Zip write error: %s",name); error_flag=true; }
}
static void record(const char *family,const char *variant,int parameter,bool dump,double gpu_ms) {
    if (!capture()) return;
    const u32 *p = (const u32*)staging;
    int marked=0;
    for (int i=0;i<PIXELS;i++) if (p[i]!=CLEAR) marked++;
    u32 h=crc32_buf((const u8*)staging,BYTES);
    u32 center=p[(SIZE/2)*SIZE + SIZE/2];
    log_case(family,variant,parameter,h,marked,center,gpu_ms,-1);
    if (dump) save_capture(family,case_id);
    logmsg(false,"case %04u %-9s %s %d pixels=%d h=%08lX",
        case_id,family,variant,parameter,marked,(unsigned long)h);
    case_id++;
}
static void run_selftest(void) {
    calibrated=false;
    u32 baseline_hash=0;
    bool pass=true;
    for(int i=0;i<3;i++) {
        if(!begin_case()) return;
        C3D_FixedAttribSet(1,1,1,1,1);
        if(i) rect(50,60,150,170,0.5f);
        // Explicit documented PICA framebuffer cache flush before PPF readback.
        GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH,1);
        C3D_FrameEnd(0);
        record("selftest",i?"rect":"clear",i,true,0.0);
        if(error_flag) return;
        const u32 *pixel=(const u32*)staging;
        int count=0;
        for(int j=0;j<PIXELS;j++) if(pixel[j]!=CLEAR) count++;
        u32 h=crc32_buf((const u8*)staging,BYTES);
        if(i==0 && count!=0) pass=false;
        if(i==1) {
            if(count<100 || count>=PIXELS) pass=false;
            baseline_hash=h;
        }
        if(i==2 && (count<100 || h!=baseline_hash)) pass=false;
    }
    calibrated=pass;
    logmsg(true,"Self-test: %s",pass?"PASS":"FAIL: no other tests permitted");
}
static void run_raster(void) {
    // Triangle translates through 1/64-pixel steps; geometry size is constant.
    // Re-run in reverse order to expose order/drift/cache artifacts.
    for(int pass=0;pass<2;pass++) {
        for(int i=0;i<64;i++) {
            int step=pass?63-i:i;
            float x=(float)(80.0 + (double)step/64.0);
            if(!begin_case()) return;
            C3D_FixedAttribSet(1,1,1,1,1);
            triangle(x,48.0f,x+96.0f,48.0f,x,176.0f);
            GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH,1);
            C3D_FrameEnd(0);
            record("raster",pass?"reverse":"forward",step,true,0.0);
        }
    }
}
/* raster_fine: 1/256-pixel offset sweeps in short windows around the exact-model
 * transition points. Geometry and windows MUST match tools/rasterfine.py (GEOMS, WINS).
 * Offsets are multiples of 1/256, exactly representable as float. Each window runs
 * forward (lo..hi) then, in a second pass, reverse (hi..lo, windows in reverse order). */
typedef struct { const char *name; int geom; char axis; int lo, hi; } FineWin;
static const float fine_geoms[6][3][2] = {
    {{80,48},{176,48},{80,176}},      /* A: 3:4 hypotenuse */
    {{100,40},{212,61},{124,181}},    /* B: three slanted edges */
    {{80,48},{80,176},{176,48}},      /* Ar: A, reversed vertex order (opposite winding) */
    {{100,40},{124,181},{212,61}},    /* Br: B, reversed vertex order */
    {{176,176},{80,176},{176,48}},    /* C: right angle bottom-right; normals (0,-),(-,0),(+,+) */
    {{176,176},{176,48},{80,176}},    /* Cr: C, reversed vertex order */
};
static const FineWin fine_wins[] = {
    {"A1",0,'x',26,38},  {"A2",0,'x',90,102}, {"A3",0,'x',122,134},
    {"A4",0,'y',122,134},{"A5",0,'y',36,48},
    {"B1",1,'x',28,40},  {"B2",1,'x',118,130},{"B3",1,'x',150,162},
    {"B4",1,'y',112,124},
    /* v0.5 tie-rule windows (reversed winding): same offsets as the A/B windows with ties */
    {"R1",2,'x',26,38},  {"R2",2,'x',90,102}, {"R3",2,'x',122,134},
    {"R4",2,'y',122,134},
    {"R5",3,'x',118,130},{"R6",3,'y',112,124},
    /* v0.6 tie probes for normal classes with no data (see tools/rasterfine.py) */
    {"C1",4,'x',122,134},{"C2",4,'y',122,134},{"C3",4,'x',90,102},
    {"C4",5,'x',122,134},{"C5",5,'y',122,134},{"C6",5,'x',90,102},
};
#define FINE_NWINS ((int)(sizeof(fine_wins)/sizeof(fine_wins[0])))
static void run_raster_fine(void) {
    for(int pass=0;pass<2;pass++) {
        for(int wi=0;wi<FINE_NWINS;wi++) {
            const FineWin *w=&fine_wins[pass?FINE_NWINS-1-wi:wi];
            for(int j=w->lo;j<=w->hi;j++) {
                int k=pass? w->hi-(j-w->lo) : j;
                float o=(float)((double)k/256.0);
                float ox=w->axis=='x'?o:0.0f, oy=w->axis=='y'?o:0.0f;
                const float (*g)[2]=fine_geoms[w->geom];
                if(!begin_case()) return;
                C3D_FixedAttribSet(1,1,1,1,1);
                triangle(g[0][0]+ox,g[0][1]+oy,g[1][0]+ox,g[1][1]+oy,g[2][0]+ox,g[2][1]+oy);
                GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH,1);
                C3D_FrameEnd(0);
                char var[24];
                snprintf(var,sizeof(var),"%s_%s",w->name,pass?"reverse":"forward");
                record("raster_fine",var,k,true,0.0);
                if(error_flag) return;
            }
        }
    }
}
/* Quad [64,192]^2 (+ offset) split along one of two diagonals. */
static void edge_quad(int diagonal, float ox, float oy) {
    const float a=64+ox, b=192+ox, c=64+oy, d=192+oy;
    if(!diagonal) { triangle(a,c,b,c,a,d); triangle(a,d,b,c,b,d); }
    else          { triangle(a,c,b,c,b,d); triangle(a,c,b,d,a,d); }
}
/* Four triangles fanned around an interior vertex: slanted shared edges. */
static void edge_fan(float cx, float cy) {
    triangle(64,64,192,64,cx,cy);
    triangle(192,64,192,192,cx,cy);
    triangle(192,192,64,192,cx,cy);
    triangle(64,192,64,64,cx,cy);
}
/* Additive coverage: each hit adds ~0x40 to RGB over a black clear, so
 * gap = 0, single hit ~0x40, double hit ~0x80. Logs gap/overlap counts. */
static void cover_stats(int *gaps, int *overlaps) {
    const u32 *p=(const u32*)staging;
    int g=0,o=0;
    for(int y=0;y<SIZE;y++) for(int x=0;x<SIZE;x++) {
        u32 r=(p[y*SIZE+x]>>24)&0xFF;
        if(r>=0x60) o++;
        else if(r<0x20 && x>=68 && x<188 && y>=68 && y<188) g++; /* interior only */
    }
    *gaps=g; *overlaps=o;
}
static void run_edge(void) {
    // Part 1: plain overwrite, both diagonals. Full quad = 16384 px; each
    // diagonal case must now cover ~the whole quad (v0.2 covered half).
    for(int diagonal=0;diagonal<2;diagonal++) {
        if(!begin_case()) return;
        C3D_FixedAttribSet(1,1,1,1,1);
        edge_quad(diagonal,0,0);
        GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH,1);
        C3D_FrameEnd(0);
        record("edge",diagonal?"reverse_diag":"normal_diag",diagonal,true,0.0);
        if(error_flag) return;
    }
    // Part 2: additive watertightness at fractional quad positions.
    static const float offs[8]={0.0f,1.0f/64,1.0f/16,1.0f/8,0.25f,0.375f,0.5f,0.75f};
    int idx=0;
    for(int mode=0;mode<3;mode++) {          /* 0: diag A  1: diag B  2: fan */
        for(int axis=0;axis<2;axis++) {      /* 0: offset x+y  1: offset x only */
            if(mode==2 && axis==1) continue;
            for(int k=0;k<8;k++) {
                float ox=offs[k], oy=axis?0.0f:offs[k];
                if(!begin_case()) return;
                C3D_FixedAttribSet(1,0.25f,0.25f,0.25f,0.25f);
                C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_ONE,GPU_ONE,GPU_ONE,GPU_ONE);
                if(mode==2) edge_fan(128+ox,128+oy); else edge_quad(mode,ox,oy);
                GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH,1);
                C3D_FrameEnd(0);
                char var[48];
                snprintf(var,sizeof(var),"%s_%s_%.6f",mode==2?"fan":(mode?"diagB":"diagA"),axis?"x":"xy",ox);
                unsigned id=case_id;
                record("edge_cov",var,idx++,true,0.0);
                if(error_flag) return;
                int gaps,overlaps; cover_stats(&gaps,&overlaps);
                logmsg(false,"cover %04u %s gaps=%d overlaps=%d",id,var,gaps,overlaps);
            }
        }
    }
}
static void run_tev(void) {
    // Constant multiplier in stage 1; stage 0 passes vertex color.
    // Alpha is left in passthrough mode to isolate RGB rounding.
    for(int i=0;i<=16;i++) {
        if(!begin_case()) return;
        C3D_FixedAttribSet(1,0.5f,0.25f,0.75f,1.0f);
        C3D_TexEnv *env=C3D_GetTexEnv(1);
        C3D_TexEnvInit(env);
        C3D_TexEnvSrc(env,C3D_RGB,GPU_PREVIOUS,GPU_CONSTANT,GPU_PREVIOUS);
        C3D_TexEnvFunc(env,C3D_RGB,GPU_MODULATE);
        C3D_TexEnvSrc(env,C3D_Alpha,GPU_PREVIOUS,GPU_PREVIOUS,GPU_PREVIOUS);
        C3D_TexEnvFunc(env,C3D_Alpha,GPU_REPLACE);
        u32 v=(u32)((i*255+8)/16);
        C3D_TexEnvColor(env,(v<<24)|(v<<16)|(v<<8)|v);
        rect(32,32,224,224,0.5f);
        GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH,1);
        C3D_FrameEnd(0);
        record("tev","constant_rgb",i,true,0.0);
    }
}
static void run_blend(void) {
    for(int i=0;i<17;i++) {
        if(!begin_case()) return;
        C3D_FixedAttribSet(1,0.25f,0.25f,0.25f,1.0f);
        rect(32,32,224,224,0.5f);
        C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_SRC_ALPHA,
                       GPU_ONE_MINUS_SRC_ALPHA,GPU_ONE,GPU_ZERO);
        C3D_FixedAttribSet(1,0.75f,0.0f,0.0f,(float)i/16.0f);
        rect(32,32,224,224,0.5f);
        GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH,1);
        C3D_FrameEnd(0);
        record("blend","alpha",i,true,0.0);
    }
}
static void run_readonly_probe(void) {
    // Non-destructive service call; offsets are relative to 0x1EB00000.
    // Not a profiler enable! A denied read is recorded as a result.
    u32 hwid=0, busy=0;
    Result id_rc=GSPGPU_ReadHWRegs(0x00400000,&hwid,4);
    Result busy_rc=GSPGPU_ReadHWRegs(0x00400034,&busy,4);
    logmsg(true,"HWID rc=%08lX value=%08lX",(unsigned long)id_rc,(unsigned long)hwid);
    logmsg(true,"BUSY rc=%08lX value=%08lX",(unsigned long)busy_rc,(unsigned long)busy);
    char ppath[128]; snprintf(ppath,sizeof(ppath),"%s/probe.txt",run_dir);
    FILE *f=fopen(ppath,"w");
    if(f) {
        fprintf(f,"id_rc=%08lX id=%08lX busy_rc=%08lX busy=%08lX\n",
           (unsigned long)id_rc,(unsigned long)hwid,(unsigned long)busy_rc,(unsigned long)busy);
        fclose(f);
    }
}
typedef struct { const char *name; int tris; int draws; int quads; } PerfVar;
/* Each variant is one frame. tris: small triangles (9x9 px). quads: full-
 * target 256x256 quads (fill-rate load). draws: the work is split evenly into
 * this many C3D_DrawArrays calls. "empty" is the fixed-cost floor. */
static const PerfVar perf_vars[] = {
    {"empty",       0,    0,  0},
    {"tri192_d1",   192,  1,  0},
    {"tri192_d64",  192,  64, 0},
    {"tri768_d1",   768,  1,  0},
    {"tri768_d16",  768,  16, 0},
    {"tri768_d64",  768,  64, 0},
    {"tri768_d256", 768,  256,0},
    {"tri3072_d1",  3072, 1,  0},
    {"fill1_d1",    0,    1,  1},
    {"fill16_d1",   0,    1,  16},
    {"fill16_d16",  0,    16, 16},
};
#define PERF_NVARS ((int)(sizeof(perf_vars)/sizeof(perf_vars[0])))
#define PERF_WARMUP 4
#define PERF_REPS 40

static int cmp_double(const void *a, const void *b) {
    double x=*(const double*)a, y=*(const double*)b;
    return (x>y)-(x<y);
}
static double median_of(double *v, int n) {
    if(n<=0) return 0;
    qsort(v,n,sizeof(double),cmp_double);
    return (n&1)? v[n/2] : 0.5*(v[n/2-1]+v[n/2]);
}
/* One timed frame. No clear and no readback: only the GPU work is timed.
 * C3D_FrameSync() makes the drawing time belong to *this* frame, and the GSP
 * perf log gives command-list (P3D) time without the completion latency. */
static bool perf_frame(const PerfVar *pv, double *gpu_ms, long *p3d_us, float *cmd_use) {
    vcur=0;
    if(!C3D_FrameBegin(C3D_FRAME_SYNCDRAW)) { error_flag=true; return false; }
    if(!C3D_FrameDrawOn(target)) { error_flag=true; C3D_FrameEnd(0); return false; }
    basic_state();
    C3D_FixedAttribSet(1,1,1,1,1);
    if(pv->tris) {
        Vertex *v=vtx_alloc(pv->tris*3);
        if(!v) { C3D_FrameEnd(0); return false; }
        for(int n=0;n<pv->tris;n++) {
            int cell=n%256, col=cell%16, row=cell/16;
            float x=(float)(col*15+4), y=(float)(row*15+4);
            v[n*3+0]=(Vertex){x,y,0.5f};
            v[n*3+1]=(Vertex){x+9,y,0.5f};
            v[n*3+2]=(Vertex){x,y+9,0.5f};
        }
        int per=pv->tris*3/pv->draws, first=(int)(v-vertices);
        for(int d=0;d<pv->draws;d++) C3D_DrawArrays(GPU_TRIANGLES,first+d*per,per);
    } else if(pv->quads) {
        Vertex *v=vtx_alloc(pv->quads*6);
        if(!v) { C3D_FrameEnd(0); return false; }
        for(int q=0;q<pv->quads;q++) {
            Vertex *w=v+q*6;
            w[0]=(Vertex){0,0,0.5f};   w[1]=(Vertex){SIZE,0,0.5f};   w[2]=(Vertex){0,SIZE,0.5f};
            w[3]=(Vertex){0,SIZE,0.5f};w[4]=(Vertex){SIZE,0,0.5f};   w[5]=(Vertex){SIZE,SIZE,0.5f};
        }
        int per=pv->quads*6/pv->draws, first=(int)(v-vertices);
        for(int d=0;d<pv->draws;d++) C3D_DrawArrays(GPU_TRIANGLES,first+d*per,per);
    }
    *cmd_use=C3D_GetCmdBufUsage();
    GSPGPU_PerfLog p0, p1;
    bool have = R_SUCCEEDED(GSPGPU_GetPerfLog(&p0));
    C3D_FrameEnd(0);
    C3D_FrameSync();
    have = have && R_SUCCEEDED(GSPGPU_GetPerfLog(&p1));
    *gpu_ms=C3D_GetDrawingTime();
    *p3d_us = have ? (long)(u32)(p1.entries[GSPGPU_EVENT_P3D].totalDurationUs -
                              p0.entries[GSPGPU_EVENT_P3D].totalDurationUs) : -1;
    /* Keep anomalous observations; record raw counters for investigation.
       DrawingTime is an independent, imperfect cross-check, not a GPU oracle. */
    if (have && *p3d_us > 5000 && *p3d_us > *gpu_ms * 1000.0 + 5000.0)
        logmsg(true,"WARN timing mismatch p3d=%ldus drawing=%.3fms total=%lu->%lu last=%lu->%lu",
               *p3d_us,*gpu_ms,
               (unsigned long)p0.entries[GSPGPU_EVENT_P3D].totalDurationUs,
               (unsigned long)p1.entries[GSPGPU_EVENT_P3D].totalDurationUs,
               (unsigned long)p0.entries[GSPGPU_EVENT_P3D].lastDurationUs,
               (unsigned long)p1.entries[GSPGPU_EVENT_P3D].lastDurationUs);
    return true;
}
static void run_perf(void) {
    // Interleaved design: every rep visits every variant once, alternating
    // forward/reverse order, so drift hits all variants equally. The "empty"
    // variant is the per-frame floor; compare others against it.
    static double gpu[PERF_NVARS][PERF_REPS], p3d[PERF_NVARS][PERF_REPS];
    int n[PERF_NVARS]={0};
    bool perflog=R_SUCCEEDED(GSPGPU_SetPerfLogMode(true));
    logmsg(true,"perf: %d variants x %d reps (+%d warmup), perflog %s",
           PERF_NVARS,PERF_REPS,PERF_WARMUP,perflog?"on":"UNAVAILABLE");
    for(int rep=-PERF_WARMUP; rep<PERF_REPS; rep++) {
        for(int i=0;i<PERF_NVARS;i++) {
            int vi=(rep&1)? PERF_NVARS-1-i : i;
            double g; long p; float cu;
            if(!perf_frame(&perf_vars[vi],&g,&p,&cu)) goto out;
            if(cu>0.9f) logmsg(true,"WARN cmdbuf %.0f%% in %s",cu*100.0,perf_vars[vi].name);
            if(rep<0) continue;
            log_case("perf",perf_vars[vi].name,rep,0,0,0,g,p);
            logmsg(false,"case %04u perf      %s %d gpu_ms=%.6f p3d_us=%ld cmdbuf=%.0f%%",
                   case_id,perf_vars[vi].name,rep,g,p,cu*100.0);
            case_id++;
            gpu[vi][n[vi]]=g; p3d[vi][n[vi]]=(double)p; n[vi]++;
        }
    }
    double base_g=0, base_p=0;
    for(int vi=0;vi<PERF_NVARS;vi++) {
        double mg=median_of(gpu[vi],n[vi]), mp=median_of(p3d[vi],n[vi]);
        if(vi==0) { base_g=mg; base_p=mp; }
        logmsg(true,"%-11s g=%.3fms p3d=%.0fus (+%.0f)",perf_vars[vi].name,mg,mp,mp-base_p);
    }
    (void)base_g;
out:
    GSPGPU_SetPerfLogMode(false);
}
/* ---- perf2: triangle size as the independent variable ----------------------
 * Right triangles with legs s px on an s-pitch grid (cells wrap when the count
 * exceeds the grid), depth/blend/texture off. Series:
 *   tri_sS_nN  : s=size, N=count. Fixed-pixel series: N chosen so N*px(s) ~ P2_PIXELS.
 *                Fixed-count series: N = P2_COUNT for several sizes.
 *   quad_nN    : N full-target quads (2N triangles, 65536 px each).
 * Pixels per triangle are NOT assumed: perf2_cov captures one triangle per size and
 * logs its marked-pixel count (nonclear_pixels column). Host fits p3d_us = a*tris + b*px. */
#define P2_PIXELS 24000
#define P2_COUNT 256
#define P2_MAX 24
typedef struct { char name[24]; int s, n, quads, pitch, draws; } P2Var;   /* pitch 0 = s; draws 0 = 1 */
static double last_wall_us;   /* svcGetSystemTick wall time of FrameEnd..FrameSync in the last p2_frame */
static P2Var p2vars[P2_MAX];
static int p2n;
static int p2_pxest(int s) { return s*(s-1)/2; }   /* only used to choose counts */
static void p2_add(int s, int n, int quads) {
    P2Var *v=&p2vars[p2n++];
    v->s=s; v->n=n; v->quads=quads;
    if(quads) snprintf(v->name,sizeof(v->name),"quad_n%d",quads);
    else snprintf(v->name,sizeof(v->name),"tri_s%d_n%d",s,n);
}
static void p2_build(void) {
    static const int fixed_px[]={3,4,6,8,12,16,24,32,48,64};
    static const int fixed_ct[]={3,4,8,16,32,64};
    p2n=0;
    snprintf(p2vars[p2n].name,sizeof(p2vars[0].name),"empty"); p2vars[p2n].s=p2vars[p2n].n=p2vars[p2n].quads=0; p2n++;
    for(unsigned i=0;i<sizeof(fixed_px)/sizeof(fixed_px[0]);i++) {
        int s=fixed_px[i], n=(P2_PIXELS+p2_pxest(s)/2)/p2_pxest(s);
        p2_add(s,n,0);
    }
    for(unsigned i=0;i<sizeof(fixed_ct)/sizeof(fixed_ct[0]);i++) {
        int s=fixed_ct[i], n=P2_COUNT;
        bool dup=false;   /* skip if identical to a fixed-pixel variant */
        for(int j=1;j<p2n;j++) if(p2vars[j].s==s && p2vars[j].n==n) dup=true;
        if(!dup) p2_add(s,n,0);
    }
    p2_add(0,0,1);
    p2_add(0,0,2);
}
static void p2_fill(const P2Var *pv, Vertex *v) {
    if(pv->quads) {
        for(int q=0;q<pv->quads;q++) {
            Vertex *w=v+q*6;
            w[0]=(Vertex){0,0,0.5f};    w[1]=(Vertex){SIZE,0,0.5f};  w[2]=(Vertex){0,SIZE,0.5f};
            w[3]=(Vertex){0,SIZE,0.5f}; w[4]=(Vertex){SIZE,0,0.5f};  w[5]=(Vertex){SIZE,SIZE,0.5f};
        }
        return;
    }
    int pitch=pv->pitch?pv->pitch:pv->s;
    int per_row=SIZE/pitch, cells=per_row*per_row;
    for(int n=0;n<pv->n;n++) {
        int cell=n%cells;
        float x=(float)((cell%per_row)*pitch), y=(float)((cell/per_row)*pitch);
        v[n*3+0]=(Vertex){x,y,0.5f};
        v[n*3+1]=(Vertex){x+pv->s,y,0.5f};
        v[n*3+2]=(Vertex){x,y+pv->s,0.5f};
    }
}
static bool p2_frame(const P2Var *pv, double *gpu_ms, long *p3d_us, float *cmd_use) {
    vcur=0;
    if(!C3D_FrameBegin(C3D_FRAME_SYNCDRAW)) { error_flag=true; return false; }
    if(!C3D_FrameDrawOn(target)) { error_flag=true; C3D_FrameEnd(0); return false; }
    basic_state();
    C3D_FixedAttribSet(1,1,1,1,1);
    int nv=pv->quads? pv->quads*6 : pv->n*3;
    if(nv) {
        Vertex *v=vtx_alloc(nv);
        if(!v) { C3D_FrameEnd(0); return false; }
        p2_fill(pv,v);
        for(int d=0; d<(pv->draws?pv->draws:1); d++)
            C3D_DrawArrays(GPU_TRIANGLES,(int)(v-vertices),nv);
    }
    *cmd_use=C3D_GetCmdBufUsage();
    GSPGPU_PerfLog p0, p1;
    bool have = R_SUCCEEDED(GSPGPU_GetPerfLog(&p0));
    u64 t0=svcGetSystemTick();
    C3D_FrameEnd(0);
    C3D_FrameSync();
    last_wall_us=(double)(svcGetSystemTick()-t0)*1e6/SYSCLOCK_ARM11;
    have = have && R_SUCCEEDED(GSPGPU_GetPerfLog(&p1));
    *gpu_ms=C3D_GetDrawingTime();
    *p3d_us = have ? (long)(u32)(p1.entries[GSPGPU_EVENT_P3D].totalDurationUs -
                              p0.entries[GSPGPU_EVENT_P3D].totalDurationUs) : -1;
    /* Keep anomalous observations; record raw counters for investigation.
       DrawingTime is an independent, imperfect cross-check, not a GPU oracle. */
    if (have && *p3d_us > 5000 && *p3d_us > *gpu_ms * 1000.0 + 5000.0)
        logmsg(true,"WARN timing mismatch p3d=%ldus drawing=%.3fms total=%lu->%lu last=%lu->%lu",
               *p3d_us,*gpu_ms,
               (unsigned long)p0.entries[GSPGPU_EVENT_P3D].totalDurationUs,
               (unsigned long)p1.entries[GSPGPU_EVENT_P3D].totalDurationUs,
               (unsigned long)p0.entries[GSPGPU_EVENT_P3D].lastDurationUs,
               (unsigned long)p1.entries[GSPGPU_EVENT_P3D].lastDurationUs);
    return true;
}
static void run_perf2(void) {
    static double p3d[P2_MAX][PERF_REPS];
    int n[P2_MAX]={0};
    p2_build();
    /* Measured pixel coverage of ONE triangle per distinct size (cell 0). */
    int seen[P2_MAX], nseen=0;
    for(int i=0;i<p2n;i++) {
        int s=p2vars[i].s; bool dup=(s==0);
        for(int j=0;j<nseen;j++) if(seen[j]==s) dup=true;
        if(dup) continue;
        seen[nseen++]=s;
        P2Var one={{0},s,1,0,0,0};
        if(!begin_case()) return;
        C3D_FixedAttribSet(1,1,1,1,1);
        Vertex *v=vtx_alloc(3); if(!v) return;
        p2_fill(&one,v);
        C3D_DrawArrays(GPU_TRIANGLES,(int)(v-vertices),3);
        GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH,1);
        C3D_FrameEnd(0);
        char var[16]; snprintf(var,sizeof(var),"s%d",s);
        record("perf2_cov",var,s,false,0.0);
        if(error_flag) return;
    }
    bool perflog=R_SUCCEEDED(GSPGPU_SetPerfLogMode(true));
    logmsg(true,"perf2: %d variants x %d reps (+%d warmup), perflog %s",
           p2n,PERF_REPS,PERF_WARMUP,perflog?"on":"UNAVAILABLE");
    for(int rep=-PERF_WARMUP; rep<PERF_REPS; rep++) {
        for(int i=0;i<p2n;i++) {
            int vi=(rep&1)? p2n-1-i : i;
            double g; long p; float cu;
            if(!p2_frame(&p2vars[vi],&g,&p,&cu)) goto out;
            if(cu>0.9f) logmsg(true,"WARN cmdbuf %.0f%% in %s",cu*100.0,p2vars[vi].name);
            if(rep<0) continue;
            log_case("perf2",p2vars[vi].name,rep,0,0,0,g,p);
            logmsg(false,"case %04u perf2     %s %d gpu_ms=%.6f p3d_us=%ld cmdbuf=%.0f%%",
                   case_id,p2vars[vi].name,rep,g,p,cu*100.0);
            case_id++;
            p3d[vi][n[vi]]=(double)p; n[vi]++;
        }
    }
    {
        double base_p=0;
        for(int vi=0;vi<p2n;vi++) {
            double mp=median_of(p3d[vi],n[vi]);
            if(vi==0) base_p=mp;
            logmsg(true,"%-14s p3d=%.0fus (+%.0f)",p2vars[vi].name,mp,mp-base_p);
        }
    }
out:
    GSPGPU_SetPerfLogMode(false);
}
/* ---- timing_iso: isolate the perf2 P3D anomaly --------------------------------
 * Runs 012-014 (v0.5): P3D for s3/s4 n<=256 sat at 3..7 x 16.67 ms (vblank multiples) while
 * C3D_GetDrawingTime was ~0.17 ms; s3_n512 and s8_n256 gave P3D ~55 us with no anomaly. P3D is
 * therefore not a reliable per-draw GPU time. v0.6 adds an independent clock and scaling tests:
 *   timing_iso  rows: p3d_us (perflog) and gpu_queue_ms (C3D_GetDrawingTime), as before
 *   timing_wall rows: same variant/rep, gpu_queue_ms = svcGetSystemTick wall time (ms) of
 *                     FrameEnd..FrameSync; p3d_us repeats the perflog value
 * Variants: 0/1 triangle baselines, count series, and 1/4/16 repeated draws of one batch in the
 * same frame (does time scale with GPU work or stay at frame multiples?).
 *   _blk : all reps of one variant back to back
 *   _gap : an empty frame before every measured frame
 * Not a fix: it only records. */
#define ISO_REPS 20
#define ISO_WARMUP 3
static void run_timing_iso(void) {
    static const struct { const char *name; int s, n, pitch, draws; } iv[] = {
        {"s3_n1",3,1,0,0},{"s3_n16",3,16,0,0},
        {"s3_n64",3,64,0,0},{"s3_n256",3,256,0,0},{"s3_n512",3,512,0,0},
        {"s3_n256_p8",3,256,8,0},{"s8_n256",8,256,0,0},
        {"s3_n256_x4",3,256,0,4},{"s3_n256_x16",3,256,0,16},
        {"s3_n64_x16",3,64,0,16},{"s8_n256_x16",8,256,0,16},
    };
    static const char *modes[2]={"blk","gap"};
    P2Var empty={{"empty"},0,0,0,0,0};
    bool perflog=R_SUCCEEDED(GSPGPU_SetPerfLogMode(true));
    logmsg(true,"timing_iso: %d variants x 2 modes x %d reps, perflog %s",
           (int)(sizeof(iv)/sizeof(iv[0])),ISO_REPS,perflog?"on":"UNAVAILABLE");
    for(unsigned vi=0;vi<sizeof(iv)/sizeof(iv[0]);vi++) {
        P2Var pv={{0},iv[vi].s,iv[vi].n,0,iv[vi].pitch,iv[vi].draws};
        for(int m=0;m<2;m++) {
            char name[24]; snprintf(name,sizeof(name),"%s_%s",iv[vi].name,modes[m]);
            for(int rep=-ISO_WARMUP;rep<ISO_REPS;rep++) {
                double g; long p; float cu;
                if(m==1 && !p2_frame(&empty,&g,&p,&cu)) goto out;
                if(!p2_frame(&pv,&g,&p,&cu)) goto out;
                if(rep<0) continue;
                log_case("timing_iso",name,rep,0,0,0,g,p);
                log_case("timing_wall",name,rep,0,0,0,last_wall_us/1000.0,p);
                logmsg(false,"case %04u timing_iso %s %d gpu_ms=%.6f wall_us=%.1f p3d_us=%ld",
                       case_id,name,rep,g,last_wall_us,p);
                case_id++;
            }
        }
    }
out:
    GSPGPU_SetPerfLogMode(false);
}
static bool initialize(void) {
    target=C3D_RenderTargetCreate(SIZE,SIZE,GPU_RB_RGBA8,GPU_RB_DEPTH24_STENCIL8);
    if(!target) return false;
    dvlb=DVLB_ParseFile((u32*)vshader_shbin,vshader_shbin_size);
    if(!dvlb) return false;
    shaderProgramInit(&program);
    shaderProgramSetVsh(&program,&dvlb->DVLE[0]);
    proj_loc=shaderInstanceGetUniformLocation(program.vertexShader,"projection");
    if(proj_loc<0) return false;
    Mtx_OrthoTilt(&projection,0,SIZE,0,SIZE,0,1,true);
    vertices=linearAlloc(MAX_VERTS*sizeof(Vertex));
    staging=linearAlloc(BYTES);
    if(!vertices || !staging) return false;
    C3D_AttrInfo *ai=C3D_GetAttrInfo();
    AttrInfo_Init(ai);
    AttrInfo_AddLoader(ai,0,GPU_FLOAT,3);
    AttrInfo_AddFixed(ai,1);
    C3D_BufInfo *bi=C3D_GetBufInfo();
    BufInfo_Init(bi);
    BufInfo_Add(bi,vertices,sizeof(Vertex),1,0x0);
    // GPU sees VBO changes on FrameEnd; no unsynchronized changes while in flight.
    return true;
}
static void shutdown_renderer(void) {
    if(vertices) linearFree(vertices);
    if(staging) linearFree(staging);
    if(dvlb) {shaderProgramFree(&program); DVLB_Free(dvlb);}
    if(target) C3D_RenderTargetDelete(target);
}
typedef struct { const char *name; void (*fn)(void); bool needs_cal; } Family;
static void run_raster_edge(void) { run_raster(); if(!error_flag) run_edge(); }

/* Runs every family in a fixed order, timing each one; aborts on error. */
static void run_all(void) {
    static const Family fams[] = {
        {"selftest", run_selftest, false},
        {"raster+edge", run_raster_edge, true},
        {"raster_fine", run_raster_fine, true},
        {"tev", run_tev, true},
        {"blend", run_blend, true},
        {"perf", run_perf, true},
        {"perf2", run_perf2, true},
        {"timing_iso", run_timing_iso, true},
        {"probe", run_readonly_probe, false},
    };
    int ok = 0, n = (int)(sizeof(fams)/sizeof(fams[0]));
    logmsg(true,"=== RUN ALL: %d families ===",n);
    for (int k = 0; k < n; k++) {
        if (fams[k].needs_cal && !calibrated) {
            logmsg(true,"[%d/%d] %-11s SKIP (self-test failed)",k+1,n,fams[k].name);
            continue;
        }
        unsigned first = case_id;
        u64 t = svcGetSystemTick();
        logmsg(true,"[%d/%d] %-11s start",k+1,n,fams[k].name);
        fams[k].fn();
        double sec = (double)(svcGetSystemTick() - t) / SYSCLOCK_ARM11;
        if (error_flag) {
            logmsg(true,"[%d/%d] %-11s ERROR after %u cases, %.1fs",k+1,n,fams[k].name,case_id-first,sec);
            break;
        }
        logmsg(true,"[%d/%d] %-11s done: %u cases, %.1fs",k+1,n,fams[k].name,case_id-first,sec);
        ok++;
        if (zipf) zip_sync();
        if (k == 0 && !calibrated) break;
    }
    logmsg(true,"=== %s: %d/%d families, %u cases ===",
           (ok==n && !error_flag)?"COMPLETE":"INCOMPLETE",ok,n,case_id);
}
static void write_manifest(void) {
    char path[128]; snprintf(path,sizeof(path),"%s/manifest.txt",run_dir);
    FILE *f = fopen(path,"w"); if(!f) return;
    bool n3ds = false; APT_CheckNew3DS(&n3ds);
    fprintf(f,"pica-lab v0.6\nstarted_unix=%lld\nmodel=%s\nfirm_version=%08lX\nshader_fnv1a32=%08lX\nsize=%d\n",
        (long long)time(NULL), n3ds?"New3DS":"Old3DS", (unsigned long)osGetFirmVersion(),
        (unsigned long)hash32((const u8*)vshader_shbin,vshader_shbin_size), SIZE);
    fclose(f);
}
/* Copies the small text outputs into the zip so it is self-contained. */
static void zip_add_file(const char *name) {
    char path[128];
    snprintf(path,sizeof(path),"%s/%s",run_dir,name);
    FILE *f=fopen(path,"rb");
    if(!f) return;
    fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
    u8 *buf=n>0?malloc((size_t)n):NULL;
    if(buf && fread(buf,1,(size_t)n,f)==(size_t)n) zip_add(name,buf,(size_t)n);
    free(buf);
    fclose(f);
}
static void finish_zip(void) {
    if(!zipf) return;
    zip_add_file("manifest.txt");
    zip_add_file("results.csv");
    zip_add_file("run.log");
    zip_add_file("probe.txt");
    zip_sync();
    fclose(zipf);
    zipf=NULL;
}
int main(void) {
    gfxInitDefault();
    consoleInit(GFX_BOTTOM,NULL);
    printf("PICA Lab v0.4 | Original 3DS focus\n");
    printf("Offscreen RGBA8 / controlled 256x256\n");
    if(!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {printf("C3D_Init failed\n");gfxExit();return 1;}
    if(!initialize()) {
        printf("Init failed. Check shader + VRAM.\n");
        shutdown_renderer(); C3D_Fini(); gfxExit();return 1;
    }
    t0_tick = svcGetSystemTick();
    make_run_dir();
    if(!run_dir[0]) {
        printf("Cannot create run dir in " OUTPUT_DIR "\n");
        shutdown_renderer(); C3D_Fini(); gfxExit(); return 1;
    }
    char path[128];
    snprintf(path,sizeof(path),"%s/run.log",run_dir);
    runlog=fopen(path,"w");
    snprintf(path,sizeof(path),"%s/results.csv",run_dir);
    logfile=fopen(path,"w");
    if(!logfile) {
        printf("Cannot create %s\n",path);
        shutdown_renderer(); C3D_Fini(); gfxExit(); return 1;
    }
    fprintf(logfile,"case_id,family,variant,parameter,crc32,nonclear_pixels,center_abgr_u32,gpu_queue_ms,p3d_us\n");
    fflush(logfile);
    write_manifest();
    crc_init();
    {   // zip sits beside the run dir: sdmc:/3ds/pica-lab/run_NNN.zip
        char zpath[128];
        snprintf(zpath,sizeof(zpath),"%s.zip",run_dir);
        const char *base=strrchr(run_dir,'/'); base=base?base+1:run_dir;
        snprintf(zprefix,sizeof(zprefix),"%s",base);
        zipf=fopen(zpath,"wb+");
        if(!zipf) logmsg(true,"WARN cannot create %s",zpath);
        else { zend=0; zip_sync(); }
    }
    logmsg(true,"Output: %s (+ .zip)",run_dir);
    printf("SELECT: run ALL (automatic)\n");
    printf("A: self-test   B: raster + edges\n");
    printf("X: TEV        Y: blending\n");
    printf("L: GPU draw timing  R: ID probe\n");
    printf("DOWN: raster_fine   RIGHT: perf2 (tri size)\n");
    printf("LEFT: timing_iso\n");
    printf("START: exit.\n");
    while(aptMainLoop()) {
        hidScanInput(); u32 down=hidKeysDown();
        if(down & KEY_START) break;
        if(down & KEY_SELECT) { run_all(); if(error_flag) break; gspWaitForVBlank(); continue; }
        if(down & KEY_A) run_selftest();
        if(down & (KEY_B|KEY_X|KEY_Y|KEY_L|KEY_DDOWN|KEY_DRIGHT|KEY_DLEFT)) {
            if(!calibrated) {logmsg(true,"Run A self-test first.");continue;}
        }
        if(down & KEY_B) {run_raster();if(!error_flag)run_edge();}
        if(down & KEY_X) run_tev();
        if(down & KEY_Y) run_blend();
        if(down & KEY_L) run_perf();
        if(down & KEY_DDOWN) run_raster_fine();
        if(down & KEY_DRIGHT) run_perf2();
        if(down & KEY_DLEFT) run_timing_iso();
        if(down & KEY_R) run_readonly_probe();
        if(error_flag) {logmsg(true,"ERROR logged. Abort experiment.");break;}
        gspWaitForVBlank();
    }
    logmsg(false,"exit error_flag=%d",(int)error_flag);
    if(logfile) fclose(logfile);
    if(runlog) fclose(runlog);
    logfile=NULL; runlog=NULL;
    finish_zip();
    // Ensure GPU has completed before freeing GPU-visible buffers.
    C3D_FrameBegin(0); C3D_FrameEnd(0);
    shutdown_renderer(); C3D_Fini(); gfxExit();
    return error_flag?1:0;
}
