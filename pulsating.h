/*
 * pulsating.h — unified header for sinc wave CA + pulsating wavefront CA
 */

#ifndef PULSATING_H_
#define PULSATING_H_

#include <SDL3/SDL.h>
#include <stdint.h>

#define L 121
#define INF_R2 0xFFFFFFFFu
#define GRID_SPACING 8

/* --- Geometry constants (derived from L) --- */
#define RADIUS      (L/2 - 2)
#define DIFF_SHIFT  4
#define SHELL_R     (L * 12 / 100)
#define SHELL_W     (L / 10)
#define CORE_R      3
#define SHELL_TARGET 16384

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
    /* shared geometry */
    int r;              /* integer radius from center */
    int r2;             /* Euclidean distance squared (true geometry) */
    /* pulsating wavefront CA */
    unsigned int wave_r2;  /* wavefront distance² (INF_R2 = unvisited) */
    /* trigger + wavefront coincidence (persistent, recalculated on next sweep) */
    int fired;          /* 1 = triggered while wavefront shell was present */
} Cell;

/* --- Grid pointers (heap-allocated due to size) --- */
extern Cell (*grid)[L][L];
extern Cell (*grid_next)[L][L];

extern const int MID;
extern const int R_MAX;
extern int tick;

/* --- Sinc wave CA --- */
void sinc_init(void);
void sinc_step(void);

/* --- Pulsating wavefront CA --- */
void pulse_init(void);
void pulse_step(void);
unsigned int pulse_from_time(unsigned int t);

/* --- Rendering --- */
void render_frame(SDL_Renderer *ren);

#endif /* PULSATING_H_ */
