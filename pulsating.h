/*
 * pulsating.h — unified header for sinc wave CA + pulsating wavefront CA
 */

#ifndef PULSATING_H_
#define PULSATING_H_

#include <SDL3/SDL.h>
#include <stdint.h>

#define L 221
#define INF_R2 0xFFFFFFFFu
#define GRID_SPACING 8

/* --- Geometry constants (derived from L) --- */
#define RADIUS      (L/2 - 2)
#define DIFF_SHIFT  4
#define SHELL_R     (L * 12 / 100)
#define SHELL_W     (L / 10)
#define CORE_R      3
#define SHELL_TARGET 16384

/* --- Scaled constants (L-invariant behaviour) --- */
/* Calibrated at L=51 so that behaviour is proportional at any L */
#define PULSE_TOLERANCE  ((L * L + 150) / 300)   /* ~1.4% of max_r2          */
#define PULSE_STEP       (((L) + 15) / 30)        /* sweep period ~ linear in L */
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
    unsigned int r2;    /* Euclidean distance² (INF_R2 = unvisited) */
    unsigned int active;/* 1 if |r2 - pulse_r2| <= PULSE_TOLERANCE */
    /* trigger + wavefront coincidence (persistent, recalculated on next sweep) */
    unsigned char ttl;
    unsigned char trig;    /* 1 if Bresenham triggered this tick, else 0 */

} Cell;

/* --- Grid pointers (heap-allocated due to size) --- */
extern Cell (*grid)[L][L];
extern Cell (*grid_next)[L][L];

extern const int MID;
extern const int R_MAX;
extern int tick;

/* --- Unified init (sinc + wavefront) --- */
void init(void);

/* --- Per-tick updates --- */
void sinc_step(void);
void pulse_step(void);
void step_all(void);
unsigned int pulse_from_time(unsigned int t);

/* --- Rendering --- */
void render_frame(SDL_Renderer *ren);

#endif /* PULSATING_H_ */
