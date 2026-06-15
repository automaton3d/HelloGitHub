/*
 * spiral.h — header for emergent spiral CA on pulsating wavefront
 *
 * The spiral emerges from two purely-additive mechanisms:
 *   1) BFS wavefront propagation (sum-of-odds for r2)
 *   2) CORDIC micro-rotation propagated through BFS z-transitions
 *      + Bresenham accumulator to distribute rotations evenly
 *   3) Bresenham line drawing to mark spin=1 along the spiral ray
 *
 * Runtime operations: addition, subtraction, shift, comparison only.
 * No multiplication, no division, no lookup tables, no floats.
 */

#ifndef SPIRAL_H_
#define SPIRAL_H_

/* --- MSVC portability --- */
#if defined(_MSC_VER) && !defined(__cplusplus)
#define SINLINE static __inline
#else
#define SINLINE static inline
#endif

/* --- SDL (only for host rendering) --- */
#ifndef NO_SDL
#include <SDL3/SDL.h>
#endif

#include <stdint.h>

/* =================================================================
 * Grid dimensions — change L here to scale the entire simulation
 * ================================================================= */
#define L 221

#define INF_R2    0xFFFFFFFFu
#define MID       (L / 2)
#define R_MAX     (L / 2)
#define RADIUS    (L / 2 - 2)

/* --- Pulsating sweep parameters (L-invariant) --- */
#define PULSE_TOLERANCE  ((L * L + 150) / 300)
#define PULSE_STEP       (((L) + 15) / 30)

/* --- CORDIC configuration (scale-invariant) ---
 *
 * CORDIC_SHIFT determines the micro-rotation angle per step:
 *   angle_per_step = arctan(2^{-CORDIC_SHIFT})
 *
 * CORDIC_N = total CORDIC steps for a half turn (pi radians):
 *   CORDIC_N ≈ pi * 2^CORDIC_SHIFT
 *
 * A Bresenham accumulator distributes CORDIC_N rotations over
 * R_MAX z-levels, so the total rotation from pole to equator
 * is always ~pi regardless of L.
 *
 * The shift is chosen so that CORDIC_N >> 10 (enough resolution)
 * and arctan(2^{-k}) is small (good circle approximation).
 */
#if R_MAX >= 2048
#define CORDIC_SHIFT 10
#elif R_MAX >= 1024
#define CORDIC_SHIFT 9
#elif R_MAX >= 512
#define CORDIC_SHIFT 8
#elif R_MAX >= 256
#define CORDIC_SHIFT 7
#elif R_MAX >= 128
#define CORDIC_SHIFT 6
#elif R_MAX >= 64
#define CORDIC_SHIFT 5
#elif R_MAX >= 32
#define CORDIC_SHIFT 4
#else
#define CORDIC_SHIFT 3
#endif

/* pi * 2^k ≈ (314 << k) / 100  (compile-time integer arithmetic) */
#define CORDIC_N  ((314 << CORDIC_SHIFT) / 100)

/* Spiral arm width in cells (constant regardless of L) */
#define SPIRAL_W 2

/* Minimum r2 for spiral marking — cells must be near sphere surface.
 * Only cells with r2 >= SPIRAL_R2_MIN get spin=1.
 * This ensures constant-width helix regardless of z-level. */
#define SPIRAL_R2_MIN  ((RADIUS - SPIRAL_W) * (RADIUS - SPIRAL_W))

/* --- Display --- */
#define WINDOW_W  (L + 500 + 80)
#define WINDOW_H  (L + 280)

/* Yellow ring visual tolerance (thin) */
#define YELLOW_VIS_TOL  ((RADIUS / 35) > 1 ? (RADIUS / 35) : 1)

/* =================================================================
 * Cell struct — minimal for spiral CA
 * ================================================================= */
typedef struct {
    /* BFS wavefront geometry */
    int r;                /* integer radius from center */
    unsigned int r2;      /* Euclidean distance-squared (INF_R2 = unvisited) */
    unsigned char active; /* 1 if on pulsating shell */
    unsigned char spin;   /* 1 if on the spiral arm */
    /* CORDIC spiral state (propagated by BFS) */
    int spiral_x;         /* direction vector x-component */
    int spiral_y;         /* direction vector y-component */
    int spiral_acc;       /* Bresenham accumulator for CORDIC stepping */
} Cell;

/* --- Grid pointers (heap-allocated) --- */
extern Cell (*grid)[L][L];
extern Cell (*grid_next)[L][L];
extern int tick;

/* =================================================================
 * Integer square root (bit-by-bit, no multiplication)
 * ================================================================= */
SINLINE int isqrt(int n) {
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

/* =================================================================
 * Pulsating sweep: triangular wave in r2 space
 * (uses multiplication — host-only, not part of the CA FSM)
 * ================================================================= */
SINLINE unsigned int pulse_from_time(unsigned int t) {
    const unsigned int min_r2 = 0;
    const unsigned int max_r2 = (unsigned int)((unsigned int)R_MAX * R_MAX * 92 / 100);
    const unsigned int step = PULSE_STEP;
    unsigned int span = max_r2 - min_r2;
    if (span == 0) return min_r2;
    unsigned int period = span + span;
    unsigned int phase  = (t * step) % period;
    if (phase < span)
        return min_r2 + phase;
    else
        return max_r2 - (phase - span);
}

/* --- Host-side functions (spiral.c) --- */
void init(void);
void step_all(void);
void pulse_step(void);
void spiral_step(void);

#ifndef NO_SDL
void render_frame(SDL_Renderer *ren);
#endif

#endif /* SPIRAL_H_ */
