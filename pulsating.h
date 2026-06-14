/*
 * pulsating.h — unified header for sinc wave CA + pulsating wavefront CA
 *
 * Shared between CPU (mytry.c) and CUDA (ca_cuda.cu) builds.
 * Define USE_CUDA to enable CUDA wrappers; define NO_SDL when
 * compiling translation units that do not link against SDL.
 */

#ifndef PULSATING_H_
#define PULSATING_H_

/* --- CUDA / MSVC portability macros --- */
#ifdef __CUDACC__
#define HD __host__ __device__
#else
#define HD
#endif

#if defined(_MSC_VER) && !defined(__cplusplus)
#define SINLINE static __inline
#else
#define SINLINE static inline
#endif

/* --- SDL (only needed for host rendering code) --- */
#ifndef NO_SDL
#include <SDL3/SDL.h>
#endif

#include <stdint.h>

/* --- Grid dimensions --- */
#define L 221
#define INF_R2 0xFFFFFFFFu
#define GRID_SPACING 8
#define MID   (L / 2)
#define R_MAX (L / 2)

/* --- Geometry constants (derived from L) --- */
#define RADIUS      (L/2 - 2)
#define DIFF_SHIFT  4
#define SHELL_R     (L * 12 / 100)
#define SHELL_W     (L / 10)
#define CORE_R      3
#define SHELL_TARGET 16384

/* --- Scaled constants (L-invariant behaviour) --- */
#define PULSE_TOLERANCE  ((L * L + 150) / 300)
#define PULSE_STEP       (((L) + 15) / 30)
#define ABSORB_W         ((RADIUS / 27) > 2 ? (RADIUS / 27) : 2)
#define DIFF_DIV_SHIFT   ((RADIUS >= 384) ? 6 : (RADIUS >= 192) ? 5 : \
                          (RADIUS >=  96) ? 4 : (RADIUS >=  40) ? 3 : 2)
#define SMOOTH_W         ((RADIUS / 20) > 1 ? (RADIUS / 20) : 1)
#define YELLOW_VIS_TOL   ((RADIUS / 35) > 1 ? (RADIUS / 35) : 1)
#define TTL_DECAY_MASK   ((RADIUS >= 384) ? 127 : (RADIUS >= 192) ? 63 : \
                          (RADIUS >=  96) ?  31 : (RADIUS >=  40) ? 15 : 7)
#define VEL_DAMP_SHIFT   ((RADIUS >= 384) ? 9 : (RADIUS >= 192) ? 8 : \
                          (RADIUS >=  96) ? 7 : (RADIUS >=  40) ? 6 : 5)

/* --- Display --- */
#define WINDOW_W 1850
#define WINDOW_H 750
#define GRAPH_HEIGHT 400
#define GRAPH_SCALE_X  (700 / (RADIUS > 1 ? RADIUS : 1))

/* --- Stability detection --- */
#define STABILITY_THRESHOLD 35
#define STABILITY_FRAMES    180

/* --- Graph normalization: absolute peak reference (scales with L) ---
 * The shell forcing drives u toward SHELL_TARGET at r=SHELL_R.
 * Spherical focusing amplifies the peak at small r by roughly
 * sqrt(RADIUS/SHELL_R).  This reference keeps the Y-axis stable
 * and consistent across grid sizes.                                  */
#define PROFILE_PEAK_REF  (SHELL_TARGET * 3)

/* --- 3-D flat indexing --- */
#define IDX(x,y,z) ((x)*L*L + (y)*L + (z))

/* --- Unified Cell struct --- */
typedef struct {
    /* sinc wave CA */
    int u;              /* displacement */
    int v;              /* velocity */
    int acc;            /* Bresenham accumulator */
    int sinc_p;         /* emergent sinc numerator */
    int sinc_q;         /* emergent sinc denominator */
    /* shared geometry (filled dynamically by wavefront BFS) */
    int r;              /* integer radius from center */
    unsigned int r2;    /* Euclidean distance-squared (INF_R2 = unvisited) */
    unsigned int active;/* 1 if |r2 - pulse_r2| <= PULSE_TOLERANCE */
    /* trigger + wavefront coincidence */
    unsigned char ttl;
    unsigned char trig; /* 1 if Bresenham triggered this tick, else 0 */
} Cell;

/* --- Grid pointers (heap-allocated due to size) --- */
extern Cell (*grid)[L][L];
extern Cell (*grid_next)[L][L];
extern int tick;

/* ================================================================
 * Shared utility functions (available on both host and device)
 * ================================================================ */

SINLINE HD int isqrt(int n) {
    if (n <= 0) return 0;
    int result = 0;
    int bit = 1 << 30;
    while (bit > n) bit >>= 2;
    while (bit != 0) {
        if (n >= result + bit) {
            n -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return result;
}

SINLINE HD unsigned int pulse_from_time(unsigned int t) {
    const unsigned int min_r2 = 0;
    const unsigned int max_r2 = (unsigned int)((unsigned int)R_MAX * R_MAX * 92 / 100);
    const unsigned int step = PULSE_STEP;
    unsigned int span = max_r2 - min_r2;
    if (span == 0) return min_r2;
    unsigned int period = 2 * span;
    unsigned int phase  = (t * step) % period;
    if (phase < span)
        return min_r2 + phase;
    else
        return max_r2 - (phase - span);
}

/* --- Host-side functions (mytry.c) --- */
void init(void);
void step_all(void);

#ifndef USE_CUDA
void sinc_step(void);
void pulse_step(void);
#endif

#ifndef NO_SDL
void render_frame(SDL_Renderer *ren);
#endif

/* --- CUDA wrappers (ca_cuda.cu) --- */
#ifdef USE_CUDA
#include "ca_cuda.h"
#endif

#endif /* PULSATING_H_ */
